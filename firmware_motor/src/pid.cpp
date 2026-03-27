#include "pid.h"

void pid_init(pid_controller_t *pid, float kp, float ki, float kd,
              float integral_max, int16_t output_min, int16_t output_max) {
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->integral_max = integral_max;
    pid->output_min = output_min;
    pid->output_max = output_max;
}

void pid_reset(pid_controller_t *pid) {
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
}

int16_t pid_compute(pid_controller_t *pid, float setpoint, float measured, uint16_t dt_ms) {
    float dt = dt_ms / 1000.0f;
    if (dt <= 0.0f) return 0;

    float error = setpoint - measured;

    // Proportional
    float p_term = pid->kp * error;

    // Integral with anti-windup
    pid->integral += error * dt;
    if (pid->integral > pid->integral_max) pid->integral = pid->integral_max;
    if (pid->integral < -pid->integral_max) pid->integral = -pid->integral_max;
    float i_term = pid->ki * pid->integral;

    // Derivative (on error, not measurement, for simplicity)
    float derivative = (error - pid->prev_error) / dt;
    float d_term = pid->kd * derivative;
    pid->prev_error = error;

    // Sum and clamp
    float output = p_term + i_term + d_term;
    if (output > pid->output_max) output = pid->output_max;
    if (output < pid->output_min) output = pid->output_min;

    return (int16_t)output;
}
