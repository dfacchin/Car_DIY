#include "avr_flash.h"
#include "config.h"
#include "protocol.h"

static WebServer *_server = nullptr;

// ============================================================================
// Intel HEX parser
// ============================================================================

// Parsed HEX record
typedef struct {
    uint8_t  byte_count;
    uint16_t address;
    uint8_t  type;      // 0x00=data, 0x01=EOF
    uint8_t  data[16];
} hex_record_t;

static bool parse_hex_line(const char *line, hex_record_t *rec) {
    if (line[0] != ':') return false;

    // Parse hex nibbles
    auto hex2 = [](const char *p) -> uint8_t {
        uint8_t hi = (p[0] >= 'A') ? (p[0] - 'A' + 10) : (p[0] >= 'a') ? (p[0] - 'a' + 10) : (p[0] - '0');
        uint8_t lo = (p[1] >= 'A') ? (p[1] - 'A' + 10) : (p[1] >= 'a') ? (p[1] - 'a' + 10) : (p[1] - '0');
        return (hi << 4) | lo;
    };

    rec->byte_count = hex2(line + 1);
    rec->address = (hex2(line + 3) << 8) | hex2(line + 5);
    rec->type = hex2(line + 7);

    uint8_t checksum = rec->byte_count + (rec->address >> 8) +
                       (rec->address & 0xFF) + rec->type;

    for (uint8_t i = 0; i < rec->byte_count && i < 16; i++) {
        rec->data[i] = hex2(line + 9 + i * 2);
        checksum += rec->data[i];
    }

    uint8_t file_checksum = hex2(line + 9 + rec->byte_count * 2);
    checksum += file_checksum;

    return (checksum == 0);  // Valid if sum of all bytes = 0
}

// ============================================================================
// STK500v1 protocol (optiboot)
// ============================================================================

#define STK_OK          0x10
#define STK_INSYNC      0x14
#define CRC_EOP         0x20    // End of packet
#define STK_GET_SYNC    0x30
#define STK_LOAD_ADDR   0x55
#define STK_PROG_PAGE   0x64
#define STK_LEAVE_PROG  0x51

static bool stk_wait_response(uint8_t expected1, uint8_t expected2, uint16_t timeout_ms = 500) {
    unsigned long start = millis();
    uint8_t state = 0;
    while ((millis() - start) < timeout_ms) {
        if (Serial.available()) {
            uint8_t b = Serial.read();
            if (state == 0 && b == expected1) state = 1;
            else if (state == 1 && b == expected2) return true;
            else return false;
        }
    }
    return false;
}

static bool stk_get_sync() {
    for (int attempt = 0; attempt < 3; attempt++) {
        // Flush input
        while (Serial.available()) Serial.read();

        Serial.write(STK_GET_SYNC);
        Serial.write(CRC_EOP);
        Serial.flush();

        if (stk_wait_response(STK_INSYNC, STK_OK, 200)) {
            return true;
        }
        delay(50);
    }
    return false;
}

static bool stk_load_address(uint16_t word_addr) {
    Serial.write(STK_LOAD_ADDR);
    Serial.write((uint8_t)(word_addr & 0xFF));
    Serial.write((uint8_t)(word_addr >> 8));
    Serial.write(CRC_EOP);
    Serial.flush();
    return stk_wait_response(STK_INSYNC, STK_OK);
}

static bool stk_prog_page(const uint8_t *data, uint16_t len) {
    Serial.write(STK_PROG_PAGE);
    Serial.write((uint8_t)(len >> 8));
    Serial.write((uint8_t)(len & 0xFF));
    Serial.write('F');  // Flash memory type
    for (uint16_t i = 0; i < len; i++) {
        Serial.write(data[i]);
    }
    Serial.write(CRC_EOP);
    Serial.flush();
    return stk_wait_response(STK_INSYNC, STK_OK, 1000);
}

static bool stk_leave_progmode() {
    Serial.write(STK_LEAVE_PROG);
    Serial.write(CRC_EOP);
    Serial.flush();
    return stk_wait_response(STK_INSYNC, STK_OK);
}

// ============================================================================
// Flash process: parse HEX, send pages via STK500v1
// ============================================================================

// Flash buffer and state (used during upload)
static uint8_t flash_buf[AVR_FLASH_SIZE];
static uint32_t flash_size = 0;
static bool flash_ok = false;
static String flash_error = "";

