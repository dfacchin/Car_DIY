#ifndef ESP32CAM_CONFIG_H
#define ESP32CAM_CONFIG_H

// ESP32-CAM firmware version (auto-incremented by build system)
#define ESP32_FW_VERSION    55

// ============================================================================
// WiFi AP Configuration
// SSID is generated at runtime from MAC: Car_AABBCCDD (last 4 bytes of MAC)
// No password (open network)
// ============================================================================

#define WIFI_AP_SSID_PREFIX "Car_"           // Prefix + last 4 MAC bytes hex
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

// Stream settings (QVGA for low latency control, VGA for higher quality)
#define CAM_FRAME_SIZE  FRAMESIZE_QVGA      // 320x240 — fast for control
#define CAM_JPEG_QUALITY    16              // 0-63, higher = smaller/faster
#define CAM_FB_COUNT    2                   // Frame buffer count (2 for streaming)

// ============================================================================
// UART to Motor Controller (Serial2, remapped to SD card pins)
// GPIO 14 = TX to Arduino RX, GPIO 15 = RX from Arduino TX
// Frees UART0 (GPIO 1/3) for USB debug output
// ============================================================================

#define MOTOR_UART_TX       12              // ESP32 TX -> Arduino RX (SD DATA2 pin)
#define MOTOR_UART_RX       15              // ESP32 RX <- Arduino TX (SD CMD pin)
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

// ============================================================================
// Arduino Pro Mini Programming (STK500v1 via UART)
// ============================================================================

#define PIN_AVR_RESET       13          // GPIO 13 -> Arduino RESET pin (SD DATA3, not strapping)
#define EXPECTED_MOTOR_FW   22           // Expected motor firmware version (match FW_VERSION in motor config.h)
#define MOTOR_FW_HEX_PATH  "/motor_fw.hex"  // LittleFS path to embedded motor firmware
#define AVR_BOOTLOADER_BAUD 57600       // Pro Mini 3.3V 8MHz uses 57600
#define AVR_PAGE_SIZE       128         // ATmega328P page size in bytes
#define AVR_FLASH_SIZE      30720       // 30KB usable flash (2KB bootloader)
#define AVR_RESET_PULSE_MS  50          // RESET low duration
#define AVR_BOOT_DELAY_MS   100         // Wait after reset for bootloader

#endif // ESP32CAM_CONFIG_H
