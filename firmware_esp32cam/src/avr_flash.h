#ifndef AVR_FLASH_H
#define AVR_FLASH_H

#include <Arduino.h>
#include <WebServer.h>

// Initialize AVR flashing (sets up RESET pin)
void avr_flash_init();

// Register AVR flash upload endpoint on the given web server
void avr_flash_register(WebServer *server);

#endif // AVR_FLASH_H
