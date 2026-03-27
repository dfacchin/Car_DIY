#ifndef OTA_H
#define OTA_H

#include <Arduino.h>
#include <WebServer.h>

// Register OTA upload endpoints on the given web server
void ota_init(WebServer *server);

#endif // OTA_H
