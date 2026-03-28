#include "motor_comm.h"
#include "config.h"

#define MOTOR_SERIAL Serial2
#define RSP_TIMEOUT_MS  100

static proto_response_t last_rsp = {};
static bool last_rsp_valid = false;

static unsigned long last_rx_time = 0;
static uint16_t fw_version = 0;

static uint32_t tx_count = 0;
static uint32_t rx_count = 0;

static uint8_t rx_buf[PROTO_RSP_SIZE];
static uint8_t rx_idx = 0;

void motor_comm_init() {
    MOTOR_SERIAL.begin(PROTO_BAUD_RATE, SERIAL_8N1, MOTOR_UART_RX, MOTOR_UART_TX);
    Serial.printf("Motor UART on TX=%d RX=%d @ %d baud\n",
                  MOTOR_UART_TX, MOTOR_UART_RX, PROTO_BAUD_RATE);
}

static bool try_receive() {
    while (MOTOR_SERIAL.available()) {
        uint8_t byte = MOTOR_SERIAL.read();

        if (rx_idx == 0) {
            if (byte == PROTO_START_BYTE) rx_buf[rx_idx++] = byte;
            continue;
        }

        rx_buf[rx_idx++] = byte;

        if (rx_idx >= PROTO_RSP_SIZE) {
            rx_idx = 0;
            if (rx_buf[PROTO_RSP_SIZE - 1] != PROTO_END_BYTE) continue;

            memcpy(&last_rsp, rx_buf, PROTO_RSP_SIZE);
            if (proto_validate_response(&last_rsp)) {
                last_rsp_valid = true;
                last_rx_time = millis();
                rx_count++;
                if (last_rsp.cmd == 'v') {
                    fw_version = (uint16_t)last_rsp.left;
                }
                return true;
            }
        }
    }
    return false;
}

bool motor_comm_send(uint8_t cmd, uint8_t flags, int16_t left, int16_t right) {
    proto_request_t req;
    proto_build_request(&req, cmd, flags, left, right);
    MOTOR_SERIAL.write((const uint8_t *)&req, PROTO_REQ_SIZE);
    MOTOR_SERIAL.flush();
    tx_count++;

    last_rsp_valid = false;
    unsigned long start = millis();
    while ((millis() - start) < RSP_TIMEOUT_MS) {
        if (try_receive()) return true;
    }
    return false;
}

const proto_response_t *motor_comm_last_response() {
    return last_rsp_valid ? &last_rsp : nullptr;
}

int16_t motor_comm_actual_left() { return last_rsp.actual_left; }
int16_t motor_comm_actual_right() { return last_rsp.actual_right; }
uint16_t motor_comm_arduino_rx_count() { return last_rsp.rx_count; }

bool motor_comm_is_connected() {
    return last_rx_time > 0 && (millis() - last_rx_time) < 1000;
}

uint16_t motor_comm_get_fw_version() { return fw_version; }
uint32_t motor_comm_get_tx_count() { return tx_count; }
uint32_t motor_comm_get_rx_count() { return rx_count; }
