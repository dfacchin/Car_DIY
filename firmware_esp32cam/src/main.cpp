#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "camera.h"
#include "motor_comm.h"
#include "web_server.h"
#include "avr_flash.h"
#include "protocol.h"

extern void camera_stream_handle();

// Command state (set by web API, sent to Arduino every 50ms)
volatile uint8_t cmd_mode = CMD_RPM;    // 'R' or 'P'
volatile int16_t cmd_left = 0;
volatile int16_t cmd_right = 0;
volatile uint8_t cmd_flags = 0;

// Ping: send version request periodically to track connection
volatile bool ping_enabled = true;

// Bridge mode
volatile bool bridge_mode = false;
static unsigned long bridge_start_time = 0;
#define BRIDGE_TIMEOUT_MS   60000

void bridge_start();
void bridge_stop();
void bridge_loop();

// Auto-flash
static bool motor_fw_up_to_date = false;

// Timing
static unsigned long last_cmd_time = 0;
static unsigned long last_version_time = 0;
#define CMD_SEND_INTERVAL   50      // 20 Hz
#define VERSION_INTERVAL    2000    // Check version every 2s

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("========================================");
    Serial.println("  Car DIY - ESP32-CAM starting...");
    Serial.println("========================================");

    pinMode(PIN_FLASH_LED, OUTPUT);
    digitalWrite(PIN_FLASH_LED, LOW);

    avr_flash_init();
    Serial.printf("[OK] AVR reset pin configured (GPIO %d)\n", PIN_AVR_RESET);

    motor_comm_init();
    Serial.println("[OK] Motor UART initialized");

    Serial.println("[..] Initializing camera...");
    if (!camera_init()) {
        Serial.println("[FAIL] Camera init failed! Restarting in 3s...");
        delay(3000);
        ESP.restart();
    }
    Serial.println("[OK] Camera initialized");

    uint8_t mac[6];
    WiFi.macAddress(mac);
    char ap_ssid[20];
    snprintf(ap_ssid, sizeof(ap_ssid), "%s%02X%02X%02X%02X",
             WIFI_AP_SSID_PREFIX, mac[2], mac[3], mac[4], mac[5]);

    Serial.println("[..] Starting WiFi AP...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, NULL, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONN);

    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[OK] AP: %s  IP: %s\n", ap_ssid, ip.toString().c_str());
    Serial.printf("     Control: http://%s\n", ip.toString().c_str());
    Serial.printf("     Admin:   http://%s/admin.html\n", ip.toString().c_str());
    Serial.printf("     Stream:  http://%s:%d/stream\n", ip.toString().c_str(), STREAM_PORT);

    Serial.println("[..] Starting web server...");
    web_server_init();
    Serial.println("[OK] Web server started");

    Serial.println("[..] Starting camera stream task...");
    camera_stream_start();
    Serial.println("[OK] Camera stream task started");

    // Verify Arduino firmware at boot — only flash if content differs
    Serial.printf("[BOOT] Checking Arduino firmware (stored v%d)...\n", EXPECTED_MOTOR_FW);
    if (avr_flash_if_needed(MOTOR_FW_HEX_PATH)) {
        Serial.println("[BOOT] Arduino firmware OK.");
        motor_fw_up_to_date = true;
    } else {
        Serial.printf("[BOOT] Arduino flash failed: %s\n", avr_flash_last_error().c_str());
    }
    // Re-init motor UART after boot-flash (verify may have reconfigured it)
    motor_comm_init();
    // Wait for Arduino to boot after verify/flash reset
    Serial.println("[BOOT] Waiting for Arduino to boot...");
    delay(1000);

    last_cmd_time = millis();
    last_version_time = millis();

    Serial.println("========================================");
    Serial.println("  READY - All systems go!");
    Serial.printf("  Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("  PSRAM: %d bytes free\n", ESP.getFreePsram());
    Serial.println("========================================");
}

void loop() {
    if (bridge_mode) {
        bridge_loop();
        return;
    }

    web_server_handle();
    camera_stream_handle();

    unsigned long now = millis();

    // Send command every 50ms (20 Hz)
    if ((now - last_cmd_time) >= CMD_SEND_INTERVAL) {
        last_cmd_time = now;
        motor_comm_send(cmd_mode, cmd_flags, cmd_left, cmd_right);
    }

    // Send version request every 2s (for connection monitoring + FW version)
    if (ping_enabled && (now - last_version_time) >= VERSION_INTERVAL) {
        last_version_time = now;
        motor_comm_send(CMD_VERSION, 0, 0, 0);

        // Auto-flash check
        if (!motor_fw_up_to_date && motor_comm_is_connected()) {
            uint16_t running = motor_comm_get_fw_version();
            if (running > 0 && running != EXPECTED_MOTOR_FW) {
                Serial.printf("[AUTO-FLASH] FW v%d != expected v%d\n", running, EXPECTED_MOTOR_FW);
                if (avr_flash_from_file(MOTOR_FW_HEX_PATH)) {
                    Serial.println("[AUTO-FLASH] Success!");
                }
            } else if (running == EXPECTED_MOTOR_FW) {
                motor_fw_up_to_date = true;
            }
        }
    }
}

// ============================================================================
// Serial Bridge Mode
// ============================================================================

void bridge_start() {
    bridge_mode = true;
    bridge_start_time = millis();
    Serial.println("\n[BRIDGE] Entering serial bridge mode (60s timeout)");
    Serial.flush();
    Serial.end();
    Serial2.end();
    Serial.begin(AVR_BOOTLOADER_BAUD);
    Serial2.begin(AVR_BOOTLOADER_BAUD, SERIAL_8N1, MOTOR_UART_RX, MOTOR_UART_TX);
    delay(10);
    pinMode(PIN_AVR_RESET, OUTPUT);
    digitalWrite(PIN_AVR_RESET, LOW);
    delay(AVR_RESET_PULSE_MS);
    pinMode(PIN_AVR_RESET, INPUT);
    delay(10);
    digitalWrite(PIN_FLASH_LED, HIGH);
}

void bridge_stop() {
    bridge_mode = false;
    digitalWrite(PIN_FLASH_LED, LOW);
    Serial.end();
    Serial2.end();
    Serial.begin(115200);
    Serial2.begin(PROTO_BAUD_RATE, SERIAL_8N1, MOTOR_UART_RX, MOTOR_UART_TX);
    delay(10);
    Serial.println("\n[BRIDGE] Bridge mode ended");
}

void bridge_loop() {
    while (Serial.available()) Serial2.write(Serial.read());
    while (Serial2.available()) Serial.write(Serial2.read());
    if ((millis() - bridge_start_time) > BRIDGE_TIMEOUT_MS) bridge_stop();
    web_server_handle();
}
