// Minimal echo test: any byte received is sent back immediately
#include <Arduino.h>

void setup() {
    pinMode(13, OUTPUT);
    Serial.begin(38400);
    digitalWrite(13, HIGH);
    delay(200);
    digitalWrite(13, LOW);
}

void loop() {
    if (Serial.available()) {
        uint8_t b = Serial.read();
        Serial.write(b);
        // Blink LED on each received byte
        digitalWrite(13, HIGH);
        delay(10);
        digitalWrite(13, LOW);
    }
}
