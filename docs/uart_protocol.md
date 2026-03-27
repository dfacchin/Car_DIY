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
ESP32-CAM GPIO 1 (TX) ──── Arduino Pin 0 (RX)
ESP32-CAM GPIO 3 (RX) ──── Arduino Pin 1 (TX)
         GND ──────────────── GND
```

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

## Timing

| Parameter | Value |
|-----------|-------|
| Command rate (ESP32 -> Arduino) | 20 Hz recommended |
| Status report rate (Arduino -> ESP32) | 10 Hz |
| Command timeout | 500 ms (motors ramp to stop) |
| Heartbeat interval | 200 ms (if no SET_RPM being sent) |

## Protocol Definition

Shared header file: `shared/protocol.h`
