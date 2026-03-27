# CAR_DIY - WiFi RC Car with Video Streaming

A DIY remote-controlled car with live video streaming, controlled via WiFi from a smartphone or web browser.

## Architecture

```
┌──────────────┐     WiFi      ┌──────────────┐     UART      ┌──────────────┐
│  Smartphone  │◄────────────►│  ESP32-CAM    │◄────────────►│ Arduino Pro  │
│  or Browser  │   HTTP/WS    │  (Gateway)    │   115200 8N1  │ Mini 3.3V    │
│              │              │  Video Stream │              │ (Motor Ctrl) │
└──────────────┘              └──────────────┘              └──────┬───────┘
                                                                   │
                                                            ┌──────┴───────┐
                                                            │   L298N      │
                                                            │ Motor Driver │
                                                            ├──────┬───────┤
                                                            │      │       │
                                                         Motor A  Motor B
                                                       JGA25-370  JGA25-370
                                                       w/encoder  w/encoder
```

## Project Structure

| Directory | Description |
|-----------|-------------|
| `firmware_esp32cam/` | ESP32-CAM firmware - WiFi AP, video streaming, web UI, UART gateway |
| `firmware_motor/` | Arduino Pro Mini firmware - PID motor control, encoders, safety |
| `app_controller/` | Flutter smartphone app - tilt, joystick, tank controls |
| `shared/` | Shared protocol definitions |
| `docs/` | Hardware documentation, schematics, BOM |

## Hardware

- **ESP32-CAM** (AI-Thinker) - WiFi gateway + camera
- **Arduino Pro Mini 3.3V 8MHz** - Motor controller
- **L298N** dual H-bridge motor driver
- **2x JGA25-370** DC motors with hall effect encoders (130 RPM @ 12V)
- **3S LiPo** battery (11.1V)

## Quick Start

### Build Motor Firmware
```bash
cd firmware_motor
pio run
pio run -t upload
```

### Build ESP32-CAM Firmware
```bash
cd firmware_esp32cam
pio run
pio run -t uploadfs   # Upload web UI
pio run -t upload     # Upload firmware
```

### Smartphone App
```bash
cd app_controller
flutter run
```

## Documentation

- [UART Protocol](docs/uart_protocol.md)
- [Wiring Schematic](docs/wiring_schematic.md)
- [Power Budget](docs/power_budget.md)
- [Bill of Materials](docs/bom.md)
