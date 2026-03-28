#ifndef CAR_DIY_PROTOCOL_H
#define CAR_DIY_PROTOCOL_H

#include <stdint.h>

// ============================================================================
// UART Protocol V2 - ESP32-CAM <-> Arduino Pro Mini
// Master/slave: ESP32 sends 1 request every 50ms, Arduino sends 1 response.
// Baud: 115200, 8N1
// ============================================================================

#define PROTO_BAUD_RATE     38400       // 8MHz Pro Mini: 0% baud error at 38400
#define PROTO_START_BYTE    0xAA
#define PROTO_END_BYTE      0x55

// ============================================================================
// Request Commands (ESP32 -> Arduino) — uppercase
// ============================================================================
#define CMD_RPM         'R'     // Set target RPM via PID
#define CMD_PWM         'P'     // Set raw PWM (bypass PID)
#define CMD_STOP        'S'     // Stop (coast, wheels free)
#define CMD_BRAKE       'B'     // Brake (lock wheels)
#define CMD_VERSION     'V'     // Request firmware version
#define CMD_GET_PID     'G'     // Get PID params (param_id in flags)
#define CMD_SET_PID     'T'     // Set PID param (param_id in flags, value in left)
#define CMD_SAVE_PID    'W'     // Write PID to EEPROM
#define CMD_DEBUG       'D'     // Toggle debug mode
#define CMD_PINS        'N'     // Request pin states
#define CMD_SAFETY      'F'     // Safety enable/disable (flags: 0=off, 1=on)

// ============================================================================
// Response Commands (Arduino -> ESP32) — lowercase of request
// ============================================================================
// Response cmd = tolower(request cmd)
// e.g. 'R' -> 'r', 'P' -> 'p', 'V' -> 'v'

// ============================================================================
// PID parameter IDs (in flags byte for CMD_GET_PID / CMD_SET_PID)
// ============================================================================
#define PID_PARAM_KP        0x01
#define PID_PARAM_KI        0x02
#define PID_PARAM_KD        0x03
#define PID_PARAM_IMAX      0x04
#define PID_PARAM_RAMP      0x05
#define PID_PARAM_ALL       0xFF    // Get all (response sends KP in left, KI in right)

// ============================================================================
// Limits
// ============================================================================
#define MAX_RPM             130

// ============================================================================
// Request packet: 9 bytes
//   [START] [CMD] [FLAGS] [LEFT_LO] [LEFT_HI] [RIGHT_LO] [RIGHT_HI] [CRC8] [END]
// ============================================================================

#define PROTO_REQ_SIZE  9

typedef struct __attribute__((packed)) {
    uint8_t  start;         // 0xAA
    uint8_t  cmd;           // Command (uppercase)
    uint8_t  flags;         // Command-specific flags
    int16_t  left;          // Left value (RPM, PWM, or param value)
    int16_t  right;         // Right value (RPM, PWM, or param value)
    uint8_t  crc;           // CRC-8 over bytes 1..6
    uint8_t  end;           // 0x55
} proto_request_t;

// ============================================================================
// Response packet: 15 bytes
//   [START] [CMD] [FLAGS] [LEFT_LO] [LEFT_HI] [RIGHT_LO] [RIGHT_HI]
//   [ACT_LEFT_LO] [ACT_LEFT_HI] [ACT_RIGHT_LO] [ACT_RIGHT_HI]
//   [RX_CNT_LO] [RX_CNT_HI] [CRC8] [END]
// ============================================================================

#define PROTO_RSP_SIZE  15

typedef struct __attribute__((packed)) {
    uint8_t  start;         // 0xAA
    uint8_t  cmd;           // Response (lowercase of request)
    uint8_t  flags;         // Echo or response-specific flags
    int16_t  left;          // Echo requested left (or response data)
    int16_t  right;         // Echo requested right (or response data)
    int16_t  actual_left;   // Measured left RPM
    int16_t  actual_right;  // Measured right RPM
    uint16_t rx_count;      // Arduino total received packet count
    uint8_t  crc;           // CRC-8 over bytes 1..12
    uint8_t  end;           // 0x55
} proto_response_t;

// ============================================================================
// CRC-8 (polynomial 0x07, init 0x00)
// ============================================================================
static inline uint8_t proto_crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }
    return crc;
}

// Build a request packet
static inline void proto_build_request(proto_request_t *req, uint8_t cmd,
                                        uint8_t flags, int16_t left, int16_t right) {
    req->start = PROTO_START_BYTE;
    req->cmd = cmd;
    req->flags = flags;
    req->left = left;
    req->right = right;
    req->crc = proto_crc8(&req->cmd, 6);  // cmd(1) + flags(1) + left(2) + right(2)
    req->end = PROTO_END_BYTE;
}

// Build a response packet
static inline void proto_build_response(proto_response_t *rsp, uint8_t cmd,
                                         uint8_t flags, int16_t left, int16_t right,
                                         int16_t actual_left, int16_t actual_right,
                                         uint16_t rx_count) {
    rsp->start = PROTO_START_BYTE;
    rsp->cmd = cmd;
    rsp->flags = flags;
    rsp->left = left;
    rsp->right = right;
    rsp->actual_left = actual_left;
    rsp->actual_right = actual_right;
    rsp->rx_count = rx_count;
    rsp->crc = proto_crc8(&rsp->cmd, 12);  // cmd through rx_count
    rsp->end = PROTO_END_BYTE;
}

// Validate request
static inline bool proto_validate_request(const proto_request_t *req) {
    return req->start == PROTO_START_BYTE &&
           req->end == PROTO_END_BYTE &&
           proto_crc8(&req->cmd, 6) == req->crc;
}

// Validate response
static inline bool proto_validate_response(const proto_response_t *rsp) {
    return rsp->start == PROTO_START_BYTE &&
           rsp->end == PROTO_END_BYTE &&
           proto_crc8(&rsp->cmd, 12) == rsp->crc;
}

#endif // CAR_DIY_PROTOCOL_H
