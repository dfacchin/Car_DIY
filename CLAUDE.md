# CAR_DIY - Project Instructions

## Documentation Sync Rules

This project maintains documentation that MUST stay synchronized with code.
When editing any authoritative source file, update the corresponding docs before completing the task.

### Source-of-Truth Mapping

| Authoritative Source | Documentation to Update |
|---------------------|------------------------|
| `shared/protocol.h` | `docs/uart_protocol.md` |
| `firmware_esp32cam/src/config.h` | `docs/wiring_schematic.md`, `docs/power_budget.md` |
| `firmware_motor/src/config.h` | `docs/wiring_schematic.md`, `docs/power_budget.md` |
| `firmware_esp32cam/src/web_server.cpp` | `docs/api_reference.md` |
| `firmware_esp32cam/src/ota.cpp` | `docs/api_reference.md` |
| `firmware_esp32cam/src/avr_flash.cpp` | `docs/api_reference.md` |
| `firmware_esp32cam/src/main.cpp` | `docs/architecture.md` |
| `firmware_esp32cam/src/camera.cpp` | `docs/architecture.md` |

### Doc Update Checklist

When modifying code, check:
1. Pin assignments changed? -> Update `docs/wiring_schematic.md`
2. Protocol commands/responses added or changed? -> Update `docs/uart_protocol.md`
3. API endpoints added or changed? -> Update `docs/api_reference.md`
4. New components or power changes? -> Update `docs/power_budget.md` and `docs/bom.md`
5. Boot sequence or task architecture changed? -> Update `docs/architecture.md`

## Project Conventions

- Firmware uses PlatformIO (`pio run`, `pio run --target upload`, `pio run --target uploadfs`)
- Shared protocol header: `shared/protocol.h` (included by both firmware projects via `-I../shared`)
- ESP32-CAM web UI files: `firmware_esp32cam/data/` (LittleFS, upload with `--target uploadfs`)
- Flutter app: `app_controller/`
- All multi-byte protocol values are little-endian
- PID values use fixed-point encoding (value * 100) over the wire for Kp/Ki/Kd
- Motor UART: ESP32 Serial2 on GPIO 12 (TX) / GPIO 14 (RX). UART0 (GPIO 1/3) is for USB debug.
- AVR RESET: GPIO 13 (not a strapping pin, uses INPUT mode normally).
- Flash LED: GPIO 4.
- No wires need disconnecting for ESP32 USB flashing.
- WiFi AP: open network, unique SSID `Car_XXXX` generated from MAC address

## Build & Upload

```bash
# ESP32-CAM
cd firmware_esp32cam
pio run --target upload      # firmware
pio run --target uploadfs    # web files (LittleFS)

# Arduino Pro Mini
cd firmware_motor
pio run --target upload
```
