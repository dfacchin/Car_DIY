#ifndef PID_H
#define PID_H

#include <Arduino.h>

typedef struct {
    float kp;
    float ki;
    float kd;
    float integral;
    float prev_error;
    float integral_max;     // Anti-windup clamp
    int16_t output_min;
    int16_t output_max;
} pid_controller_t;

// Initialize PID controller with gains
void pid_init(pid_controller_t *pid, float kp, float ki, float kd,
              float integral_max, int16_t output_min, int16_t output_max);

// Reset PID state (call when target changes significantly or on stop)
void pid_reset(pid_controller_t *pid);

// Compute PID output given setpoint and measured value
// dt_ms: time since last call in milliseconds
// Returns: control output (clamped to output_min..output_max)
int16_t pid_compute(pid_controller_t *pid, float setpoint, float measured, uint16_t dt_ms);

#endif // PID_H
