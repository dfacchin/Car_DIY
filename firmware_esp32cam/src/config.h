#ifndef ESP32CAM_CONFIG_H
#define ESP32CAM_CONFIG_H

// ============================================================================
// WiFi AP Configuration
// ============================================================================

#define WIFI_AP_SSID        "CarDIY"
#define WIFI_AP_PASSWORD    "cardiy1234"     // Min 8 characters
#define WIFI_AP_CHANNEL     1
#define WIFI_AP_MAX_CONN    2

// ============================================================================
// Camera Configuration
// ============================================================================

// AI-Thinker ESP32-CAM pin mapping
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0      5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

// Stream settings
#define CAM_FRAME_SIZE  FRAMESIZE_VGA       // 640x480
#define CAM_JPEG_QUALITY    12              // 0-63, lower = better quality
#define CAM_FB_COUNT    2                   // Frame buffer count (2 for streaming)

// ============================================================================
// UART to Motor Controller
// ============================================================================

#define MOTOR_UART_NUM      UART_NUM_0      // GPIO 1 (TX), GPIO 3 (RX)
#define MOTOR_CMD_RATE_MS   50              // Send commands at 20 Hz
#define MOTOR_PING_RATE_MS  200             // Heartbeat interval

// ============================================================================
// Web Server
// ============================================================================

#define WEB_SERVER_PORT     80
#define STREAM_PORT         81              // Separate port for MJPEG stream

// ============================================================================
// Flash LED
// ============================================================================

#define PIN_FLASH_LED   4

#endif // ESP32CAM_CONFIG_H
