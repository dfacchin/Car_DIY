#ifndef COMM_H
#define COMM_H

#include <Arduino.h>
#include "protocol.h"

// Initialize UART communication
void comm_init();

// Check for incoming packets (non-blocking)
// Returns true if a valid packet was received
bool comm_receive(proto_packet_t *pkt);

// Send a packet
void comm_send(const proto_packet_t *pkt);

// Send status response with current RPM values
void comm_send_status(int16_t left_rpm, int16_t right_rpm);

// Send error response
void comm_send_error(uint8_t error_code);

// Send pong response
void comm_send_pong();

// Send PID parameter value (fixed-point: value = actual * 100)
void comm_send_pid_param(uint8_t param_id, int16_t value);

// Send extended debug status
void comm_send_debug(uint8_t pwm_a, uint8_t pwm_b,
                     int16_t target_a, int16_t target_b);

// Send encoder cumulative tick counters (wrapping int16)
void comm_send_encoders(long total_a, long total_b);

#endif // COMM_H
