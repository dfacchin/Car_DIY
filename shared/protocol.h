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
#define PROTO_PACKET_SIZE   8       // Standard packet size in bytes

// ============================================================================
// Command IDs (ESP32 -> Arduino)
// ============================================================================
#define CMD_SET_RPM         0x01    // Set target RPM for both motors
#define CMD_STOP            0x02    // Emergency stop
#define CMD_PING            0x03    // Heartbeat ping
#define CMD_GET_PID         0x04    // Request current PID params
#define CMD_SET_PID         0x05    // Set one PID parameter
#define CMD_SAVE_PID        0x06    // Save current PID params to EEPROM
#define CMD_BRAKE           0x07    // Active brake (lock wheels)
#define CMD_GET_DEBUG       0x08    // Request extended debug status

// ============================================================================
// Response IDs (Arduino -> ESP32)
// ============================================================================
#define RSP_STATUS          0x81    // Current motor RPM status
#define RSP_ERROR           0x82    // Error report
#define RSP_PONG            0x83    // Heartbeat response
#define RSP_PID_PARAM       0x84    // PID parameter value
#define RSP_DEBUG           0x85    // Extended debug status (PWM + targets)
#define RSP_ENCODERS        0x86    // Encoder tick counters (debug only)

// ============================================================================
// PID parameter IDs (used in CMD_SET_PID / RSP_PID_PARAM)
// ============================================================================
#define PID_PARAM_KP        0x01
#define PID_PARAM_KI        0x02
#define PID_PARAM_KD        0x03
#define PID_PARAM_IMAX      0x04    // Integral max (anti-windup)
#define PID_PARAM_RAMP      0x05    // Ramp rate limit (RPM per cycle)

// Error codes (prefixed to avoid conflicts with ESP-IDF/lwip)
#define PROTO_ERR_NONE            0x00
#define PROTO_ERR_STALL_LEFT      0x01    // Left motor stalled
#define PROTO_ERR_STALL_RIGHT     0x02    // Right motor stalled
#define PROTO_ERR_STALL_BOTH      0x03    // Both motors stalled
#define PROTO_ERR_TIMEOUT         0x04    // Command timeout (no commands received)
#define PROTO_ERR_OVERCURRENT     0x05    // Overcurrent detected
#define PROTO_ERR_WATCHDOG        0x06    // Watchdog reset occurred

// Limits
#define MAX_RPM             130     // Max RPM for JGA25-370 130RPM variant
#define CMD_TIMEOUT_MS      500     // Stop motors if no command for this long

// ============================================================================
// Standard Packet (8 bytes):
//   [START] [CMD] [P0] [P1] [P2] [P3] [CRC8] [END]
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
        // PID parameter: param_id + value as fixed-point int16 (value * 100)
        // e.g., Kp=2.50 -> value=250
        struct {
            uint8_t  param_id;  // PID_PARAM_*
            uint8_t  reserved;
            int16_t  value;     // Fixed-point: actual = value / 100.0
        } pid;
        // Debug response (compact): pwm_a, pwm_b, target_a, target_b
        struct {
            int8_t   pwm_a;     // PWM/2 (0..127)
            int8_t   pwm_b;     // PWM/2 (0..127)
            int8_t   target_a;  // Target RPM (clamped to int8 range)
            int8_t   target_b;  // Target RPM (clamped to int8 range)
        } debug;
        // Encoder counters (debug only, wrapping int16)
        struct {
            int16_t  enc_a;     // Encoder A total ticks (wrapping)
            int16_t  enc_b;     // Encoder B total ticks (wrapping)
        } encoders;
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

// Build a packet with RPM payload
static inline void proto_build(proto_packet_t *pkt, uint8_t cmd,
                                int16_t left_rpm, int16_t right_rpm) {
    pkt->start = PROTO_START_BYTE;
    pkt->cmd = cmd;
    pkt->payload.rpm.left_rpm = left_rpm;
    pkt->payload.rpm.right_rpm = right_rpm;
    pkt->crc = proto_crc8(&pkt->cmd, 5);
    pkt->end = PROTO_END_BYTE;
}

// Build a packet with raw payload bytes
static inline void proto_build_raw(proto_packet_t *pkt, uint8_t cmd,
                                    uint8_t p0, uint8_t p1, uint8_t p2, uint8_t p3) {
    pkt->start = PROTO_START_BYTE;
    pkt->cmd = cmd;
    pkt->payload.raw[0] = p0;
    pkt->payload.raw[1] = p1;
    pkt->payload.raw[2] = p2;
    pkt->payload.raw[3] = p3;
    pkt->crc = proto_crc8(&pkt->cmd, 5);
    pkt->end = PROTO_END_BYTE;
}

// Build a PID parameter packet
static inline void proto_build_pid(proto_packet_t *pkt, uint8_t cmd,
                                    uint8_t param_id, int16_t value) {
    pkt->start = PROTO_START_BYTE;
    pkt->cmd = cmd;
    pkt->payload.pid.param_id = param_id;
    pkt->payload.pid.reserved = 0;
    pkt->payload.pid.value = value;
    pkt->crc = proto_crc8(&pkt->cmd, 5);
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
