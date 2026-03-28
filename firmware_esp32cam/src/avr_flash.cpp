#include "avr_flash.h"
#include "config.h"
#include "protocol.h"
#include <LittleFS.h>

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
#define STK_READ_PAGE   0x74
#define STK_LEAVE_PROG  0x51

static bool stk_wait_response(uint8_t expected1, uint8_t expected2, uint16_t timeout_ms = 500) {
    unsigned long start = millis();
    uint8_t state = 0;
    while ((millis() - start) < timeout_ms) {
        if (Serial2.available()) {
            uint8_t b = Serial2.read();
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
        while (Serial2.available()) Serial2.read();

        Serial2.write(STK_GET_SYNC);
        Serial2.write(CRC_EOP);
        Serial2.flush();

        if (stk_wait_response(STK_INSYNC, STK_OK, 200)) {
            return true;
        }
        delay(50);
    }
    return false;
}

static bool stk_load_address(uint16_t word_addr) {
    Serial2.write(STK_LOAD_ADDR);
    Serial2.write((uint8_t)(word_addr & 0xFF));
    Serial2.write((uint8_t)(word_addr >> 8));
    Serial2.write(CRC_EOP);
    Serial2.flush();
    return stk_wait_response(STK_INSYNC, STK_OK);
}

static bool stk_prog_page(const uint8_t *data, uint16_t len) {
    Serial2.write(STK_PROG_PAGE);
    Serial2.write((uint8_t)(len >> 8));
    Serial2.write((uint8_t)(len & 0xFF));
    Serial2.write('F');  // Flash memory type
    for (uint16_t i = 0; i < len; i++) {
        Serial2.write(data[i]);
    }
    Serial2.write(CRC_EOP);
    Serial2.flush();
    return stk_wait_response(STK_INSYNC, STK_OK, 1000);
}

static bool stk_read_page(uint8_t *buf, uint16_t len) {
    Serial2.write(STK_READ_PAGE);
    Serial2.write((uint8_t)(len >> 8));
    Serial2.write((uint8_t)(len & 0xFF));
    Serial2.write('F');  // Flash memory type
    Serial2.write(CRC_EOP);
    Serial2.flush();

    // Expect: STK_INSYNC, then len data bytes, then STK_OK
    unsigned long start = millis();
    // Wait for INSYNC
    while ((millis() - start) < 1000) {
        if (Serial2.available()) {
            if (Serial2.read() == STK_INSYNC) break;
            else return false;
        }
    }
    if ((millis() - start) >= 1000) return false;

    // Read data bytes
    uint16_t received = 0;
    while (received < len && (millis() - start) < 2000) {
        if (Serial2.available()) {
            buf[received++] = Serial2.read();
        }
    }
    if (received < len) return false;

    // Wait for STK_OK
    while ((millis() - start) < 2000) {
        if (Serial2.available()) {
            return Serial2.read() == STK_OK;
        }
    }
    return false;
}

static bool stk_leave_progmode() {
    Serial2.write(STK_LEAVE_PROG);
    Serial2.write(CRC_EOP);
    Serial2.flush();
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
    pinMode(PIN_AVR_RESET, OUTPUT);
    digitalWrite(PIN_AVR_RESET, LOW);
    delay(AVR_RESET_PULSE_MS);
    pinMode(PIN_AVR_RESET, INPUT);  // Release — Arduino pull-up restores HIGH
    delay(AVR_BOOT_DELAY_MS);
}

static bool do_flash() {
    // Switch motor UART to bootloader baud rate (57600 for Pro Mini 3.3V 8MHz)
    Serial2.end();
    Serial2.begin(AVR_BOOTLOADER_BAUD, SERIAL_8N1, MOTOR_UART_RX, MOTOR_UART_TX);
    delay(50);
    Serial.println("[AVR] Switching to bootloader baud, resetting Arduino...");

    // Reset Arduino into bootloader
    reset_avr();

    // Sync with bootloader
    if (!stk_get_sync()) {
        flash_error = "No sync with bootloader";
        Serial.println("[AVR] No sync!");
        Serial2.end();
        Serial2.begin(PROTO_BAUD_RATE, SERIAL_8N1, MOTOR_UART_RX, MOTOR_UART_TX);
        return false;
    }
    Serial.println("[AVR] Bootloader sync OK");

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

    // Restore motor UART baud rate
    Serial2.end();
    Serial2.begin(PROTO_BAUD_RATE, SERIAL_8N1, MOTOR_UART_RX, MOTOR_UART_TX);
    Serial.printf("[AVR] Flash %s\n", success ? "OK" : "FAILED");

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
    // Keep as INPUT — Arduino's 10K pull-up holds RESET HIGH.
    // Only drive OUTPUT LOW when we need to reset for programming.
    pinMode(PIN_AVR_RESET, INPUT);
}

void avr_flash_register(WebServer *server) {
    _server = server;
    server->on("/api/ota/avr", HTTP_POST, handle_avr_result, handle_avr_upload);
}

bool avr_flash_from_file(const char *path) {
    File f = LittleFS.open(path, "r");
    if (!f) {
        flash_error = "File not found: " + String(path);
        Serial.printf("[AVR-AUTO] %s\n", flash_error.c_str());
        return false;
    }

    // Reset state
    flash_size = 0;
    flash_ok = false;
    flash_error = "";
    memset(flash_buf, 0xFF, sizeof(flash_buf));

    Serial.printf("[AVR-AUTO] Reading %s (%u bytes)\n", path, f.size());

    // Parse hex line by line
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() < 11) continue;

        hex_record_t rec;
        if (!parse_hex_line(line.c_str(), &rec)) {
            flash_error = "Bad HEX line";
            f.close();
            return false;
        }

        if (rec.type == 0x01) break;  // EOF
        if (rec.type != 0x00) continue;

        for (uint8_t i = 0; i < rec.byte_count; i++) {
            uint32_t addr = rec.address + i;
            if (addr >= AVR_FLASH_SIZE) {
                flash_error = "HEX exceeds flash size";
                f.close();
                return false;
            }
            flash_buf[addr] = rec.data[i];
            if (addr + 1 > flash_size) flash_size = addr + 1;
        }
    }
    f.close();

    if (flash_size == 0) {
        flash_error = "Empty HEX file";
        return false;
    }

    Serial.printf("[AVR-AUTO] Parsed %u bytes, flashing...\n", flash_size);
    flash_ok = do_flash();
    return flash_ok;
}

