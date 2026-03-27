#include "camera.h"
#include "config.h"
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClient.h>

// Use raw WiFiServer instead of WebServer for streaming —
// this runs in its own FreeRTOS task so it won't block the main loop.
static WiFiServer stream_server(STREAM_PORT);
static TaskHandle_t stream_task_handle = NULL;

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

    if (psramFound()) {
        config.frame_size = CAM_FRAME_SIZE;
        config.jpeg_quality = CAM_JPEG_QUALITY;
        config.fb_count = CAM_FB_COUNT;
        config.fb_location = CAMERA_FB_IN_PSRAM;
    } else {
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 15;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: 0x%x\n", err);
        return false;
    }

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

static void stream_one_client(WiFiClient &client) {
    client.print("HTTP/1.1 200 OK\r\n"
                 "Content-Type: " STREAM_CONTENT_TYPE "\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Connection: close\r\n"
                 "\r\n");

    while (client.connected()) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) break;

        char part_header[128];
        int header_len = snprintf(part_header, sizeof(part_header), STREAM_PART_HEADER, fb->len);

        if (client.write((const uint8_t *)part_header, header_len) == 0) {
            esp_camera_fb_return(fb);
            break;
        }
        if (client.write(fb->buf, fb->len) == 0) {
            esp_camera_fb_return(fb);
            break;
        }
        client.write("\r\n", 2);

        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(10));  // ~30fps max, yield to other tasks
    }
    client.stop();
}

// FreeRTOS task that runs independently on core 0
static void stream_task(void *param) {
    stream_server.begin();
    Serial.printf("Stream server started on port %d (task on core %d)\n",
                  STREAM_PORT, xPortGetCoreID());

    for (;;) {
        WiFiClient client = stream_server.available();
        if (client) {
            stream_one_client(client);
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void camera_stream_start() {
    // Run stream server on core 0 (main loop runs on core 1)
    xTaskCreatePinnedToCore(
        stream_task,
        "stream",
        4096,           // Stack size
        NULL,
        1,              // Priority (low)
        &stream_task_handle,
        0               // Core 0
    );
}

// No longer needed in main loop — stream runs in its own task
void camera_stream_handle() {
    // No-op: stream is handled by FreeRTOS task
}
