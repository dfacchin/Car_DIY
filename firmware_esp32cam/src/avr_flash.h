#ifndef AVR_FLASH_H
#define AVR_FLASH_H

#include <Arduino.h>
#include <WebServer.h>

// Initialize AVR flashing (sets up RESET pin)
void avr_flash_init();

// Register AVR flash upload endpoint on the given web server
void avr_flash_register(WebServer *server);

// Flash Arduino from a hex file on LittleFS. Returns true on success.
bool avr_flash_from_file(const char *path);

// Verify Arduino flash matches hex file on LittleFS. Returns true if identical.
// Resets Arduino into bootloader, reads flash pages, compares byte-by-byte.
bool avr_verify_from_file(const char *path);

// Flash only if verify fails. Returns true if already up-to-date or flash succeeded.
bool avr_flash_if_needed(const char *path);

// Get last flash error message
const String &avr_flash_last_error();

#endif // AVR_FLASH_H
