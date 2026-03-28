#ifndef COMM_H
#define COMM_H

#include <Arduino.h>
#include "protocol.h"

// Initialize UART communication
void comm_init();

// Check for incoming request (non-blocking)
// Returns true if a valid request was received
bool comm_receive(proto_request_t *req);

// Send a response
void comm_send_response(const proto_response_t *rsp);

// Get total received packet count
uint16_t comm_get_rx_count();

#endif // COMM_H
