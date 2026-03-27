#include "comm.h"

static uint8_t rx_buf[PROTO_PACKET_SIZE];
static uint8_t rx_idx = 0;

void comm_init() {
    Serial.begin(PROTO_BAUD_RATE);
}

bool comm_receive(proto_packet_t *pkt) {
    while (Serial.available()) {
        uint8_t byte = Serial.read();

        // Look for start byte
        if (rx_idx == 0) {
            if (byte == PROTO_START_BYTE) {
                rx_buf[rx_idx++] = byte;
            }
            continue;
        }

        rx_buf[rx_idx++] = byte;

        // Full packet received
        if (rx_idx >= PROTO_PACKET_SIZE) {
            rx_idx = 0;

            // Check end byte
            if (rx_buf[PROTO_PACKET_SIZE - 1] != PROTO_END_BYTE) {
                continue;   // Bad framing, discard
            }

            // Copy to output and validate CRC
            memcpy(pkt, rx_buf, PROTO_PACKET_SIZE);
            if (proto_validate(pkt)) {
                return true;
            }
            // Bad CRC, discard
        }
    }
    return false;
}

void comm_send(const proto_packet_t *pkt) {
    Serial.write((const uint8_t *)pkt, PROTO_PACKET_SIZE);
}

void comm_send_status(int16_t left_rpm, int16_t right_rpm) {
    proto_packet_t pkt;
    proto_build(&pkt, RSP_STATUS, left_rpm, right_rpm);
    comm_send(&pkt);
}

void comm_send_error(uint8_t error_code) {
    proto_packet_t pkt;
    pkt.start = PROTO_START_BYTE;
    pkt.cmd = RSP_ERROR;
    pkt.payload.error.error_code = error_code;
    pkt.payload.error.reserved[0] = 0;
    pkt.payload.error.reserved[1] = 0;
    pkt.payload.error.reserved[2] = 0;
    pkt.crc = proto_crc8(&pkt.cmd, 5);
    pkt.end = PROTO_END_BYTE;
    comm_send(&pkt);
}

void comm_send_pong() {
    proto_packet_t pkt;
    proto_build(&pkt, RSP_PONG, 0, 0);
    comm_send(&pkt);
}

void comm_send_pid_param(uint8_t param_id, int16_t value) {
    proto_packet_t pkt;
    proto_build_pid(&pkt, RSP_PID_PARAM, param_id, value);
    comm_send(&pkt);
}

void comm_send_debug(uint8_t pwm_a, uint8_t pwm_b,
                     int16_t target_a, int16_t target_b) {
    proto_packet_t pkt;
    pkt.start = PROTO_START_BYTE;
    pkt.cmd = RSP_DEBUG;
    pkt.payload.debug.pwm_a = (int8_t)(pwm_a / 2);
    pkt.payload.debug.pwm_b = (int8_t)(pwm_b / 2);
    pkt.payload.debug.target_a = (int8_t)constrain(target_a, -127, 127);
    pkt.payload.debug.target_b = (int8_t)constrain(target_b, -127, 127);
    pkt.crc = proto_crc8(&pkt.cmd, 5);
    pkt.end = PROTO_END_BYTE;
    comm_send(&pkt);
}

void comm_send_encoders(long total_a, long total_b) {
    proto_packet_t pkt;
    pkt.start = PROTO_START_BYTE;
    pkt.cmd = RSP_ENCODERS;
    pkt.payload.encoders.enc_a = (int16_t)(total_a & 0xFFFF);
    pkt.payload.encoders.enc_b = (int16_t)(total_b & 0xFFFF);
    pkt.crc = proto_crc8(&pkt.cmd, 5);
    pkt.end = PROTO_END_BYTE;
    comm_send(&pkt);
}
