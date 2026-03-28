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

// Defined in main.cpp
extern volatile uint8_t cmd_mode;
extern volatile int16_t cmd_left;
extern volatile int16_t cmd_right;
extern volatile uint8_t cmd_flags;
extern volatile bool ping_enabled;
extern volatile bool bridge_mode;
extern void bridge_start();
extern void bridge_stop();

// ============================================================================
// Control API
// ============================================================================

static void handle_control() {
    if (server.hasArg("left") && server.hasArg("right")) {
        cmd_left = constrain(server.arg("left").toInt(), -MAX_RPM, MAX_RPM);
        cmd_right = constrain(server.arg("right").toInt(), -MAX_RPM, MAX_RPM);
        cmd_mode = CMD_RPM;
        server.send(200, "application/json", "{\"ok\":true}");
    } else {
        server.send(400, "application/json", "{\"error\":\"missing left/right\"}");
    }
}

static void handle_stop() {
    cmd_left = 0;
    cmd_right = 0;
    cmd_mode = CMD_RPM;
    motor_comm_send(CMD_STOP, 0, 0, 0);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_brake() {
    cmd_left = 0;
    cmd_right = 0;
    motor_comm_send(CMD_BRAKE, 0, 0, 0);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_status() {
    const proto_response_t *rsp = motor_comm_last_response();
    bool connected = motor_comm_is_connected();

    char json[320];
    snprintf(json, sizeof(json),
        "{\"left_rpm\":%d,\"right_rpm\":%d,"
        "\"cmd_left\":%d,\"cmd_right\":%d,\"cmd_mode\":\"%c\","
        "\"error\":0,\"motor_connected\":%s,"
        "\"esp32_fw\":%u,\"fw_version\":%u,\"expected_fw\":%u,"
        "\"tx_count\":%lu,\"rx_count\":%lu,\"arduino_rx\":%u,"
        "\"uptime\":%lu}",
        rsp ? rsp->actual_left : 0, rsp ? rsp->actual_right : 0,
        (int)cmd_left, (int)cmd_right, (char)cmd_mode,
        connected ? "true" : "false",
        ESP32_FW_VERSION, motor_comm_get_fw_version(), EXPECTED_MOTOR_FW,
        motor_comm_get_tx_count(), motor_comm_get_rx_count(),
        rsp ? rsp->rx_count : 0,
        millis() / 1000);

    server.send(200, "application/json", json);
}

// ============================================================================
// Debug API
// ============================================================================

static void handle_raw_pwm() {
    if (server.hasArg("left") && server.hasArg("right")) {
        cmd_left = constrain(server.arg("left").toInt(), -255, 255);
        cmd_right = constrain(server.arg("right").toInt(), -255, 255);
        cmd_mode = CMD_PWM;
        server.send(200, "application/json", "{\"ok\":true}");
    } else {
        server.send(400, "application/json", "{\"error\":\"missing left/right\"}");
    }
}

static void handle_safety() {
    if (server.hasArg("enabled")) {
        uint8_t en = (server.arg("enabled") == "1" || server.arg("enabled") == "true") ? 1 : 0;
        motor_comm_send(CMD_SAFETY, en, 0, 0);
        char json[48];
        snprintf(json, sizeof(json), "{\"ok\":true,\"safety\":%s}", en ? "true" : "false");
        server.send(200, "application/json", json);
    } else {
        server.send(400, "application/json", "{\"error\":\"missing enabled\"}");
    }
}

static void handle_pins() {
    motor_comm_send(CMD_PINS, 0, 0, 0);
    const proto_response_t *rsp = motor_comm_last_response();
    if (!rsp || rsp->cmd != 'n') {
        server.send(500, "application/json", "{\"error\":\"no response\"}");
        return;
    }

    uint16_t dirs = (uint16_t)rsp->left;
    uint16_t vals = (uint16_t)rsp->right;

    char json[512];
    int pos = 0;
    pos += snprintf(json + pos, sizeof(json) - pos, "{\"pins\":[");
    for (int i = 0; i <= 13; i++) {
        bool is_out = (dirs >> i) & 1;
        bool val = (vals >> i) & 1;
        const char *mode = is_out ? "OUT" : "IN";
        int pwm_val = -1;
        if (is_out && (i == 6 || i == 9)) {
            mode = "PWM";
            // Get PWM values via debug command
        }
        pos += snprintf(json + pos, sizeof(json) - pos,
            "%s{\"p\":%d,\"m\":\"%s\",\"v\":%d,\"pwm\":%d}",
            i > 0 ? "," : "", i, mode, val ? 1 : 0, pwm_val);
    }
    pos += snprintf(json + pos, sizeof(json) - pos, "]}");
    server.send(200, "application/json", json);
}

static void handle_ping_toggle() {
    if (server.hasArg("enabled")) {
        ping_enabled = server.arg("enabled") == "1" || server.arg("enabled") == "true";
    } else {
        ping_enabled = !ping_enabled;
    }
    char json[48];
    snprintf(json, sizeof(json), "{\"ok\":true,\"ping\":%s}", ping_enabled ? "true" : "false");
    server.send(200, "application/json", json);
}

static void handle_debug_cmd() {
    motor_comm_send(CMD_DEBUG, 0, 0, 0);
    const proto_response_t *rsp = motor_comm_last_response();
    if (rsp && rsp->cmd == 'd') {
        char json[64];
        snprintf(json, sizeof(json), "{\"pwm_a\":%d,\"pwm_b\":%d}", (int)rsp->left, (int)rsp->right);
        server.send(200, "application/json", json);
    } else {
        server.send(500, "application/json", "{\"error\":\"no response\"}");
    }
}

// ============================================================================
// PID API
// ============================================================================

static void handle_pid_get() {
    // Get all params one by one
    int16_t kp = 0, ki = 0, kd = 0, imax = 0, ramp = 0;

    motor_comm_send(CMD_GET_PID, PID_PARAM_KP, 0, 0);
    const proto_response_t *r = motor_comm_last_response();
    if (r && r->cmd == 'g') kp = r->left;

    motor_comm_send(CMD_GET_PID, PID_PARAM_KI, 0, 0);
    r = motor_comm_last_response();
    if (r && r->cmd == 'g') ki = r->left;

    motor_comm_send(CMD_GET_PID, PID_PARAM_KD, 0, 0);
    r = motor_comm_last_response();
    if (r && r->cmd == 'g') kd = r->left;

    motor_comm_send(CMD_GET_PID, PID_PARAM_IMAX, 0, 0);
    r = motor_comm_last_response();
    if (r && r->cmd == 'g') imax = r->left;

    motor_comm_send(CMD_GET_PID, PID_PARAM_RAMP, 0, 0);
    r = motor_comm_last_response();
    if (r && r->cmd == 'g') ramp = r->left;

    char json[128];
    snprintf(json, sizeof(json),
        "{\"kp\":%.2f,\"ki\":%.2f,\"kd\":%.2f,\"imax\":%d,\"ramp\":%d,\"valid\":true}",
        kp / 100.0f, ki / 100.0f, kd / 100.0f, imax, ramp);
    server.send(200, "application/json", json);
}

static void handle_pid_set() {
    if (!server.hasArg("param") || !server.hasArg("value")) {
        server.send(400, "application/json", "{\"error\":\"missing param/value\"}");
        return;
    }

    String param = server.arg("param");
    float fval = server.arg("value").toFloat();
    uint8_t pid = 0;
    int16_t wire = 0;

    if (param == "kp")        { pid = PID_PARAM_KP;   wire = (int16_t)(fval * 100); }
    else if (param == "ki")   { pid = PID_PARAM_KI;   wire = (int16_t)(fval * 100); }
    else if (param == "kd")   { pid = PID_PARAM_KD;   wire = (int16_t)(fval * 100); }
    else if (param == "imax") { pid = PID_PARAM_IMAX; wire = (int16_t)fval; }
    else if (param == "ramp") { pid = PID_PARAM_RAMP; wire = (int16_t)fval; }
    else { server.send(400, "application/json", "{\"error\":\"unknown param\"}"); return; }

    motor_comm_send(CMD_SET_PID, pid, wire, 0);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_pid_save() {
    motor_comm_send(CMD_SAVE_PID, 0, 0, 0);
    server.send(200, "application/json", "{\"ok\":true}");
}

// ============================================================================
// Bridge
// ============================================================================

static void handle_bridge_start() {
    bridge_start();
    server.send(200, "application/json", "{\"ok\":true,\"msg\":\"Bridge active 60s\"}");
}

static void handle_bridge_stop() {
    bridge_stop();
    server.send(200, "application/json", "{\"ok\":true}");
}

// ============================================================================
// Captive portal + static files
// ============================================================================

static void redirect_to_root() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
}

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
    String fp = path;
    if (fp.endsWith("/")) fp += "index.html";
    if (LittleFS.exists(fp)) {
        File f = LittleFS.open(fp, "r");
        server.streamFile(f, get_content_type(fp));
        f.close();
        return true;
    }
    return false;
}

// ============================================================================
// Init
// ============================================================================

void web_server_init() {
    if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");

    // Control
    server.on("/api/control", HTTP_POST, handle_control);
    server.on("/api/control", HTTP_GET, handle_control);
    server.on("/api/stop", HTTP_POST, handle_stop);
    server.on("/api/stop", HTTP_GET, handle_stop);
    server.on("/api/brake", HTTP_POST, handle_brake);
    server.on("/api/brake", HTTP_GET, handle_brake);
    server.on("/api/status", HTTP_GET, handle_status);

    // Debug
    server.on("/api/debug/raw_pwm", HTTP_GET, handle_raw_pwm);
    server.on("/api/debug/raw_pwm", HTTP_POST, handle_raw_pwm);
    server.on("/api/debug/safety", HTTP_GET, handle_safety);
    server.on("/api/debug/safety", HTTP_POST, handle_safety);
    server.on("/api/debug/pins", HTTP_GET, handle_pins);
    server.on("/api/debug/ping", HTTP_GET, handle_ping_toggle);
    server.on("/api/debug/ping", HTTP_POST, handle_ping_toggle);
    server.on("/api/debug/cmd", HTTP_GET, handle_debug_cmd);

    // PID
    server.on("/api/pid", HTTP_GET, handle_pid_get);
    server.on("/api/pid/set", HTTP_GET, handle_pid_set);
    server.on("/api/pid/set", HTTP_POST, handle_pid_set);
    server.on("/api/pid/save", HTTP_GET, handle_pid_save);
    server.on("/api/pid/save", HTTP_POST, handle_pid_save);

    // Bridge
    server.on("/api/bridge/start", HTTP_GET, handle_bridge_start);
    server.on("/api/bridge/stop", HTTP_GET, handle_bridge_stop);

    // OTA
    ota_init(&server);
    avr_flash_register(&server);

    // Shortcuts
    server.on("/admin", HTTP_GET, []() {
        server.sendHeader("Location", "/admin.html", true);
        server.send(302, "text/plain", "");
    });

    // Captive portal
    server.on("/generate_204", HTTP_GET, []() { server.send(204); });
    server.on("/gen_204", HTTP_GET, []() { server.send(204); });
    server.on("/hotspot-detect.html", HTTP_GET, []() {
        server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    server.on("/library/test/success.html", HTTP_GET, []() {
        server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    server.on("/ncsi.txt", HTTP_GET, []() { server.send(200, "text/plain", "Microsoft NCSI"); });
    server.on("/connecttest.txt", HTTP_GET, []() { server.send(200, "text/plain", "Microsoft Connect Test"); });
    server.on("/fwlink", HTTP_GET, redirect_to_root);

    server.onNotFound([]() {
        if (!serve_file(server.uri())) redirect_to_root();
    });

    server.enableCORS(true);
    server.begin();
    dns_server.start(53, "*", IPAddress(192, 168, 4, 1));
    Serial.printf("Web server started on port %d\n", WEB_SERVER_PORT);
    Serial.println("DNS captive portal active");
}

void web_server_handle() {
    dns_server.processNextRequest();
    server.handleClient();
}
