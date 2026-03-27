#ifndef CAR_DIY_PROTOCOL_H
#define CAR_DIY_PROTOCOL_H

#include <stdint.h>

// ============================================================================
// UART Protocol Definition - ESP32-CAM <-> Arduino Pro Mini
// Baud: 115200, 8N1
// ============================================================================

#define PROTO_BAUD_RATE     115200

// Packet framing
#define PROTO_START_BYTE    0xAA
#define PROTO_END_BYTE      0x55
#define PROTO_PACKET_SIZE   8       // Total packet size in bytes

// Command IDs (ESP32 -> Arduino)
#define CMD_SET_RPM         0x01    // Set target RPM for both motors
#define CMD_STOP            0x02    // Emergency stop
#define CMD_PING            0x03    // Heartbeat ping

// Response IDs (Arduino -> ESP32)
#define RSP_STATUS          0x81    // Current motor RPM status
#define RSP_ERROR           0x82    // Error report
#define RSP_PONG            0x83    // Heartbeat response

// Error codes
#define ERR_NONE            0x00
#define ERR_STALL_LEFT      0x01    // Left motor stalled
#define ERR_STALL_RIGHT     0x02    // Right motor stalled
#define ERR_STALL_BOTH      0x03    // Both motors stalled
#define ERR_TIMEOUT         0x04    // Command timeout (no commands received)
#define ERR_OVERCURRENT     0x05    // Overcurrent detected
#define ERR_WATCHDOG        0x06    // Watchdog reset occurred

// Limits
#define MAX_RPM             130     // Max RPM for JGA25-370 130RPM variant
#define CMD_TIMEOUT_MS      500     // Stop motors if no command for this long

// ============================================================================
// Packet structure (8 bytes total):
//   [START] [CMD] [PAYLOAD_0] [PAYLOAD_1] [PAYLOAD_2] [PAYLOAD_3] [CRC8] [END]
//    0xAA    cmd   -------- 4 bytes data --------      crc        0x55
// ============================================================================

typedef struct __attribute__((packed)) {
    uint8_t  start;         // PROTO_START_BYTE
    uint8_t  cmd;           // Command/Response ID
    union {
        uint8_t raw[4];     // Raw payload bytes
        struct {
            int16_t left_rpm;   // Signed: positive = forward, negative = reverse
            int16_t right_rpm;
        } rpm;
        struct {
            uint8_t  error_code;
            uint8_t  reserved[3];
        } error;
    } payload;
    uint8_t  crc;           // CRC-8 over bytes 1..5 (cmd + payload)
    uint8_t  end;           // PROTO_END_BYTE
} proto_packet_t;

// CRC-8 (polynomial 0x07, init 0x00)
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

// Build a packet
static inline void proto_build(proto_packet_t *pkt, uint8_t cmd,
                                int16_t left_rpm, int16_t right_rpm) {
    pkt->start = PROTO_START_BYTE;
    pkt->cmd = cmd;
    pkt->payload.rpm.left_rpm = left_rpm;
    pkt->payload.rpm.right_rpm = right_rpm;
    pkt->crc = proto_crc8(&pkt->cmd, 5);  // cmd(1) + payload(4) = 5 bytes
    pkt->end = PROTO_END_BYTE;
}

// Validate a received packet
static inline uint8_t proto_validate(const proto_packet_t *pkt) {
    if (pkt->start != PROTO_START_BYTE) return 0;
    if (pkt->end != PROTO_END_BYTE) return 0;
    if (proto_crc8(&pkt->cmd, 5) != pkt->crc) return 0;
    return 1;
}

#endif // CAR_DIY_PROTOCOL_H
