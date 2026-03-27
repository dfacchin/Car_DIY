#ifndef MOTOR_COMM_H
#define MOTOR_COMM_H

#include <Arduino.h>
#include "protocol.h"

// Initialize UART for motor communication
void motor_comm_init();

// Send RPM command to motor controller
void motor_comm_set_rpm(int16_t left_rpm, int16_t right_rpm);

// Send emergency stop
void motor_comm_stop();

// Send heartbeat ping
void motor_comm_ping();

// Check for incoming status/error from motor controller (non-blocking)
// Returns true if a packet was received
bool motor_comm_receive(proto_packet_t *pkt);

// Get last known motor status
void motor_comm_get_status(int16_t *left_rpm, int16_t *right_rpm);

// Get last error code
uint8_t motor_comm_get_error();

#endif // MOTOR_COMM_H
