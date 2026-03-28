#include <Arduino.h>
#include <avr/wdt.h>
#include "config.h"
#include "protocol.h"
#include "encoder.h"
#include "motor_control.h"
#include "pid.h"
#include "safety.h"
#include "comm.h"
#include "pid_store.h"

// PID controllers
static pid_controller_t pid_a;
static pid_controller_t pid_b;
static pid_params_t pid_params;

// Target RPM (after ramp limiting)
static int16_t target_rpm_a = 0;
static int16_t target_rpm_b = 0;

// Desired RPM (from latest command, before ramp)
static int16_t desired_rpm_a = 0;
static int16_t desired_rpm_b = 0;

// Measured RPM
static float measured_rpm_a = 0.0f;
static float measured_rpm_b = 0.0f;

// Current PWM outputs
static uint8_t current_pwm_a = 0;
static uint8_t current_pwm_b = 0;

// Timing
static unsigned long last_pid_time = 0;
static unsigned long last_rx_time = 0;
static unsigned long led_timer = 0;
static uint8_t led_phase = 0;

// State
static bool motors_enabled = false;
static bool safety_enabled = true;
static bool raw_pwm_mode = false;
static bool error_reported = false;

// ============================================================================
// Compute RPM from encoder ticks
// ============================================================================
static float ticks_to_rpm(long ticks, uint16_t dt_ms) {
    return ((float)ticks / COUNTS_PER_OUTPUT_REV) * (60000.0f / dt_ms);
}

// ============================================================================
// Motor mapping: logical left/right -> physical motor A/B
// ============================================================================
static void motor_set_left(int16_t power) {
#if MOTOR_SWAP
    motor_set_b(power * MOTOR_LEFT_INVERT);
#else
    motor_set_a(power * MOTOR_LEFT_INVERT);
#endif
}

static void motor_set_right(int16_t power) {
#if MOTOR_SWAP
    motor_set_a(power * MOTOR_RIGHT_INVERT);
#else
    motor_set_b(power * MOTOR_RIGHT_INVERT);
#endif
}

static float get_measured_left() {
#if MOTOR_SWAP
    return measured_rpm_b * MOTOR_LEFT_INVERT;
#else
    return measured_rpm_a * MOTOR_LEFT_INVERT;
#endif
}

static float get_measured_right() {
#if MOTOR_SWAP
    return measured_rpm_a * MOTOR_RIGHT_INVERT;
#else
    return measured_rpm_b * MOTOR_RIGHT_INVERT;
#endif
}

// ============================================================================
// Helper: send standard response (uses mapped RPM)
// ============================================================================
static void send_response(uint8_t cmd, uint8_t flags, int16_t left, int16_t right) {
    proto_response_t rsp;
    proto_build_response(&rsp, cmd, flags, left, right,
                         (int16_t)measured_rpm_a, (int16_t)measured_rpm_b,
                         comm_get_rx_count());
    comm_send_response(&rsp);
}

