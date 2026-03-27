#include <Arduino.h>
#include "config.h"
#include "protocol.h"
#include "encoder.h"
#include "motor_control.h"
#include "pid.h"
#include "safety.h"
#include "comm.h"

// PID controllers for each motor
static pid_controller_t pid_a;
static pid_controller_t pid_b;

// Target RPM (from commands, after ramp limiting)
static int16_t target_rpm_a = 0;
static int16_t target_rpm_b = 0;

// Desired RPM (from latest command, before ramp limiting)
static int16_t desired_rpm_a = 0;
static int16_t desired_rpm_b = 0;

// Measured RPM
static float measured_rpm_a = 0.0f;
static float measured_rpm_b = 0.0f;

// Current PWM outputs (absolute value)
static uint8_t current_pwm_a = 0;
static uint8_t current_pwm_b = 0;

// Timing
static unsigned long last_pid_time = 0;
static unsigned long last_status_time = 0;

// State
static bool motors_enabled = false;

// ============================================================================
// Compute RPM from encoder ticks
// ============================================================================
static float ticks_to_rpm(long ticks, uint16_t dt_ms) {
    // RPM = (ticks / counts_per_output_rev) * (60000 / dt_ms)
    return ((float)ticks / COUNTS_PER_OUTPUT_REV) * (60000.0f / dt_ms);
}

// ============================================================================
// Handle incoming UART commands
// ============================================================================
static void handle_command(const proto_packet_t *pkt) {
    safety_cmd_received();

    switch (pkt->cmd) {
        case CMD_SET_RPM: {
            int16_t left = pkt->payload.rpm.left_rpm;
            int16_t right = pkt->payload.rpm.right_rpm;

            // Clamp to valid range
            desired_rpm_a = constrain(left, -MOTOR_MAX_RPM, MOTOR_MAX_RPM);
            desired_rpm_b = constrain(right, -MOTOR_MAX_RPM, MOTOR_MAX_RPM);
            motors_enabled = true;
            safety_clear_error();
            break;
        }

        case CMD_STOP:
            desired_rpm_a = 0;
            desired_rpm_b = 0;
            target_rpm_a = 0;
            target_rpm_b = 0;
            motors_enabled = false;
            motor_brake();
            pid_reset(&pid_a);
            pid_reset(&pid_b);
            break;

        case CMD_PING:
            comm_send_pong();
            break;
    }
}

// ============================================================================
// PID control loop
// ============================================================================
static void pid_loop() {
    unsigned long now = millis();
    uint16_t dt = now - last_pid_time;
    if (dt < PID_LOOP_INTERVAL_MS) return;
    last_pid_time = now;

    // Read and reset encoder ticks
    long ticks_a = encoder_get_ticks_a();
    long ticks_b = encoder_get_ticks_b();
    encoder_reset_a();
    encoder_reset_b();

    // Convert to RPM
    measured_rpm_a = ticks_to_rpm(ticks_a, dt);
    measured_rpm_b = ticks_to_rpm(ticks_b, dt);

    // Safety check
    if (safety_check(current_pwm_a, current_pwm_b, measured_rpm_a, measured_rpm_b)) {
        // Error condition - stop motors
        motor_brake();
        target_rpm_a = 0;
        target_rpm_b = 0;
        desired_rpm_a = 0;
        desired_rpm_b = 0;
        motors_enabled = false;
        pid_reset(&pid_a);
        pid_reset(&pid_b);
        current_pwm_a = 0;
        current_pwm_b = 0;
        comm_send_error(safety_get_error());
        return;
    }

    if (!motors_enabled) return;

    // Apply ramp limiting
    target_rpm_a = safety_ramp_limit(target_rpm_a, desired_rpm_a);
    target_rpm_b = safety_ramp_limit(target_rpm_b, desired_rpm_b);

    // If target is zero and measured is near zero, just brake
    if (target_rpm_a == 0 && abs((int)measured_rpm_a) < SAFETY_STALL_RPM_THRESHOLD) {
        motor_set_a(0);
        current_pwm_a = 0;
        pid_reset(&pid_a);
    } else {
        // PID computes unsigned effort; we apply sign from target direction
        int16_t effort_a = pid_compute(&pid_a, abs(target_rpm_a), abs(measured_rpm_a), dt);
        int16_t signed_effort_a = (target_rpm_a >= 0) ? effort_a : -effort_a;
        motor_set_a(signed_effort_a);
        current_pwm_a = abs(effort_a);
    }

    if (target_rpm_b == 0 && abs((int)measured_rpm_b) < SAFETY_STALL_RPM_THRESHOLD) {
        motor_set_b(0);
        current_pwm_b = 0;
        pid_reset(&pid_b);
    } else {
        int16_t effort_b = pid_compute(&pid_b, abs(target_rpm_b), abs(measured_rpm_b), dt);
        int16_t signed_effort_b = (target_rpm_b >= 0) ? effort_b : -effort_b;
        motor_set_b(signed_effort_b);
        current_pwm_b = abs(effort_b);
    }
}

// ============================================================================
// Send periodic status reports
// ============================================================================
static void status_report() {
    unsigned long now = millis();
    if ((now - last_status_time) < STATUS_REPORT_INTERVAL_MS) return;
    last_status_time = now;

    comm_send_status((int16_t)measured_rpm_a, (int16_t)measured_rpm_b);
}

// ============================================================================
// Arduino setup/loop
// ============================================================================

void setup() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);    // LED on during init

    comm_init();
    encoder_init();
    motor_init();

    pid_init(&pid_a, PID_KP, PID_KI, PID_KD, PID_INTEGRAL_MAX, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    pid_init(&pid_b, PID_KP, PID_KI, PID_KD, PID_INTEGRAL_MAX, PID_OUTPUT_MIN, PID_OUTPUT_MAX);

    safety_init();

    last_pid_time = millis();
    last_status_time = millis();

    digitalWrite(PIN_LED, LOW);     // LED off = ready
}

void loop() {
    safety_feed_watchdog();

    // Process incoming commands
    proto_packet_t rx_pkt;
    if (comm_receive(&rx_pkt)) {
        handle_command(&rx_pkt);
    }

    // Run PID control loop
    pid_loop();

    // Send status reports
    status_report();
}
