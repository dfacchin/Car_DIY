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
    // Flash LED off
    pinMode(PIN_FLASH_LED, OUTPUT);
    digitalWrite(PIN_FLASH_LED, LOW);

    // Initialize AVR reset pin
    avr_flash_init();

    // Initialize motor UART
    motor_comm_init();

    // Initialize camera
    if (!camera_init()) {
        Serial.println("Camera init failed! Restarting...");
        delay(1000);
        ESP.restart();
    }
    Serial.println("Camera initialized");

    // Start WiFi AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONN);

    IPAddress ip = WiFi.softAPIP();
    Serial.printf("AP started: %s\n", WIFI_AP_SSID);
    Serial.printf("IP: %s\n", ip.toString().c_str());
    Serial.printf("Control: http://%s\n", ip.toString().c_str());
    Serial.printf("Stream:  http://%s:%d/stream\n", ip.toString().c_str(), STREAM_PORT);

    // Start servers
    web_server_init();
    camera_stream_start();

    // Brief flash to indicate ready
    digitalWrite(PIN_FLASH_LED, HIGH);
    delay(200);
    digitalWrite(PIN_FLASH_LED, LOW);

    last_ping_time = millis();
}

void loop() {
    // Handle web control requests
    web_server_handle();

    // Handle video stream clients
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