// ============================================================================
// Handle incoming request — always sends exactly 1 response
// ============================================================================
static void handle_request(const proto_request_t *req) {
    safety_cmd_received();
    last_rx_time = millis();
    error_reported = false;

    switch (req->cmd) {
        case CMD_RPM: {
            int16_t left = constrain(req->left, -MAX_RPM, MAX_RPM);
            int16_t right = constrain(req->right, -MAX_RPM, MAX_RPM);
            desired_rpm_a = left;
            desired_rpm_b = right;
            motors_enabled = true;
            raw_pwm_mode = false;
            safety_clear_error();
            send_response('r', req->flags, left, right);
            break;
        }

        case CMD_PWM: {
            int16_t left = constrain(req->left, -255, 255);
            int16_t right = constrain(req->right, -255, 255);
            raw_pwm_mode = true;
            motors_enabled = false;
            motor_set_left(left);
            motor_set_right(right);
            current_pwm_a = abs(left);
            current_pwm_b = abs(right);
            send_response('p', req->flags, left, right);
            break;
        }

        case CMD_STOP:
            desired_rpm_a = 0;
            desired_rpm_b = 0;
            target_rpm_a = 0;
            target_rpm_b = 0;
            motors_enabled = false;
            raw_pwm_mode = false;
            motor_coast();
            pid_reset(&pid_a);
            pid_reset(&pid_b);
            current_pwm_a = 0;
            current_pwm_b = 0;
            send_response('s', 0, 0, 0);
            break;

        case CMD_BRAKE:
            desired_rpm_a = 0;
            desired_rpm_b = 0;
            target_rpm_a = 0;
            target_rpm_b = 0;
            motors_enabled = false;
            raw_pwm_mode = false;
            motor_brake();
            pid_reset(&pid_a);
            pid_reset(&pid_b);
            current_pwm_a = 255;
            current_pwm_b = 255;
            send_response('b', 0, 0, 0);
            break;

        case CMD_VERSION:
            // left = FW_VERSION, right = safety_enabled
            send_response('v', 0, (int16_t)FW_VERSION, safety_enabled ? 1 : 0);
            break;

        case CMD_GET_PID: {
            // flags = param_id, respond with value in left
            int16_t val = 0;
            switch (req->flags) {
                case PID_PARAM_KP:   val = (int16_t)(pid_params.kp * 100); break;
                case PID_PARAM_KI:   val = (int16_t)(pid_params.ki * 100); break;
                case PID_PARAM_KD:   val = (int16_t)(pid_params.kd * 100); break;
                case PID_PARAM_IMAX: val = (int16_t)pid_params.integral_max; break;
                case PID_PARAM_RAMP: val = (int16_t)pid_params.ramp_limit; break;
            }
            send_response('g', req->flags, val, 0);
            break;
        }

        case CMD_SET_PID: {
            uint8_t param_id = req->flags;
            int16_t value = req->left;
            float fval = value / 100.0f;
            switch (param_id) {
                case PID_PARAM_KP:   pid_params.kp = fval; break;
                case PID_PARAM_KI:   pid_params.ki = fval; break;
                case PID_PARAM_KD:   pid_params.kd = fval; break;
                case PID_PARAM_IMAX: pid_params.integral_max = (float)value; break;
                case PID_PARAM_RAMP: pid_params.ramp_limit = (uint8_t)value; break;
            }
            // Apply to PID controllers
            pid_a.kp = pid_params.kp; pid_a.ki = pid_params.ki; pid_a.kd = pid_params.kd;
            pid_a.integral_max = pid_params.integral_max;
            pid_b.kp = pid_params.kp; pid_b.ki = pid_params.ki; pid_b.kd = pid_params.kd;
            pid_b.integral_max = pid_params.integral_max;
            send_response('t', param_id, value, 0);
            break;
        }

        case CMD_SAVE_PID:
            pid_store_save(&pid_params);
            send_response('w', 0, 0, 0);
            break;

        case CMD_DEBUG:
            // left = pwm_a, right = pwm_b in response
            send_response('d', 0, current_pwm_a, current_pwm_b);
            break;

        case CMD_PINS: {
            // Read pin registers
            uint16_t dirs = (uint16_t)(DDRD) | ((uint16_t)(DDRB & 0x3F) << 8);
            uint16_t vals = (uint16_t)(PORTD) | ((uint16_t)(PORTB & 0x3F) << 8);
            send_response('n', 0, (int16_t)dirs, (int16_t)vals);
            break;
        }

        case CMD_SAFETY:
            safety_enabled = (req->flags != 0);
            if (!safety_enabled) {
                safety_clear_error();
                wdt_disable();
            } else {
                wdt_enable(WDTO_2S);
            }
            send_response('f', req->flags, 0, 0);
            break;

        default:
            // Unknown command — echo it back
            send_response(req->cmd | 0x20, req->flags, req->left, req->right);
            break;
    }
}

