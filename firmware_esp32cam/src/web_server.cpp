#include "web_server.h"
#include "config.h"
#include "motor_comm.h"
#include "ota.h"
#include "avr_flash.h"
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

static WebServer server(WEB_SERVER_PORT);

// Current command state
static int16_t cmd_left_rpm = 0;
static int16_t cmd_right_rpm = 0;
static unsigned long last_cmd_time = 0;

// ============================================================================
// API Endpoints
// ============================================================================

static void handle_control() {
    // POST /api/control?left=<rpm>&right=<rpm>
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
    // POST /api/stop
    cmd_left_rpm = 0;
    cmd_right_rpm = 0;
    motor_comm_stop();
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_status() {
    // GET /api/status
    int16_t left_rpm, right_rpm;
    motor_comm_get_status(&left_rpm, &right_rpm);
    uint8_t error = motor_comm_get_error();

    char json[128];
    snprintf(json, sizeof(json),
        "{\"left_rpm\":%d,\"right_rpm\":%d,\"target_left\":%d,\"target_right\":%d,\"error\":%d}",
        left_rpm, right_rpm, cmd_left_rpm, cmd_right_rpm, error);

    server.send(200, "application/json", json);
}

static void handle_not_found() {
    server.send(404, "text/plain", "Not found");
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
    // Mount LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
    }

    // API endpoints
    server.on("/api/control", HTTP_POST, handle_control);
    server.on("/api/control", HTTP_GET, handle_control);  // Also allow GET for easy testing
    server.on("/api/stop", HTTP_POST, handle_stop);
    server.on("/api/stop", HTTP_GET, handle_stop);
    server.on("/api/status", HTTP_GET, handle_status);

    // OTA and AVR flash endpoints
    ota_init(&server);
    avr_flash_register(&server);

    // Serve static files for everything else
    server.onNotFound([]() {
        if (!serve_file(server.uri())) {
            handle_not_found();
        }
    });

    // CORS headers
    server.enableCORS(true);

    server.begin();
    Serial.printf("Web server started on port %d\n", WEB_SERVER_PORT);
}

void web_server_handle() {
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
