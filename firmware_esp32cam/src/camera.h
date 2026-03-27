#ifndef CAMERA_H
#define CAMERA_H

#include <Arduino.h>

// Initialize camera with config settings. Returns true on success.
bool camera_init();

// Start MJPEG streaming HTTP server on STREAM_PORT
void camera_stream_start();

#endif // CAMERA_H
