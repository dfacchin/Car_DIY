#ifndef MOTOR_COMM_H
#define MOTOR_COMM_H

#include <Arduino.h>
#include "protocol.h"

// Initialize UART for motor communication
void motor_comm_init();

// Send RPM command to motor controller
void motor_comm_set_rpm(int16_t left_rpm, int16_t right_rpm);

// Send emergency stop (coast - wheels free)
void motor_comm_stop();

// Send active brake (lock wheels)
void motor_comm_brake();

// Send heartbeat ping
void motor_comm_ping();

// Request PID parameters from motor controller
void motor_comm_get_pid();

// Set a single PID parameter (value is fixed-point * 100 for Kp/Ki/Kd)
void motor_comm_set_pid(uint8_t param_id, int16_t value);

// Save PID params to EEPROM on motor controller
void motor_comm_save_pid();

// Toggle debug mode on motor controller
void motor_comm_toggle_debug();

// Check for incoming status/error from motor controller (non-blocking)
// Returns true if a packet was received
bool motor_comm_receive(proto_packet_t *pkt);

// Get last known motor status
void motor_comm_get_status(int16_t *left_rpm, int16_t *right_rpm);

// Get last error code
uint8_t motor_comm_get_error();

// Get last debug data
void motor_comm_get_debug(int8_t *pwm_a, int8_t *pwm_b,
                          int8_t *target_a, int8_t *target_b);

// Get last encoder counters
void motor_comm_get_encoders(int16_t *enc_a, int16_t *enc_b);

// Returns true if Arduino responded to ping within the last timeout_ms
bool motor_comm_is_connected(unsigned long timeout_ms = 1000);

// Get milliseconds since last pong received (0 = never)
unsigned long motor_comm_last_pong_age();

// PID param cache (populated by RSP_PID_PARAM responses)
typedef struct {
    int16_t kp;     // *100 fixed point
    int16_t ki;     // *100 fixed point
    int16_t kd;     // *100 fixed point
    int16_t imax;
    int16_t ramp;
    bool valid;
} motor_pid_cache_t;

// Get cached PID params
const motor_pid_cache_t *motor_comm_get_pid_cache();

#endif // MOTOR_COMM_H
