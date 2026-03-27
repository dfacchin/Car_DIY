#ifndef PID_STORE_H
#define PID_STORE_H

#include <Arduino.h>

typedef struct {
    float kp;
    float ki;
    float kd;
    float integral_max;
    uint8_t ramp_limit;     // RPM per PID cycle
} pid_params_t;

// Load PID params from EEPROM. Returns true if valid data found.
// If no valid data, fills defaults.
bool pid_store_load(pid_params_t *params);

// Save PID params to EEPROM
void pid_store_save(const pid_params_t *params);

#endif // PID_STORE_H
