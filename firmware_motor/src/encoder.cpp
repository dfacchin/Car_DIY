#include "encoder.h"
#include "config.h"

// Volatile tick counters updated by ISRs (reset by PID each cycle)
static volatile long ticks_a = 0;
static volatile long ticks_b = 0;

// Cumulative counters (debug only, never reset, start at 0 on boot)
static volatile long total_ticks_a = 0;
static volatile long total_ticks_b = 0;

// ============================================================================
// Encoder A - Hardware interrupts (INT0, INT1)
// ============================================================================

static void isr_enc_a_phase_a() {
    if (digitalRead(PIN_ENC_A_PHASE_A) == digitalRead(PIN_ENC_A_PHASE_B)) {
        ticks_a--;
        total_ticks_a--;
    } else {
        ticks_a++;
        total_ticks_a++;
    }
}

static void isr_enc_a_phase_b() {
    if (digitalRead(PIN_ENC_A_PHASE_A) == digitalRead(PIN_ENC_A_PHASE_B)) {
        ticks_a++;
        total_ticks_a++;
    } else {
        ticks_a--;
        total_ticks_a--;
    }
}

// ============================================================================
// Encoder B - Pin change interrupts (PCINT2 vector for port D pins 4,5)
// ============================================================================

static volatile uint8_t prev_enc_b_state = 0;

ISR(PCINT2_vect) {
    uint8_t curr = (digitalRead(PIN_ENC_B_PHASE_A) << 1) | digitalRead(PIN_ENC_B_PHASE_B);
    // Quadrature state machine using XOR of transitions
    uint8_t change = curr ^ prev_enc_b_state;

    if (change) {
        // Determine direction from quadrature transition
        // Using a lookup approach for 2-bit gray code transitions
        int8_t dir = 0;
        switch ((prev_enc_b_state << 2) | curr) {
            case 0b0001: case 0b0111: case 0b1110: case 0b1000: dir =  1; break;
            case 0b0010: case 0b1011: case 0b1101: case 0b0100: dir = -1; break;
        }
        ticks_b += dir;
        total_ticks_b += dir;
        prev_enc_b_state = curr;
    }
}

// ============================================================================
// Public API
// ============================================================================

void encoder_init() {
    // Encoder A pins
    pinMode(PIN_ENC_A_PHASE_A, INPUT_PULLUP);
    pinMode(PIN_ENC_A_PHASE_B, INPUT_PULLUP);

    // Encoder B pins
    pinMode(PIN_ENC_B_PHASE_A, INPUT_PULLUP);
    pinMode(PIN_ENC_B_PHASE_B, INPUT_PULLUP);

    // Attach hardware interrupts for Encoder A (both edges)
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_A_PHASE_A), isr_enc_a_phase_a, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_A_PHASE_B), isr_enc_a_phase_b, CHANGE);

    // Enable pin change interrupts for Encoder B (PCINT2 group, pins 4 & 5)
    prev_enc_b_state = (digitalRead(PIN_ENC_B_PHASE_A) << 1) | digitalRead(PIN_ENC_B_PHASE_B);
    PCICR  |= (1 << PCIE2);                    // Enable PCINT2 vector
    PCMSK2 |= (1 << PCINT20) | (1 << PCINT21); // Enable pins 4 and 5
}

long encoder_get_ticks_a() {
    noInterrupts();
    long val = ticks_a;
    interrupts();
    return val;
}

long encoder_get_ticks_b() {
    noInterrupts();
    long val = ticks_b;
    interrupts();
    return val;
}

void encoder_reset_a() {
    noInterrupts();
    ticks_a = 0;
    interrupts();
}

void encoder_reset_b() {
    noInterrupts();
    ticks_b = 0;
    interrupts();
}

long encoder_get_total_a() {
    noInterrupts();
    long val = total_ticks_a;
    interrupts();
    return val;
}

long encoder_get_total_b() {
    noInterrupts();
    long val = total_ticks_b;
    interrupts();
    return val;
}
