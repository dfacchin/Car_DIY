#include "camera.h"
#include "config.h"
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

static WebServer stream_server(STREAM_PORT);

bool camera_init() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CAM_PIN_D0;
    config.pin_d1 = CAM_PIN_D1;
    config.pin_d2 = CAM_PIN_D2;
    config.pin_d3 = CAM_PIN_D3;
    config.pin_d4 = CAM_PIN_D4;
    config.pin_d5 = CAM_PIN_D5;
    config.pin_d6 = CAM_PIN_D6;
    config.pin_d7 = CAM_PIN_D7;
    config.pin_xclk = CAM_PIN_XCLK;
    config.pin_pclk = CAM_PIN_PCLK;
    config.pin_vsync = CAM_PIN_VSYNC;
    config.pin_href = CAM_PIN_HREF;
    config.pin_sccb_sda = CAM_PIN_SIOD;
    config.pin_sccb_scl = CAM_PIN_SIOC;
    config.pin_pwdn = CAM_PIN_PWDN;
    config.pin_reset = CAM_PIN_RESET;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_LATEST;

    // Use PSRAM for frame buffers if available
    if (psramFound()) {
        config.frame_size = CAM_FRAME_SIZE;
        config.jpeg_quality = CAM_JPEG_QUALITY;
        config.fb_count = CAM_FB_COUNT;
        config.fb_location = CAMERA_FB_IN_PSRAM;
    } else {
        config.frame_size = FRAMESIZE_QVGA;    // Fallback to 320x240
        config.jpeg_quality = 15;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: 0x%x\n", err);
        return false;
    }

    // Adjust sensor settings for better streaming
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_framesize(s, CAM_FRAME_SIZE);
        s->set_quality(s, CAM_JPEG_QUALITY);
        s->set_brightness(s, 0);
        s->set_contrast(s, 0);
        s->set_saturation(s, 0);
    }

    return true;
}

// MJPEG stream boundary
#define STREAM_BOUNDARY     "----frame"
#define STREAM_CONTENT_TYPE "multipart/x-mixed-replace;boundary=" STREAM_BOUNDARY
#define STREAM_PART_HEADER  "--" STREAM_BOUNDARY "\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n"

static void handle_stream() {
    WiFiClient client = stream_server.client();

    stream_server.sendContent("HTTP/1.1 200 OK\r\n"
                               "Content-Type: " STREAM_CONTENT_TYPE "\r\n"
                               "Access-Control-Allow-Origin: *\r\n"
                               "\r\n");

    while (client.connected()) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            break;
        }

        char part_header[128];
        int header_len = snprintf(part_header, sizeof(part_header), STREAM_PART_HEADER, fb->len);

        client.write((const uint8_t *)part_header, header_len);
        client.write(fb->buf, fb->len);
        client.write("\r\n", 2);

        esp_camera_fb_return(fb);

        if (!client.connected()) break;

        delay(10);  // ~30fps max, yields to other tasks
    }
}

void camera_stream_start() {
    stream_server.on("/stream", HTTP_GET, handle_stream);
    stream_server.begin();
    Serial.printf("Stream server started on port %d\n", STREAM_PORT);
}

// Must be called from loop() to handle stream clients
void camera_stream_handle() {
    stream_server.handleClient();
}
