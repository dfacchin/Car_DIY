#ifndef MOTOR_CONFIG_H
#define MOTOR_CONFIG_H

// Firmware version (auto-incremented by build system)
#define FW_VERSION          22

// ============================================================================
// Pin Definitions - Arduino Pro Mini 3.3V
// ============================================================================

// Motor A (Left)
#define PIN_ENA         6       // PWM speed control
#define PIN_IN1         7       // Direction
#define PIN_IN2         8       // Direction

// Motor B (Right)
#define PIN_ENB         9       // PWM speed control
#define PIN_IN3         10      // Direction
#define PIN_IN4         11      // Direction

// Encoder A (Left) - Hardware interrupts
#define PIN_ENC_A_PHASE_A   2   // INT0
#define PIN_ENC_A_PHASE_B   3   // INT1

// Encoder B (Right) - Pin change interrupts
#define PIN_ENC_B_PHASE_A   4   // PCINT20
#define PIN_ENC_B_PHASE_B   5   // PCINT21

// Status LED
#define PIN_LED         13

// ============================================================================
// Motor Parameters - JGA25-370 130RPM @ 12V
// ============================================================================

#define MOTOR_MAX_RPM       130
#define ENCODER_PPR         11      // Pulses per revolution (per phase)
#define ENCODER_CPR         44      // Counts per rev (4x quadrature decoding)
#define GEAR_RATIO          46.0f   // Gear ratio ~1:46
#define COUNTS_PER_OUTPUT_REV   (ENCODER_CPR * GEAR_RATIO)  // ~2024

// ============================================================================
// PID Parameters
// ============================================================================

#define PID_LOOP_INTERVAL_MS    20      // 50 Hz PID loop
#define PID_KP              2.0f
#define PID_KI              0.5f
#define PID_KD              0.1f
#define PID_INTEGRAL_MAX    200.0f      // Anti-windup clamp
#define PID_OUTPUT_MIN      0
#define PID_OUTPUT_MAX      255         // 8-bit PWM

// ============================================================================
// Safety Parameters
// ============================================================================

#define SAFETY_CMD_TIMEOUT_MS       500     // Stop if no command received
#define SAFETY_WATCHDOG_TIMEOUT_MS  2000    // Hardware watchdog
#define SAFETY_RAMP_MAX_RPM_PER_CYCLE  10  // Max RPM change per PID cycle
#define SAFETY_STALL_PWM_THRESHOLD  200    // PWM value considered "high effort"
#define SAFETY_STALL_RPM_THRESHOLD  5      // RPM below this = not moving
#define SAFETY_STALL_DURATION_MS    1000   // How long stall before error

// ============================================================================
// Communication
// ============================================================================

#define STATUS_REPORT_INTERVAL_MS   100     // 10 Hz status reports

#endif // MOTOR_CONFIG_H
