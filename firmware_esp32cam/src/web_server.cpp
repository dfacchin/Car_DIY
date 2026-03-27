#include "web_server.h"
#include "config.h"
#include "motor_comm.h"
#include "ota.h"
#include "avr_flash.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>

static DNSServer dns_server;

static WebServer server(WEB_SERVER_PORT);

// Current command state
static int16_t cmd_left_rpm = 0;
static int16_t cmd_right_rpm = 0;
static unsigned long last_cmd_time = 0;

// ============================================================================
// API Endpoints - Control
// ============================================================================

static void handle_control() {
    if (server.hasArg("left") && server.hasArg("right")) {
        cmd_left_rpm = constrain(server.arg("left").toInt(), -MAX_RPM, MAX_RPM);
        cmd_right_rpm = constrain(server.arg("right").toInt(), -MAX_RPM, MAX_RPM);
        last_cmd_time = millis();
        motor_comm_set_rpm(cmd_left_rpm, cmd_right_rpm);
        server.send(200, "application/json", "{\"ok\":true}");
    } else {
        server.send(400, "application/json", "{\"error\":\"missing left/right params\"}");
    }
}

static void handle_stop() {
    cmd_left_rpm = 0;
    cmd_right_rpm = 0;
    motor_comm_stop();
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_brake() {
    cmd_left_rpm = 0;
    cmd_right_rpm = 0;
    motor_comm_brake();
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_status() {
    int16_t left_rpm, right_rpm;
    motor_comm_get_status(&left_rpm, &right_rpm);
    uint8_t error = motor_comm_get_error();
    bool connected = motor_comm_is_connected();

    char json[192];
    snprintf(json, sizeof(json),
        "{\"left_rpm\":%d,\"right_rpm\":%d,\"target_left\":%d,\"target_right\":%d,"
        "\"error\":%d,\"motor_connected\":%s}",
        left_rpm, right_rpm, cmd_left_rpm, cmd_right_rpm, error,
        connected ? "true" : "false");

    server.send(200, "application/json", json);
}

// ============================================================================
// API Endpoints - Admin / Debug
// ============================================================================

static void handle_debug_status() {
    // GET /api/debug/status - extended status with debug data
    int16_t left_rpm, right_rpm;
    motor_comm_get_status(&left_rpm, &right_rpm);
    uint8_t error = motor_comm_get_error();

    int8_t pwm_a, pwm_b, target_a, target_b;
    motor_comm_get_debug(&pwm_a, &pwm_b, &target_a, &target_b);

    int16_t enc_a, enc_b;
    motor_comm_get_encoders(&enc_a, &enc_b);

    bool connected = motor_comm_is_connected();
    unsigned long pong_age = motor_comm_last_pong_age();
    uint32_t tx_cnt = motor_comm_get_tx_count();
    uint32_t rx_cnt = motor_comm_get_rx_count();

    char json[448];
    snprintf(json, sizeof(json),
        "{\"left_rpm\":%d,\"right_rpm\":%d,"
        "\"target_left\":%d,\"target_right\":%d,"
        "\"cmd_left\":%d,\"cmd_right\":%d,"
        "\"pwm_a\":%d,\"pwm_b\":%d,"
        "\"enc_a\":%d,\"enc_b\":%d,"
        "\"error\":%d,\"uptime\":%lu,"
        "\"motor_connected\":%s,\"pong_age\":%lu,"
        "\"tx_count\":%lu,\"rx_count\":%lu}",
        left_rpm, right_rpm,
        (int)target_a, (int)target_b,
        cmd_left_rpm, cmd_right_rpm,
        (int)pwm_a * 2, (int)pwm_b * 2,
        enc_a, enc_b,
        error, millis() / 1000,
        connected ? "true" : "false", pong_age,
        tx_cnt, rx_cnt);

    server.send(200, "application/json", json);
}

static void handle_pid_get() {
    // GET /api/pid - request PID params from motor controller
    motor_comm_get_pid();

    // Wait briefly for response (motor controller should reply quickly)
    unsigned long start = millis();
    proto_packet_t rx_pkt;
    while ((millis() - start) < 200) {
        motor_comm_receive(&rx_pkt);
        delay(1);
    }

    const motor_pid_cache_t *pid = motor_comm_get_pid_cache();
    char json[192];
    snprintf(json, sizeof(json),
        "{\"kp\":%.2f,\"ki\":%.2f,\"kd\":%.2f,\"imax\":%d,\"ramp\":%d,\"valid\":%s}",
        pid->kp / 100.0f, pid->ki / 100.0f, pid->kd / 100.0f,
        pid->imax, pid->ramp,
        pid->valid ? "true" : "false");

    server.send(200, "application/json", json);
}

static void handle_pid_set() {
    // POST /api/pid/set?param=<id>&value=<val>
    if (!server.hasArg("param") || !server.hasArg("value")) {
        server.send(400, "application/json", "{\"error\":\"missing param/value\"}");
        return;
    }

    String param = server.arg("param");
    float fval = server.arg("value").toFloat();

    uint8_t param_id = 0;
    int16_t wire_value = 0;

    if (param == "kp")        { param_id = PID_PARAM_KP;   wire_value = (int16_t)(fval * 100); }
    else if (param == "ki")   { param_id = PID_PARAM_KI;   wire_value = (int16_t)(fval * 100); }
    else if (param == "kd")   { param_id = PID_PARAM_KD;   wire_value = (int16_t)(fval * 100); }
    else if (param == "imax") { param_id = PID_PARAM_IMAX; wire_value = (int16_t)fval; }
    else if (param == "ramp") { param_id = PID_PARAM_RAMP; wire_value = (int16_t)fval; }
    else {
        server.send(400, "application/json", "{\"error\":\"unknown param\"}");
        return;
    }

    motor_comm_set_pid(param_id, wire_value);

    char json[64];
    snprintf(json, sizeof(json), "{\"ok\":true,\"param\":\"%s\",\"value\":%.2f}", param.c_str(), fval);
    server.send(200, "application/json", json);
}

static void handle_pid_save() {
    // POST /api/pid/save - save to EEPROM
    motor_comm_save_pid();
    server.send(200, "application/json", "{\"ok\":true,\"msg\":\"PID saved to EEPROM\"}");
}

static void handle_debug_toggle() {
    // POST /api/debug/toggle - toggle debug mode on motor controller
    motor_comm_toggle_debug();
    server.send(200, "application/json", "{\"ok\":true}");
}

static void redirect_to_root() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
}

// ============================================================================
// Static file serving from LittleFS
// ============================================================================

static String get_content_type(const String &path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css"))  return "text/css";
    if (path.endsWith(".js"))   return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".png"))  return "image/png";
    if (path.endsWith(".ico"))  return "image/x-icon";
    return "text/plain";
}