// ============================================================================
// PID control loop (runs at 50Hz regardless of commands)
// ============================================================================
static void pid_loop() {
    unsigned long now = millis();
    uint16_t dt = now - last_pid_time;
    if (dt < PID_LOOP_INTERVAL_MS) return;
    last_pid_time = now;

    // Read encoders (physical A/B)
    long ticks_phys_a = encoder_get_ticks_a();
    long ticks_phys_b = encoder_get_ticks_b();
    encoder_reset_a();
    encoder_reset_b();

    // Map to logical left/right (with swap + encoder invert)
#if MOTOR_SWAP
    measured_rpm_a = ticks_to_rpm(ticks_phys_b, dt) * ENC_LEFT_INVERT;   // logical left = physical B
    measured_rpm_b = ticks_to_rpm(ticks_phys_a, dt) * ENC_RIGHT_INVERT;  // logical right = physical A
#else
    measured_rpm_a = ticks_to_rpm(ticks_phys_a, dt) * ENC_LEFT_INVERT;
    measured_rpm_b = ticks_to_rpm(ticks_phys_b, dt) * ENC_RIGHT_INVERT;
#endif

    if (raw_pwm_mode) return;

    // Safety check
    if (safety_enabled && safety_check(current_pwm_a, current_pwm_b, measured_rpm_a, measured_rpm_b)) {
        motor_coast();
        target_rpm_a = 0;
        target_rpm_b = 0;
        desired_rpm_a = 0;
        desired_rpm_b = 0;
        motors_enabled = false;
        pid_reset(&pid_a);
        pid_reset(&pid_b);
        current_pwm_a = 0;
        current_pwm_b = 0;
        return;
    }

    if (!motors_enabled) return;

    // Ramp limiting
    int16_t ramp = pid_params.ramp_limit;
    int16_t diff_a = desired_rpm_a - target_rpm_a;
    if (diff_a > ramp) target_rpm_a += ramp;
    else if (diff_a < -ramp) target_rpm_a -= ramp;
    else target_rpm_a = desired_rpm_a;

    int16_t diff_b = desired_rpm_b - target_rpm_b;
    if (diff_b > ramp) target_rpm_b += ramp;
    else if (diff_b < -ramp) target_rpm_b -= ramp;
    else target_rpm_b = desired_rpm_b;

    // Left motor PID (logical A)
    if (target_rpm_a == 0 && abs((int)measured_rpm_a) < SAFETY_STALL_RPM_THRESHOLD) {
        motor_set_left(0);
        current_pwm_a = 0;
        pid_reset(&pid_a);
    } else {
        int16_t effort = pid_compute(&pid_a, abs(target_rpm_a), abs(measured_rpm_a), dt);
        motor_set_left((target_rpm_a >= 0) ? effort : -effort);
        current_pwm_a = abs(effort);
    }

    // Right motor PID (logical B)
    if (target_rpm_b == 0 && abs((int)measured_rpm_b) < SAFETY_STALL_RPM_THRESHOLD) {
        motor_set_right(0);
        current_pwm_b = 0;
        pid_reset(&pid_b);
    } else {
        int16_t effort = pid_compute(&pid_b, abs(target_rpm_b), abs(measured_rpm_b), dt);
        motor_set_right((target_rpm_b >= 0) ? effort : -effort);
        current_pwm_b = abs(effort);
    }
}

// ============================================================================
// LED: 1 blink/s = connected, 2 fast blinks/s = no connection
// ============================================================================
static void led_blink() {
    unsigned long now = millis();
    bool connected = (last_rx_time > 0) && ((now - last_rx_time) < 1000);

    if (connected) {
        if ((now - led_timer) >= 500) {
            led_timer = now;
            led_phase ^= 1;
            digitalWrite(PIN_LED, led_phase);
        }
    } else {
        unsigned long t = now - led_timer;
        if (t < 125)       digitalWrite(PIN_LED, HIGH);
        else if (t < 250)  digitalWrite(PIN_LED, LOW);
        else if (t < 375)  digitalWrite(PIN_LED, HIGH);
        else               digitalWrite(PIN_LED, LOW);
        if (t >= 1000) led_timer = now;
    }
}

// ============================================================================
// Setup / Loop
// ============================================================================

void setup() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);

    comm_init();
    encoder_init();
    motor_init();

    pid_store_load(&pid_params);
    pid_init(&pid_a, pid_params.kp, pid_params.ki, pid_params.kd,
             pid_params.integral_max, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    pid_init(&pid_b, pid_params.kp, pid_params.ki, pid_params.kd,
             pid_params.integral_max, PID_OUTPUT_MIN, PID_OUTPUT_MAX);

    safety_init();
    last_pid_time = millis();

    digitalWrite(PIN_LED, LOW);
}

void loop() {
    if (safety_enabled) safety_feed_watchdog();

    // Process incoming request — exactly 1 response per request
    proto_request_t req;
    if (comm_receive(&req)) {
        handle_request(&req);
    }

    // PID loop
    pid_loop();

    // LED
    led_blink();

}