const String &avr_flash_last_error() {
    return flash_error;
}

bool avr_verify_from_file(const char *path) {
    // Parse hex file into flash_buf
    File f = LittleFS.open(path, "r");
    if (!f) {
        flash_error = "File not found: " + String(path);
        return false;
    }

    flash_size = 0;
    flash_error = "";
    memset(flash_buf, 0xFF, sizeof(flash_buf));

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() < 11) continue;

        hex_record_t rec;
        if (!parse_hex_line(line.c_str(), &rec)) { f.close(); flash_error = "Bad HEX"; return false; }
        if (rec.type == 0x01) break;
        if (rec.type != 0x00) continue;

        for (uint8_t i = 0; i < rec.byte_count; i++) {
            uint32_t addr = rec.address + i;
            if (addr >= AVR_FLASH_SIZE) { f.close(); flash_error = "HEX too large"; return false; }
            flash_buf[addr] = rec.data[i];
            if (addr + 1 > flash_size) flash_size = addr + 1;
        }
    }
    f.close();

    if (flash_size == 0) { flash_error = "Empty HEX"; return false; }

    // Enter bootloader
    Serial2.end();
    Serial2.begin(AVR_BOOTLOADER_BAUD, SERIAL_8N1, MOTOR_UART_RX, MOTOR_UART_TX);
    delay(50);

    reset_avr();

    if (!stk_get_sync()) {
        flash_error = "No sync for verify";
        Serial2.end();
        Serial2.begin(PROTO_BAUD_RATE, SERIAL_8N1, MOTOR_UART_RX, MOTOR_UART_TX);
        return false;
    }

    // Read and compare page by page
    bool match = true;
    uint32_t offset = 0;
    uint8_t read_buf[AVR_PAGE_SIZE];

    while (offset < flash_size) {
        uint16_t page_len = AVR_PAGE_SIZE;
        if (offset + page_len > flash_size) page_len = flash_size - offset;

        if (!stk_load_address(offset / 2)) {
            flash_error = "Verify: load address failed";
            match = false;
            break;
        }

        if (!stk_read_page(read_buf, AVR_PAGE_SIZE)) {
            flash_error = "Verify: read page failed";
            match = false;
            break;
        }

        if (memcmp(read_buf, flash_buf + offset, page_len) != 0) {
            Serial.printf("[VERIFY] Mismatch at offset 0x%04X\n", offset);
            match = false;
            break;
        }

        offset += AVR_PAGE_SIZE;
    }

    stk_leave_progmode();

    // Restore normal baud
    Serial2.end();
    Serial2.begin(PROTO_BAUD_RATE, SERIAL_8N1, MOTOR_UART_RX, MOTOR_UART_TX);

    return match;
}

bool avr_flash_if_needed(const char *path) {
    Serial.println("[AVR] Verifying Arduino flash against stored hex...");

    if (avr_verify_from_file(path)) {
        Serial.println("[AVR] Flash matches — no update needed.");
        return true;
    }

    Serial.printf("[AVR] Mismatch or verify failed (%s) — flashing...\n", flash_error.c_str());
    return avr_flash_from_file(path);
}
