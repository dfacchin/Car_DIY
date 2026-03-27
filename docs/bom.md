# Bill of Materials

## Electronics

| # | Component | Qty | Notes |
|---|-----------|-----|-------|
| 1 | ESP32-CAM (AI-Thinker) | 1 | With OV2640 camera module |
| 2 | Arduino Pro Mini 3.3V 8MHz | 1 | ATmega328P |
| 3 | L298N dual H-bridge motor driver module | 1 | With onboard 5V regulator |
| 4 | JGA25-370 DC motor with encoder, 130RPM 12V | 2 | 6-pin connector, hall effect encoder |
| 5 | 3S LiPo battery 11.1V 1500-2200mAh 25C+ | 1 | XT60 connector |
| 6 | 3S LiPo balance charger | 1 | |
| 7 | LiPo low-voltage alarm/buzzer | 1 | Plugs into balance connector |
| 8 | 5V 3A BEC (voltage regulator) | 1 | Optional but recommended |
| 9 | FTDI USB-to-serial adapter 3.3V | 1 | For programming both boards |
| 10 | XT60 connector pair | 1 | For battery connection |
| 11 | Power switch (toggle, 5A+) | 1 | Main power on/off |

## Passive Components

| # | Component | Qty | Notes |
|---|-----------|-----|-------|
| 12 | 100µF 25V electrolytic capacitor | 2 | L298N motor supply + 5V rail |
| 13 | 0.1µF ceramic capacitor | 2 | Across motor terminals |
| 14 | 5A blade fuse + holder | 1 | Main power protection |

## Mechanical / Wiring

| # | Component | Qty | Notes |
|---|-----------|-----|-------|
| 15 | RC car chassis / frame | 1 | Fits 2x JGA25-370 motors |
| 16 | Wheels compatible with JGA25-370 shaft | 4 | (2 driven + 2 caster/free) |
| 17 | Jumper wires (male-female) | ~20 | For prototyping connections |
| 18 | Breadboard or perfboard | 1 | For mounting Arduino + connections |
| 19 | Mounting hardware (screws, standoffs, zip ties) | assorted | |
| 20 | 18-20 AWG silicone wire (red + black) | 1m each | Power wiring |
| 21 | Heat shrink tubing | assorted | |

## Optional

| # | Component | Qty | Notes |
|---|-----------|-----|-------|
| 22 | Servo motor (SG90) | 1 | For camera pan/tilt |
| 23 | LED headlights | 2 | Controlled from ESP32 GPIO 4 (flash LED) |
| 24 | Ultrasonic sensor (HC-SR04) | 1 | Obstacle detection (future) |
