#include "ota.h"
#include "config.h"
#include <Update.h>

static WebServer *_server = nullptr;

static void handle_ota_upload() {
    HTTPUpload &upload = _server->upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("OTA: receiving %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("OTA: success, %u bytes\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}

static void handle_ota_result() {
    if (Update.hasError()) {
        _server->send(500, "application/json", "{\"ok\":false,\"error\":\"OTA failed\"}");
    } else {
        _server->send(200, "application/json", "{\"ok\":true,\"msg\":\"Rebooting...\"}");
        delay(500);
        ESP.restart();
    }
}

void ota_init(WebServer *server) {
    _server = server;

    server->on("/api/ota/esp32", HTTP_POST, handle_ota_result, handle_ota_upload);
}
