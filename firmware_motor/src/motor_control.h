#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>

// Initialize motor driver pins
void motor_init();

// Set motor A (left) power: -255 to +255 (negative = reverse)
void motor_set_a(int16_t power);

// Set motor B (right) power: -255 to +255 (negative = reverse)
void motor_set_b(int16_t power);

// Brake both motors (active braking: both pins HIGH)
void motor_brake();

// Coast both motors (free spin: both pins LOW)
void motor_coast();

#endif // MOTOR_CONTROL_H
