#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "camera.h"
#include "motor_comm.h"
#include "web_server.h"
#include "avr_flash.h"
#include "protocol.h"

// Forward declaration for stream handler
extern void camera_stream_handle();

static unsigned long last_ping_time = 0;

void setup() {
    // USB debug serial (UART0, GPIO 1/3)
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("========================================");
    Serial.println("  Car DIY - ESP32-CAM starting...");
    Serial.println("========================================");

    // Flash LED off
    pinMode(PIN_FLASH_LED, OUTPUT);
    digitalWrite(PIN_FLASH_LED, LOW);

    // Initialize AVR reset pin (GPIO 2)
    avr_flash_init();
    Serial.println("[OK] AVR reset pin configured (GPIO 2)");

    // Initialize motor UART (Serial2 on GPIO 12/13)
    motor_comm_init();
    Serial.println("[OK] Motor UART initialized");

    // Initialize camera
    Serial.println("[..] Initializing camera...");
    if (!camera_init()) {
        Serial.println("[FAIL] Camera init failed! Restarting in 3s...");
        delay(3000);
        ESP.restart();
    }
    Serial.println("[OK] Camera initialized");

    // Build unique SSID from MAC address (last 4 bytes)
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char ap_ssid[20];
    snprintf(ap_ssid, sizeof(ap_ssid), "%s%02X%02X%02X%02X",
             WIFI_AP_SSID_PREFIX, mac[2], mac[3], mac[4], mac[5]);

    // Start WiFi AP (open, no password)
    Serial.println("[..] Starting WiFi AP...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, NULL, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONN);

    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[OK] AP: %s  IP: %s\n", ap_ssid, ip.toString().c_str());
    Serial.printf("     Control: http://%s\n", ip.toString().c_str());
    Serial.printf("     Admin:   http://%s/admin.html\n", ip.toString().c_str());
    Serial.printf("     Stream:  http://%s:%d/stream\n", ip.toString().c_str(), STREAM_PORT);

    // Start servers
    Serial.println("[..] Starting web server...");
    web_server_init();
    Serial.println("[OK] Web server started");

    Serial.println("[..] Starting camera stream task...");
    camera_stream_start();
    Serial.println("[OK] Camera stream task started");

    // Ready

    last_ping_time = millis();

    Serial.println("========================================");
    Serial.println("  READY - All systems go!");
    Serial.printf("  Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("  PSRAM: %d bytes free\n", ESP.getFreePsram());
    Serial.println("========================================");
}

void loop() {
    // Handle web control requests
    web_server_handle();

    // Handle video stream clients (no-op, runs in FreeRTOS task)
    camera_stream_handle();

    // Process motor status/error responses
    proto_packet_t rx_pkt;
    motor_comm_receive(&rx_pkt);

    // Send heartbeat ping periodically
    unsigned long now = millis();
    if ((now - last_ping_time) > MOTOR_PING_RATE_MS) {
        motor_comm_ping();
        last_ping_time = now;
    }
}
