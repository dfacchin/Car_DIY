# HTTP API Reference

ESP32-CAM web server on `http://192.168.4.1` (port 80).
MJPEG video stream on port 81.

## WiFi Connection

- SSID: `Car_XXXXXXXX` (unique per device, derived from MAC address)
- Password: none (open network)
- IP: `192.168.4.1`
- Max clients: 2

## Control Endpoints

| Method | Path | Parameters | Description |
|--------|------|-----------|-------------|
| POST/GET | `/api/control` | `left`, `right` (int, -130..130) | Set motor RPM |
| POST/GET | `/api/stop` | none | Emergency stop (coast, wheels free) |
| POST/GET | `/api/brake` | none | Active brake (lock wheels via H-bridge short) |
| GET | `/api/status` | none | Get motor status JSON |

### GET /api/status Response

```json
{
  "left_rpm": 0,
  "right_rpm": 0,
  "target_left": 0,
  "target_right": 0,
  "error": 0,
  "motor_connected": true
}
```

## Debug/Admin Endpoints

| Method | Path | Parameters | Description |
|--------|------|-----------|-------------|
| GET | `/api/debug/status` | none | Extended status with PWM, encoders, uptime, connection |
| POST/GET | `/api/debug/toggle` | none | Toggle debug stream from motor controller |
| GET | `/api/pid` | none | Read PID parameters (waits 200ms for motor response) |
| POST/GET | `/api/pid/set` | `param` (kp/ki/kd/imax/ramp), `value` (float) | Set one PID parameter |
| POST/GET | `/api/pid/save` | none | Save PID to motor controller EEPROM |

### GET /api/debug/status Response

```json
{
  "left_rpm": 0,
  "right_rpm": 0,
  "target_left": 0,
  "target_right": 0,
  "cmd_left": 0,
  "cmd_right": 0,
  "pwm_a": 0,
  "pwm_b": 0,
  "enc_a": 0,
  "enc_b": 0,
  "error": 0,
  "uptime": 123,
  "motor_connected": true,
  "pong_age": 152
}
```

### GET /api/pid Response

```json
{
  "kp": 2.00,
  "ki": 0.50,
  "kd": 0.10,
  "imax": 200,
  "ramp": 10,
  "valid": true
}
```

## OTA/Firmware Endpoints

| Method | Path | Body | Description |
|--------|------|------|-------------|
| POST | `/api/ota/esp32` | multipart file (.bin) | Flash ESP32-CAM firmware. Reboots on success. |
| POST | `/api/ota/avr` | multipart file (.hex) | Flash Arduino Pro Mini via STK500v1 over UART. |

## Video Stream

| Port | Path | Description |
|------|------|-------------|
| 81 | `/stream` | MJPEG stream (320x240 QVGA, ~30fps) |

Stream runs in a dedicated FreeRTOS task on core 0. Access via `http://192.168.4.1:81/stream`.

## Web Pages

| Path | Description |
|------|-------------|
| `/` | Main control page (Drive + Tank modes) |
| `/admin` | Redirects to `/admin.html` |
| `/admin.html` | Admin page (debug, PID tuning, test commands, firmware upload) |

## Captive Portal

The ESP32 runs a DNS server that resolves all domains to `192.168.4.1`.
OS-specific captive portal probes return expected responses to avoid popup browsers:

| Path | OS | Response |
|------|----|----------|
| `/generate_204`, `/gen_204` | Android | HTTP 204 |
| `/hotspot-detect.html`, `/library/test/success.html` | iOS | "Success" HTML |
| `/ncsi.txt`, `/connecttest.txt` | Windows | Expected text |

All other unknown paths redirect to `/`.
