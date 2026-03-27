# UART Protocol Specification

Communication between ESP32-CAM and Arduino Pro Mini 3.3V.

## Physical Layer

| Parameter | Value |
|-----------|-------|
| Baud rate | 115200 |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Voltage | 3.3V TTL (direct connection, no level shifter) |

## Connections

```
ESP32-CAM GPIO 12 (TX, Serial2) ──── Arduino Pin 0 (RX)
ESP32-CAM GPIO 14 (RX, Serial2) ──── Arduino Pin 1 (TX)
                  GND ───────────────── GND
```

UART0 (GPIO 1/3) is reserved for USB debug. Motor comm uses Serial2 remapped to GPIO 12 (TX) / 14 (RX).

## Packet Format (8 bytes)

```
Byte:  [0]     [1]    [2]      [3]      [4]      [5]      [6]    [7]
Field: START   CMD    PAY_0    PAY_1    PAY_2    PAY_3    CRC8   END
Value: 0xAA    id     ──── 4 bytes payload ────           crc    0x55
```

- **CRC-8** calculated over bytes 1-5 (CMD + PAYLOAD), polynomial 0x07, init 0x00
- All multi-byte values are **little-endian** (native for both AVR and ESP32)

## Commands (ESP32 -> Arduino)

### 0x01 SET_RPM
Set target RPM for both motors.

| Payload bytes | Type | Description |
|--------------|------|-------------|
| 0-1 | int16_t | Left motor RPM (-130 to +130, signed) |
| 2-3 | int16_t | Right motor RPM (-130 to +130, signed) |

Positive = forward, negative = reverse.

### 0x02 STOP
Emergency stop. All payload bytes ignored. Both motors brake immediately.

### 0x03 PING
Heartbeat. Arduino responds with PONG. Also resets the command timeout timer.

### 0x04 GET_PID
Request current PID parameters. No payload. Arduino responds with five RSP_PID_PARAM packets.

### 0x05 SET_PID
Set one PID parameter.

| Payload bytes | Type | Description |
|--------------|------|-------------|
| 0 | uint8_t | Parameter ID (see PID Parameters table) |
| 1 | uint8_t | Reserved (0) |
| 2-3 | int16_t | Value (fixed-point *100 for Kp/Ki/Kd, raw int for imax/ramp) |

Arduino echoes back RSP_PID_PARAM to confirm.

### 0x06 SAVE_PID
Save current PID parameters to EEPROM. No payload. Arduino responds with PONG.

### 0x07 BRAKE
Active brake: lock wheels by shorting motor windings through H-bridge (IN1=H, IN2=H, ENA=255).
Different from STOP which coasts (power cut, wheels free). No payload.

### 0x08 GET_DEBUG
Toggle extended debug reporting. When enabled, Arduino sends RSP_DEBUG and RSP_ENCODERS alongside STATUS at 10Hz. No payload.

## Responses (Arduino -> ESP32)

### 0x81 STATUS
Sent periodically (10 Hz) with actual measured RPM.

| Payload bytes | Type | Description |
|--------------|------|-------------|
| 0-1 | int16_t | Left motor actual RPM |
| 2-3 | int16_t | Right motor actual RPM |

### 0x82 ERROR
Sent when an error condition is detected.

| Payload bytes | Type | Description |
|--------------|------|-------------|
| 0 | uint8_t | Error code (see below) |
| 1-3 | reserved | |

**Error codes:**
- `0x01` Left motor stalled
- `0x02` Right motor stalled
- `0x03` Both motors stalled
- `0x04` Command timeout (no command for 500ms)
- `0x05` Overcurrent detected
- `0x06` Watchdog reset occurred

### 0x83 PONG
Response to PING. Payload mirrors the PING packet.

### 0x84 PID_PARAM
Single PID parameter value.

| Payload bytes | Type | Description |
|--------------|------|-------------|
| 0 | uint8_t | Parameter ID (see PID Parameters table) |
| 1 | uint8_t | Reserved |
| 2-3 | int16_t | Value |

### 0x85 DEBUG
Extended debug status. Sent when debug mode is enabled (see CMD_GET_DEBUG).

| Payload bytes | Type | Description |
|--------------|------|-------------|
| 0 | int8_t | PWM A / 2 (0..127) |
| 1 | int8_t | PWM B / 2 (0..127) |
| 2 | int8_t | Target RPM A (clamped to -127..127) |
| 3 | int8_t | Target RPM B (clamped to -127..127) |

### 0x86 ENCODERS
Encoder cumulative tick counters (debug only, wrapping int16).

| Payload bytes | Type | Description |
|--------------|------|-------------|
| 0-1 | int16_t | Encoder A total ticks |
| 2-3 | int16_t | Encoder B total ticks |

## PID Parameter IDs

Used in CMD_SET_PID and RSP_PID_PARAM payloads.

| ID | Name | Encoding |
|----|------|----------|
| 0x01 | PID_PARAM_KP | Fixed-point: actual = value / 100.0 |
| 0x02 | PID_PARAM_KI | Fixed-point: actual = value / 100.0 |
| 0x03 | PID_PARAM_KD | Fixed-point: actual = value / 100.0 |
| 0x04 | PID_PARAM_IMAX | Raw integer (anti-windup max) |
| 0x05 | PID_PARAM_RAMP | Raw integer (RPM change per PID cycle) |

## Timing

| Parameter | Value |
|-----------|-------|
| Command rate (ESP32 -> Arduino) | 20 Hz recommended |
| Status report rate (Arduino -> ESP32) | 10 Hz |
| Command timeout | 500 ms (motors ramp to stop) |
| Heartbeat interval | 200 ms (if no SET_RPM being sent) |

## Protocol Definition

Shared header file: `shared/protocol.h`
