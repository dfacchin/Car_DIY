#include "motor_comm.h"
#include "config.h"

// Use Serial2 on GPIO 14 (TX) / GPIO 15 (RX) for motor comm
// Serial (UART0) stays free for USB debug
#define MOTOR_SERIAL Serial2

static int16_t last_left_rpm = 0;
static int16_t last_right_rpm = 0;
static uint8_t last_error = 0;

// Debug data cache
static int8_t last_pwm_a = 0, last_pwm_b = 0;
static int8_t last_target_a = 0, last_target_b = 0;

// Encoder counters
static int16_t last_enc_a = 0, last_enc_b = 0;

// PID param cache
static motor_pid_cache_t pid_cache = {0, 0, 0, 0, 0, false};

// Connection tracking
static unsigned long last_pong_time = 0;
static unsigned long last_any_rx_time = 0;

// Packet counters
static uint32_t tx_count = 0;
static uint32_t rx_count = 0;

static uint8_t rx_buf[PROTO_PACKET_SIZE];
static uint8_t rx_idx = 0;

void motor_comm_init() {
    MOTOR_SERIAL.begin(PROTO_BAUD_RATE, SERIAL_8N1, MOTOR_UART_RX, MOTOR_UART_TX);
    Serial.printf("Motor UART on TX=%d RX=%d @ %d baud\n",
                  MOTOR_UART_TX, MOTOR_UART_RX, PROTO_BAUD_RATE);
}

static void motor_send(const proto_packet_t *pkt) {
    MOTOR_SERIAL.write((const uint8_t *)pkt, PROTO_PACKET_SIZE);
    tx_count++;
}

void motor_comm_set_rpm(int16_t left_rpm, int16_t right_rpm) {
    proto_packet_t pkt;
    proto_build(&pkt, CMD_SET_RPM, left_rpm, right_rpm);
    motor_send(&pkt);
}

void motor_comm_stop() {
    proto_packet_t pkt;
    proto_build(&pkt, CMD_STOP, 0, 0);
    motor_send(&pkt);
}

void motor_comm_brake() {
    proto_packet_t pkt;
    proto_build(&pkt, CMD_BRAKE, 0, 0);
    motor_send(&pkt);
}

void motor_comm_ping() {
    proto_packet_t pkt;
    proto_build(&pkt, CMD_PING, 0, 0);
    motor_send(&pkt);
}

void motor_comm_get_pid() {
    proto_packet_t pkt;
    proto_build_raw(&pkt, CMD_GET_PID, 0, 0, 0, 0);
    motor_send(&pkt);
}

void motor_comm_set_pid(uint8_t param_id, int16_t value) {
    proto_packet_t pkt;
    proto_build_pid(&pkt, CMD_SET_PID, param_id, value);
    motor_send(&pkt);
}

void motor_comm_save_pid() {
    proto_packet_t pkt;
    proto_build_raw(&pkt, CMD_SAVE_PID, 0, 0, 0, 0);
    motor_send(&pkt);
}

void motor_comm_toggle_debug() {
    proto_packet_t pkt;
    proto_build_raw(&pkt, CMD_GET_DEBUG, 0, 0, 0, 0);
    motor_send(&pkt);
}

bool motor_comm_receive(proto_packet_t *pkt) {
    while (MOTOR_SERIAL.available()) {
        uint8_t byte = MOTOR_SERIAL.read();

        if (rx_idx == 0) {
            if (byte == PROTO_START_BYTE) {
                rx_buf[rx_idx++] = byte;
            }
            continue;
        }

        rx_buf[rx_idx++] = byte;

        if (rx_idx >= PROTO_PACKET_SIZE) {
            rx_idx = 0;

            if (rx_buf[PROTO_PACKET_SIZE - 1] != PROTO_END_BYTE) {
                continue;
            }

            memcpy(pkt, rx_buf, PROTO_PACKET_SIZE);
            if (proto_validate(pkt)) {
                last_any_rx_time = millis();
                rx_count++;
                switch (pkt->cmd) {
                    case RSP_PONG:
                        last_pong_time = millis();
                        break;
                    case RSP_STATUS:
                        last_left_rpm = pkt->payload.rpm.left_rpm;
                        last_right_rpm = pkt->payload.rpm.right_rpm;
                        break;
                    case RSP_ERROR:
                        last_error = pkt->payload.error.error_code;
                        break;
                    case RSP_PID_PARAM:
                        switch (pkt->payload.pid.param_id) {
                            case PID_PARAM_KP:   pid_cache.kp = pkt->payload.pid.value; break;
                            case PID_PARAM_KI:   pid_cache.ki = pkt->payload.pid.value; break;
                            case PID_PARAM_KD:   pid_cache.kd = pkt->payload.pid.value; break;
                            case PID_PARAM_IMAX: pid_cache.imax = pkt->payload.pid.value; break;
                            case PID_PARAM_RAMP: pid_cache.ramp = pkt->payload.pid.value; break;
                        }
                        pid_cache.valid = true;
                        break;
                    case RSP_DEBUG:
                        last_pwm_a = pkt->payload.debug.pwm_a;
                        last_pwm_b = pkt->payload.debug.pwm_b;
                        last_target_a = pkt->payload.debug.target_a;
                        last_target_b = pkt->payload.debug.target_b;
                        break;
                    case RSP_ENCODERS:
                        last_enc_a = pkt->payload.encoders.enc_a;
                        last_enc_b = pkt->payload.encoders.enc_b;
                        break;
                }
                return true;
            }
        }
    }
    return false;
}

void motor_comm_get_status(int16_t *left_rpm, int16_t *right_rpm) {
    *left_rpm = last_left_rpm;
    *right_rpm = last_right_rpm;
}

uint8_t motor_comm_get_error() {
    return last_error;
}

void motor_comm_get_debug(int8_t *pwm_a, int8_t *pwm_b,
                          int8_t *target_a, int8_t *target_b) {
    *pwm_a = last_pwm_a;
    *pwm_b = last_pwm_b;
    *target_a = last_target_a;
    *target_b = last_target_b;
}

void motor_comm_get_encoders(int16_t *enc_a, int16_t *enc_b) {
    *enc_a = last_enc_a;
    *enc_b = last_enc_b;
}

bool motor_comm_is_connected(unsigned long timeout_ms) {
    if (last_any_rx_time == 0) return false;
    return (millis() - last_any_rx_time) < timeout_ms;
}

unsigned long motor_comm_last_pong_age() {
    if (last_pong_time == 0) return 0;
    return millis() - last_pong_time;
}

uint32_t motor_comm_get_tx_count() { return tx_count; }
uint32_t motor_comm_get_rx_count() { return rx_count; }

const motor_pid_cache_t *motor_comm_get_pid_cache() {
    return &pid_cache;
}
