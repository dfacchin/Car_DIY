#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>

// Initialize web server with control endpoints and static files from LittleFS
void web_server_init();

// Handle web server requests (call from loop)
void web_server_handle();

#endif // WEB_SERVER_H