static void reset_avr() {
    digitalWrite(PIN_AVR_RESET, LOW);
    delay(AVR_RESET_PULSE_MS);
    digitalWrite(PIN_AVR_RESET, HIGH);
    delay(AVR_BOOT_DELAY_MS);
}

static bool do_flash() {
    // Switch UART to bootloader baud rate
    Serial.end();
    Serial.begin(AVR_BOOTLOADER_BAUD);
    delay(50);

    // Reset Arduino into bootloader
    reset_avr();

    // Sync with bootloader
    if (!stk_get_sync()) {
        flash_error = "No sync with bootloader";
        Serial.end();
        Serial.begin(PROTO_BAUD_RATE);
        return false;
    }

    // Program page by page
    uint32_t offset = 0;
    while (offset < flash_size) {
        uint16_t page_len = AVR_PAGE_SIZE;
        if (offset + page_len > flash_size) {
            page_len = flash_size - offset;
        }

        // Pad to full page with 0xFF
        uint8_t page[AVR_PAGE_SIZE];
        memset(page, 0xFF, AVR_PAGE_SIZE);
        memcpy(page, flash_buf + offset, page_len);

        // Load address (word address = byte address / 2)
        if (!stk_load_address(offset / 2)) {
            flash_error = "Load address failed at 0x" + String(offset, HEX);
            break;
        }

        // Program page
        if (!stk_prog_page(page, AVR_PAGE_SIZE)) {
            flash_error = "Page write failed at 0x" + String(offset, HEX);
            break;
        }

        offset += AVR_PAGE_SIZE;
    }

    bool success = (offset >= flash_size) && flash_error.isEmpty();

    // Leave programming mode
    stk_leave_progmode();

    // Restore normal baud rate
    Serial.end();
    Serial.begin(PROTO_BAUD_RATE);

    return success;
}

// ============================================================================
// Web upload handler
// ============================================================================

// Accumulate the uploaded .hex file content
static String hex_content = "";

static void handle_avr_upload() {
    HTTPUpload &upload = _server->upload();

    if (upload.status == UPLOAD_FILE_START) {
        hex_content = "";
        flash_size = 0;
        flash_ok = false;
        flash_error = "";
        memset(flash_buf, 0xFF, sizeof(flash_buf));
        Serial.printf("AVR: receiving %s\n", upload.filename.c_str());
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        hex_content += String((const char *)upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
        Serial.printf("AVR: parsing HEX, %u bytes\n", hex_content.length());

        // Parse Intel HEX into flash buffer
        int start = 0;
        while (start < (int)hex_content.length()) {
            int end = hex_content.indexOf('\n', start);
            if (end < 0) end = hex_content.length();

            String line = hex_content.substring(start, end);
            line.trim();
            start = end + 1;

            if (line.length() < 11) continue;

            hex_record_t rec;
            if (!parse_hex_line(line.c_str(), &rec)) {
                flash_error = "Bad HEX line";
                return;
            }

            if (rec.type == 0x01) break;  // EOF record
            if (rec.type != 0x00) continue;  // Skip non-data records

            for (uint8_t i = 0; i < rec.byte_count; i++) {
                uint32_t addr = rec.address + i;
                if (addr >= AVR_FLASH_SIZE) {
                    flash_error = "HEX exceeds flash size";
                    return;
                }
                flash_buf[addr] = rec.data[i];
                if (addr + 1 > flash_size) flash_size = addr + 1;
            }
        }

        hex_content = "";  // Free memory

        if (flash_size == 0) {
            flash_error = "Empty HEX file";
            return;
        }

        Serial.printf("AVR: parsed %u bytes, flashing...\n", flash_size);
        flash_ok = do_flash();
    }
}

static void handle_avr_result() {
    if (flash_ok) {
        char json[64];
        snprintf(json, sizeof(json), "{\"ok\":true,\"size\":%lu}", flash_size);
        _server->send(200, "application/json", json);
    } else {
        String json = "{\"ok\":false,\"error\":\"" + flash_error + "\"}";
        _server->send(500, "application/json", json);
    }
}

// ============================================================================
// Public API
// ============================================================================

void avr_flash_init() {
    pinMode(PIN_AVR_RESET, OUTPUT);
    digitalWrite(PIN_AVR_RESET, HIGH);  // Keep Arduino running (RESET is active LOW)
}

void avr_flash_register(WebServer *server) {
    _server = server;
    server->on("/api/ota/avr", HTTP_POST, handle_avr_result, handle_avr_upload);
}