static bool serve_file(const String &path) {
    String file_path = path;
    if (file_path.endsWith("/")) file_path += "index.html";

    if (LittleFS.exists(file_path)) {
        File file = LittleFS.open(file_path, "r");
        server.streamFile(file, get_content_type(file_path));
        file.close();
        return true;
    }
    return false;
}

// ============================================================================
// Public API
// ============================================================================

void web_server_init() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
    }

    // Control API
    server.on("/api/control", HTTP_POST, handle_control);
    server.on("/api/control", HTTP_GET, handle_control);
    server.on("/api/stop", HTTP_POST, handle_stop);
    server.on("/api/stop", HTTP_GET, handle_stop);
    server.on("/api/brake", HTTP_POST, handle_brake);
    server.on("/api/brake", HTTP_GET, handle_brake);
    server.on("/api/status", HTTP_GET, handle_status);

    // Admin/Debug API
    server.on("/api/debug/status", HTTP_GET, handle_debug_status);
    server.on("/api/debug/toggle", HTTP_POST, handle_debug_toggle);
    server.on("/api/debug/toggle", HTTP_GET, handle_debug_toggle);
    server.on("/api/pid", HTTP_GET, handle_pid_get);
    server.on("/api/pid/set", HTTP_POST, handle_pid_set);
    server.on("/api/pid/set", HTTP_GET, handle_pid_set);
    server.on("/api/pid/save", HTTP_POST, handle_pid_save);
    server.on("/api/pid/save", HTTP_GET, handle_pid_save);

    // OTA and AVR flash endpoints
    ota_init(&server);
    avr_flash_register(&server);

    // Shortcut: /admin -> /admin.html
    server.on("/admin", HTTP_GET, []() {
        server.sendHeader("Location", "/admin.html", true);
        server.send(302, "text/plain", "");
    });

    // Captive portal detection: return expected responses so OS marks
    // network as "connected, no internet" instead of trapping in portal popup.
    // Then the user can open the regular browser normally.
    server.on("/generate_204", HTTP_GET, []() { server.send(204); });  // Android
    server.on("/gen_204", HTTP_GET, []() { server.send(204); });       // Android
    server.on("/hotspot-detect.html", HTTP_GET, []() {                 // iOS
        server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    server.on("/library/test/success.html", HTTP_GET, []() {          // iOS
        server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    server.on("/ncsi.txt", HTTP_GET, []() {                            // Windows
        server.send(200, "text/plain", "Microsoft NCSI");
    });
    server.on("/connecttest.txt", HTTP_GET, []() {                     // Windows
        server.send(200, "text/plain", "Microsoft Connect Test");
    });
    server.on("/fwlink", HTTP_GET, redirect_to_root);                  // Windows fallback

    // Serve static files, redirect everything else to root
    server.onNotFound([]() {
        String uri = server.uri();
        // Serve known static files (css, js, html, images)
        if (serve_file(uri)) return;
        // Everything else -> redirect to main page
        redirect_to_root();
    });

    server.enableCORS(true);
    server.begin();

    // DNS server: resolve ALL domains to 192.168.4.1 (captive portal)
    dns_server.start(53, "*", IPAddress(192, 168, 4, 1));

    Serial.printf("Web server started on port %d\n", WEB_SERVER_PORT);
    Serial.println("DNS captive portal active");
}

void web_server_handle() {
    dns_server.processNextRequest();
    server.handleClient();

    // Resend last command periodically to keep motor controller alive
    if (last_cmd_time > 0 && (cmd_left_rpm != 0 || cmd_right_rpm != 0)) {
        unsigned long now = millis();
        if ((now - last_cmd_time) > MOTOR_CMD_RATE_MS) {
            motor_comm_set_rpm(cmd_left_rpm, cmd_right_rpm);
            last_cmd_time = now;
        }
    }
}
