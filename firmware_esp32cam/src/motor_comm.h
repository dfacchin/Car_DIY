#ifndef MOTOR_COMM_H
#define MOTOR_COMM_H

#include <Arduino.h>
#include "protocol.h"

// Initialize UART for motor communication
void motor_comm_init();

// Send a request and receive response (blocking, timeout 100ms)
// Returns true if valid response received
bool motor_comm_send(uint8_t cmd, uint8_t flags, int16_t left, int16_t right);

// Get last response (valid after motor_comm_send returns true)
const proto_response_t *motor_comm_last_response();

// Convenience: get cached values from last response
int16_t motor_comm_actual_left();
int16_t motor_comm_actual_right();
uint16_t motor_comm_arduino_rx_count();
bool motor_comm_is_connected();
uint16_t motor_comm_get_fw_version();

// Packet counters
uint32_t motor_comm_get_tx_count();
uint32_t motor_comm_get_rx_count();

#endif // MOTOR_COMM_H
