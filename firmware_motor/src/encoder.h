#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>

// Initialize encoder pins and interrupts
void encoder_init();

// Get accumulated tick count (signed) since last reset
long encoder_get_ticks_a();
long encoder_get_ticks_b();

// Reset tick counters (call after reading for delta calculation)
void encoder_reset_a();
void encoder_reset_b();

#endif // ENCODER_H
