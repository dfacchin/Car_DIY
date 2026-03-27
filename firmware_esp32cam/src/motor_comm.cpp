#include "motor_comm.h"
#include "config.h"

static int16_t last_left_rpm = 0;
static int16_t last_right_rpm = 0;
static uint8_t last_error = 0;

static uint8_t rx_buf[PROTO_PACKET_SIZE];
static uint8_t rx_idx = 0;

void motor_comm_init() {
    // ESP32-CAM uses Serial (UART0) on GPIO 1/3
    Serial.begin(PROTO_BAUD_RATE);
}

void motor_comm_set_rpm(int16_t left_rpm, int16_t right_rpm) {
    proto_packet_t pkt;
    proto_build(&pkt, CMD_SET_RPM, left_rpm, right_rpm);
    Serial.write((const uint8_t *)&pkt, PROTO_PACKET_SIZE);
}

void motor_comm_stop() {
    proto_packet_t pkt;
    proto_build(&pkt, CMD_STOP, 0, 0);
    Serial.write((const uint8_t *)&pkt, PROTO_PACKET_SIZE);
}

void motor_comm_ping() {
    proto_packet_t pkt;
    proto_build(&pkt, CMD_PING, 0, 0);
    Serial.write((const uint8_t *)&pkt, PROTO_PACKET_SIZE);
}

bool motor_comm_receive(proto_packet_t *pkt) {
    while (Serial.available()) {
        uint8_t byte = Serial.read();

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
                // Update cached state
                if (pkt->cmd == RSP_STATUS) {
                    last_left_rpm = pkt->payload.rpm.left_rpm;
                    last_right_rpm = pkt->payload.rpm.right_rpm;
                } else if (pkt->cmd == RSP_ERROR) {
                    last_error = pkt->payload.error.error_code;
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
