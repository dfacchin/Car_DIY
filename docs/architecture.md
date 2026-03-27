# System Architecture

## Overview

```
┌──────────────────────────────────────────────────────────┐
│  Phone/Tablet (WiFi client)                              │
│  Browser → http://192.168.4.1                            │
│  - Drive controls (throttle + steering + brake)          │
│  - Tank controls (dual sliders)                          │
│  - Admin page (PID tuning, debug, firmware upload)       │
│  - MJPEG video stream from port 81                       │
└──────────────┬───────────────────────────────────────────┘
               │ WiFi (open AP, Car_XXXX)
┌──────────────┴───────────────────────────────────────────┐
│  ESP32-CAM (AI-Thinker)                                  │
│                                                          │
│  Core 1 (main loop):                                     │
│    - WebServer on port 80 (control API + static files)   │
│    - DNSServer on port 53 (captive portal)               │
│    - Motor UART send/receive (Serial2, GPIO 14/15)       │
│    - Heartbeat ping every 200ms                          │
│                                                          │
│  Core 0 (FreeRTOS task):                                 │
│    - WiFiServer on port 81 (MJPEG stream)                │
│    - Camera frame capture + stream per client             │
└──────────────┬───────────────────────────────────────────┘
               │ UART 115200 8N1 (GPIO 12 TX, GPIO 14 RX)
┌──────────────┴───────────────────────────────────────────┐
│  Arduino Pro Mini 3.3V 8MHz                              │
│                                                          │
│  Main loop (50Hz PID):                                   │
│    - UART command receive                                │
│    - Quadrature encoder reading (4x decoding)            │
│    - PID control → PWM output                            │
│    - Safety (timeout, stall detect, ramp limiting)       │
│    - Status reporting (10Hz)                             │
│                                                          │
│  EEPROM: PID parameters (Kp, Ki, Kd, Imax, Ramp)       │
└──────────────┬───────────────────────────────────────────┘
               │ PWM + direction pins
┌──────────────┴───────────────────────────────────────────┐
│  L298N H-Bridge → 2x JGA25-370 DC motors with encoders  │
└──────────────────────────────────────────────────────────┘
```

## Boot Sequence (ESP32-CAM)

From `firmware_esp32cam/src/main.cpp`:

1. Init USB debug serial (UART0, 115200)
2. Configure flash LED (GPIO 4)
3. Configure AVR reset pin (GPIO 2)
4. Init motor UART (Serial2 on GPIO 12 TX / 14 RX, 115200)
5. Init camera (OV2640, QVGA 320x240, JPEG quality 16)
6. Build unique SSID from MAC: `Car_XXXX`
7. Start WiFi AP (open, no password)
8. Start web server (port 80) + DNS captive portal (port 53)
9. Start camera stream FreeRTOS task (port 81, core 0)
10. Flash LED briefly → ready

## Data Flow

### Command path (user → motors)
```
Browser JS → POST /api/control?left=X&right=Y
  → web_server.cpp: motor_comm_set_rpm(left, right)
    → Serial2: [0xAA][CMD_SET_RPM][left_lo][left_hi][right_lo][right_hi][CRC][0x55]
      → Arduino: PID controller computes PWM
        → L298N: motor spins at target RPM
```

### Status path (motors → user)
```
Arduino: 10Hz status report via UART
  → Serial2: [0xAA][RSP_STATUS][left_rpm][right_rpm][CRC][0x55]
    → motor_comm_receive(): caches values
      → GET /api/status → JSON response
        → Browser JS: updates display
```

### Heartbeat / connection monitoring
```
ESP32 → CMD_PING every 200ms
Arduino → RSP_PONG
  → motor_comm_receive(): updates last_pong_time
    → motor_comm_is_connected(): true if any response within 1s
      → /api/status → motor_connected: true/false
```

## Motor Control Modes

| Action | Protocol Command | Motor Behavior |
|--------|-----------------|----------------|
| Drive | CMD_SET_RPM (0x01) | PID controls speed to target RPM |
| Stop (coast) | CMD_STOP (0x02) | Power cut, wheels free to spin |
| Brake (lock) | CMD_BRAKE (0x07) | H-bridge short, wheels locked |
| Timeout | (no cmd for 500ms) | Auto-coast, error reported |

## OTA Updates

### ESP32-CAM self-update
Upload `.bin` via `/api/ota/esp32`. Uses ESP32 `Update` library. Reboots on success.

### Arduino Pro Mini remote flash
Upload `.hex` via `/api/ota/avr`. ESP32 speaks STK500v1 bootloader protocol:
1. Temporarily switches motor UART to bootloader baud (57600)
2. Pulls GPIO 2 LOW to reset Arduino into bootloader
3. Sends Intel HEX pages via STK500v1 protocol
4. Restores motor UART to 115200

Requires GPIO 2 wired to Arduino RESET pin.

## Safety Features (Motor Firmware)

| Feature | Threshold | Action |
|---------|-----------|--------|
| Command timeout | 500ms no command | Coast to stop, report PROTO_ERR_TIMEOUT |
| Stall detection | PWM > 200 and RPM < 5 for 1s | Cut power, report stall error |
| Ramp limiting | Configurable RPM/cycle | Smooth acceleration/deceleration |
| Anti-windup | Configurable integral max | PID integral term clamped |
| Hardware watchdog | 2s timeout | MCU reset if firmware hangs |
