# Wiring Schematic

## System Overview

```
                    ┌─────────────────────────────┐
                    │        3S LiPo 11.1V        │
                    │       (min 1500mAh 25C)      │
                    └──────┬──────────────┬────────┘
                           │              │
                      [LiPo Alarm]        │
                           │              │
                    ┌──────┴──────┐  ┌────┴──────────────────┐
                    │  5V BEC     │  │  L298N Motor Driver    │
                    │ (optional)  │  │  +12V ── LiPo +       │
                    └──────┬──────┘  │  GND  ── LiPo -       │
                           │         │                        │
                      5V rail        │  OUT1 ─┬─ Motor A      │
                           │         │  OUT2 ─┘               │
                    ┌──────┴──────┐  │  OUT3 ─┬─ Motor B      │
                    │ ESP32-CAM   │  │  OUT4 ─┘               │
                    │ (5V pin)    │  └────────────────────────┘
                    └─────────────┘
```

## Detailed Connections

### Power Distribution

```
3S LiPo (+) ──┬── L298N +12V input
               └── 5V BEC input (or use L298N 5V output with jumper)

3S LiPo (-) ──┬── L298N GND
               ├── ESP32-CAM GND
               ├── Arduino Pro Mini GND
               └── 5V BEC GND

5V Rail ───────┬── ESP32-CAM 5V pin
               └── Arduino Pro Mini RAW pin (onboard 3.3V regulator)

IMPORTANT: All GND connections must be tied together!
```

### ESP32-CAM to Arduino Pro Mini (UART + Programming)

```
ESP32-CAM              Arduino Pro Mini 3.3V
─────────              ─────────────────────
GPIO 14 (TX, Serial2) ── Pin 0 (RX)     Motor UART
GPIO 15 (RX, Serial2) ── Pin 1 (TX)     Motor UART
GPIO 2                ── RESET           OTA programming
GND ──────────────────── GND

UART0 (GPIO 1/3) is reserved for USB debug output.
Motor communication uses Serial2 remapped to GPIO 14/15
(normally the SD card CLK/CMD pins, free since SD is unused).

No level shifter needed - both are 3.3V logic.
GPIO 2 → RESET allows the ESP32 to reprogram the Arduino
via the web interface (STK500v1 bootloader protocol).
```

**Note on GPIO 2:** This pin must be LOW or floating during ESP32 boot.
The Arduino RESET pin has a 10K pullup. If you have trouble flashing the
ESP32 via USB, temporarily disconnect the GPIO 2 → RESET wire.

### Arduino Pro Mini to L298N

```
Arduino Pro Mini       L298N
────────────────       ─────
Pin 6  (PWM) ──────── ENA  (Motor A speed)
Pin 7  (OUT) ──────── IN1  (Motor A direction)
Pin 8  (OUT) ──────── IN2  (Motor A direction)
Pin 9  (PWM) ──────── ENB  (Motor B speed)
Pin 10 (OUT) ──────── IN3  (Motor B direction)
Pin 11 (OUT) ──────── IN4  (Motor B direction)
GND ───────────────── GND

Motor direction truth table:
  IN1=H, IN2=L → Forward
  IN1=L, IN2=H → Reverse
  IN1=H, IN2=H → Brake
  IN1=L, IN2=L → Coast (free spin)
```

### Motor Encoders to Arduino Pro Mini

```
Motor A Encoder        Arduino Pro Mini
───────────────        ────────────────
VCC ───────────────── 3.3V
GND ───────────────── GND
Phase A ───────────── Pin 2 (INT0 - hardware interrupt)
Phase B ───────────── Pin 3 (INT1 - hardware interrupt)

Motor B Encoder        Arduino Pro Mini
───────────────        ────────────────
VCC ───────────────── 3.3V
GND ───────────────── GND
Phase A ───────────── Pin 4 (PCINT20 - pin change interrupt)
Phase B ───────────── Pin 5 (PCINT21 - pin change interrupt)
```

### JGA25-370 Motor Connector Pinout (6-pin)

```
Pin 1: Motor +    (to L298N OUT1/OUT3)
Pin 2: Motor -    (to L298N OUT2/OUT4)
Pin 3: Encoder GND
Pin 4: Encoder VCC (3.3V - 5V)
Pin 5: Encoder Phase A
Pin 6: Encoder Phase B
```

### Capacitors (noise filtering)

```
100µF electrolytic ── across L298N +12V and GND (close to terminal)
0.1µF ceramic ─────── across each motor terminal (solder directly on motor)
100µF electrolytic ── across 5V rail and GND (near ESP32-CAM)
```

## Notes

1. **5V BEC vs L298N regulator:** The L298N onboard 5V regulator works but may
   brown out under heavy motor load. A dedicated 5V BEC (3A) from the LiPo is
   more reliable for powering the ESP32-CAM.

2. **LiPo alarm:** Always use a low-voltage alarm on the balance connector.
   Set alarm to 3.3V per cell (9.9V total). Discharging below this damages the battery.

3. **Disconnect UART for programming:** Arduino Pro Mini uses pins 0/1 for both
   UART and USB programming. Disconnect ESP32-CAM GPIO 14/15 wires when uploading
   firmware to the Arduino via FTDI adapter.

4. **ESP32-CAM programming:** Use an FTDI adapter connected to GPIO 1 (TX),
   GPIO 3 (RX), and GND. Pull GPIO 0 to GND during upload, then release for normal boot.
   GPIO 1/3 are the USB debug UART, not the motor UART.

5. **WiFi:** The ESP32 creates an open WiFi AP with SSID `Car_XXXXXXXX`
   (unique per chip, derived from MAC address). No password. IP: 192.168.4.1.
