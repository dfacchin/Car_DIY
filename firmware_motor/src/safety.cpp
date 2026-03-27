#include "safety.h"
#include "config.h"
#include "protocol.h"
#include <avr/wdt.h>

static unsigned long last_cmd_time = 0;
static unsigned long stall_start_a = 0;
static unsigned long stall_start_b = 0;
static uint8_t current_error = PROTO_ERR_NONE;

void safety_init() {
    last_cmd_time = millis();

    // Enable hardware watchdog (2 second timeout)
    wdt_enable(WDTO_2S);
}

void safety_cmd_received() {
    last_cmd_time = millis();
}

bool safety_check(uint8_t pwm_a, uint8_t pwm_b, float rpm_a, float rpm_b) {
    unsigned long now = millis();

    // --- Command timeout ---
    if ((now - last_cmd_time) > SAFETY_CMD_TIMEOUT_MS) {
        current_error = PROTO_ERR_TIMEOUT;
        return true;    // Stop motors
    }

    // --- Stall detection Motor A ---
    if (pwm_a > SAFETY_STALL_PWM_THRESHOLD && abs((int)rpm_a) < SAFETY_STALL_RPM_THRESHOLD) {
        if (stall_start_a == 0) {
            stall_start_a = now;
        } else if ((now - stall_start_a) > SAFETY_STALL_DURATION_MS) {
            current_error = PROTO_ERR_STALL_LEFT;
            stall_start_a = 0;
            return true;
        }
    } else {
        stall_start_a = 0;
    }

    // --- Stall detection Motor B ---
    if (pwm_b > SAFETY_STALL_PWM_THRESHOLD && abs((int)rpm_b) < SAFETY_STALL_RPM_THRESHOLD) {
        if (stall_start_b == 0) {
            stall_start_b = now;
        } else if ((now - stall_start_b) > SAFETY_STALL_DURATION_MS) {
            current_error = PROTO_ERR_STALL_RIGHT;
            stall_start_b = 0;
            return true;
        }
    } else {
        stall_start_b = 0;
    }

    return false;   // All OK
}

uint8_t safety_get_error() {
    return current_error;
}

void safety_clear_error() {
    current_error = PROTO_ERR_NONE;
    stall_start_a = 0;
    stall_start_b = 0;
}

void safety_feed_watchdog() {
    wdt_reset();
}

int16_t safety_ramp_limit(int16_t current, int16_t desired) {
    int16_t diff = desired - current;
    if (diff > SAFETY_RAMP_MAX_RPM_PER_CYCLE) {
        return current + SAFETY_RAMP_MAX_RPM_PER_CYCLE;
    } else if (diff < -SAFETY_RAMP_MAX_RPM_PER_CYCLE) {
        return current - SAFETY_RAMP_MAX_RPM_PER_CYCLE;
    }
    return desired;
}
