# Power Budget

## Component Power Requirements

| Component | Supply | Typical | Peak | Notes |
|-----------|--------|---------|------|-------|
| ESP32-CAM (streaming) | 5V | 160 mA | 310 mA | WiFi + camera active |
| Arduino Pro Mini 3.3V | 3.3V (via RAW@5V) | 5 mA | 10 mA | 8 MHz clock |
| L298N logic | 5V | 36 mA | 36 mA | Internal logic |
| Motor A (JGA25-370) | 11.1V | 250 mA | 1.2 A | Stall current |
| Motor B (JGA25-370) | 11.1V | 250 mA | 1.2 A | Stall current |
| Encoders x2 | 3.3V | 20 mA | 20 mA | Hall effect sensors |
| LiPo alarm | 0 mA | 1 mA | 1 mA | Balance connector |

## Totals

| Rail | Typical | Peak |
|------|---------|------|
| 5V logic | 201 mA | 356 mA |
| 11.1V motors | 500 mA | 2.4 A |
| **Total from LiPo** | **~700 mA** | **~2.8 A** |

## Battery Selection

### Recommended: 3S LiPo 11.1V

| Parameter | Value |
|-----------|-------|
| Nominal voltage | 11.1V (3.7V x 3 cells) |
| Full charge | 12.6V (4.2V x 3 cells) |
| Low voltage cutoff | 9.9V (3.3V x 3 cells) |
| Minimum capacity | 1500 mAh |
| Minimum C-rating | 25C (37.5A burst - more than enough) |
| Connector | XT60 recommended |

### Estimated Runtime

| Scenario | Current draw | Runtime (1500mAh) | Runtime (2200mAh) |
|----------|-------------|-------------------|-------------------|
| Idle (streaming, no motors) | 200 mA | ~7.5 hours | ~11 hours |
| Normal driving | 700 mA | ~2 hours | ~3 hours |
| Aggressive driving | 1.5 A | ~1 hour | ~1.5 hours |
| Peak (both motors stalled) | 2.8 A | N/A (stall!) | N/A (stall!) |

## Voltage at Motors

L298N has approximately 2V voltage drop across the H-bridge.

| LiPo state | LiPo voltage | Motor voltage | Motor RPM (approx) |
|------------|-------------|---------------|---------------------|
| Full | 12.6V | 10.6V | ~115 RPM |
| Nominal | 11.1V | 9.1V | ~99 RPM |
| Low | 9.9V | 7.9V | ~86 RPM |

Note: Actual max RPM will be lower than the rated 130 RPM due to L298N voltage drop.

## Safety Recommendations

1. **LiPo alarm** on balance connector - alarm at 3.3V/cell
2. **Fuse** on main power line - 5A blade fuse recommended
3. **Power switch** between LiPo and system
4. **Capacitors** near L298N motor supply (100µF) and logic supply (100µF)
5. **Never discharge below 9.9V** - permanent battery damage
