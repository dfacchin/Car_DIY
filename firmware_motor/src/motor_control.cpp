#include "motor_control.h"
#include "config.h"

void motor_init() {
    pinMode(PIN_ENA, OUTPUT);
    pinMode(PIN_IN1, OUTPUT);
    pinMode(PIN_IN2, OUTPUT);
    pinMode(PIN_ENB, OUTPUT);
    pinMode(PIN_IN3, OUTPUT);
    pinMode(PIN_IN4, OUTPUT);

    // Start with motors stopped
    motor_brake();
}

void motor_set_a(int16_t power) {
    if (power > 0) {
        // Forward
        digitalWrite(PIN_IN1, HIGH);
        digitalWrite(PIN_IN2, LOW);
        analogWrite(PIN_ENA, constrain(power, 0, 255));
    } else if (power < 0) {
        // Reverse
        digitalWrite(PIN_IN1, LOW);
        digitalWrite(PIN_IN2, HIGH);
        analogWrite(PIN_ENA, constrain(-power, 0, 255));
    } else {
        // Brake
        digitalWrite(PIN_IN1, HIGH);
        digitalWrite(PIN_IN2, HIGH);
        analogWrite(PIN_ENA, 0);
    }
}

void motor_set_b(int16_t power) {
    if (power > 0) {
        // Forward
        digitalWrite(PIN_IN3, HIGH);
        digitalWrite(PIN_IN4, LOW);
        analogWrite(PIN_ENB, constrain(power, 0, 255));
    } else if (power < 0) {
        // Reverse
        digitalWrite(PIN_IN3, LOW);
        digitalWrite(PIN_IN4, HIGH);
        analogWrite(PIN_ENB, constrain(-power, 0, 255));
    } else {
        // Brake
        digitalWrite(PIN_IN3, HIGH);
        digitalWrite(PIN_IN4, HIGH);
        analogWrite(PIN_ENB, 0);
    }
}

void motor_brake() {
    // Active brake: both direction pins HIGH + full PWM
    // This shorts motor windings through the H-bridge, locking wheels
    digitalWrite(PIN_IN1, HIGH);
    digitalWrite(PIN_IN2, HIGH);
    analogWrite(PIN_ENA, 255);
    digitalWrite(PIN_IN3, HIGH);
    digitalWrite(PIN_IN4, HIGH);
    analogWrite(PIN_ENB, 255);
}

void motor_coast() {
    // Coast: both direction pins LOW
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, LOW);
    analogWrite(PIN_ENA, 0);
    digitalWrite(PIN_IN3, LOW);
    digitalWrite(PIN_IN4, LOW);
    analogWrite(PIN_ENB, 0);
}
