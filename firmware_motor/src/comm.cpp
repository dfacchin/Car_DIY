#include "comm.h"
#include "config.h"

static uint8_t rx_buf[PROTO_REQ_SIZE];
static uint8_t rx_idx = 0;
static uint16_t rx_count = 0;

void comm_init() {
    Serial.begin(PROTO_BAUD_RATE);
}

bool comm_receive(proto_request_t *req) {
    while (Serial.available()) {
        uint8_t byte = Serial.read();

        if (rx_idx == 0) {
            if (byte == PROTO_START_BYTE) {
                rx_buf[rx_idx++] = byte;
            }
            continue;
        }

        rx_buf[rx_idx++] = byte;

        if (rx_idx >= PROTO_REQ_SIZE) {
            rx_idx = 0;

            if (rx_buf[PROTO_REQ_SIZE - 1] != PROTO_END_BYTE) {
                continue;
            }

            memcpy(req, rx_buf, PROTO_REQ_SIZE);
            if (proto_validate_request(req)) {
                rx_count++;
                return true;
            }
        }
    }
    return false;
}

void comm_send_response(const proto_response_t *rsp) {
    Serial.write((const uint8_t *)rsp, PROTO_RSP_SIZE);
}

uint16_t comm_get_rx_count() {
    return rx_count;
}
