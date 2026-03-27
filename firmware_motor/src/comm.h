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

#endif // COMM_H
