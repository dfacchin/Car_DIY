#ifndef SAFETY_H
#define SAFETY_H

#include <Arduino.h>

// Initialize safety systems (watchdog, timers)
void safety_init();

// Call when a valid command is received (resets timeout)
void safety_cmd_received();

// Call every PID cycle with current motor state
// Returns true if motors should be stopped (error condition)
// pwm_a/b: current PWM output, rpm_a/b: current measured RPM
bool safety_check(uint8_t pwm_a, uint8_t pwm_b, float rpm_a, float rpm_b);

// Get the current error code (0 if no error)
uint8_t safety_get_error();

// Clear error state
void safety_clear_error();

// Reset watchdog timer (call in main loop)
void safety_feed_watchdog();

// Apply ramp limiting to target RPM
// current: current target, desired: new target
// Returns: ramp-limited target
int16_t safety_ramp_limit(int16_t current, int16_t desired);

#endif // SAFETY_H
