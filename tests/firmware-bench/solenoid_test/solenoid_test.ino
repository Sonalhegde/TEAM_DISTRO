/*
 * ============================================================================
 * LAM RESEARCH CHALLENGE 3.0 - STAGE 2
 * ESP32 Standalone Hardware Bench Test: Solenoid Valve OV-01 Driver
 *
 * NOTE: This is a standalone Arduino IDE diagnostic sketch (Arduino-ESP32 Core 3.x),
 * not part of the ESP-IDF production firmware build.
 *
 * Pin assignments match firmware/main/config.h (LAM Stage 2 PCB Rev 1.0):
 *   OV-01 Driver -> GPIO 27 (Active LOW: driven LOW 0V to energize coil;
 *                            released to input/high-Z to de-energize coil)
 * ============================================================================
 */

#include <Arduino.h>

static const int PIN_SV = 27;

static bool activeLow = true;
static bool autoCycle = true;
static unsigned long lastToggleMs = 0;
static bool valveOpen = false;

void setSolenoid(bool open) {
    valveOpen = open;
    if (activeLow) {
        if (open) {
            pinMode(PIN_SV, OUTPUT);
            digitalWrite(PIN_SV, LOW);
        } else {
            pinMode(PIN_SV, INPUT); // High-Z release lets 5V relay module pull up safely
        }
    } else {
        pinMode(PIN_SV, OUTPUT);
        digitalWrite(PIN_SV, open ? HIGH : LOW);
    }

    Serial.printf("[SOLENOID] OV-01 (GPIO %d) -> %s\n", PIN_SV, valveOpen ? "OPEN / ENERGIZED" : "CLOSED / DE-ENERGIZED");
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("==================================================");
    Serial.println("  SOLENOID VALVE OV-01 HARDWARE BENCH TEST");
    Serial.println("  Pin: GPIO 27 (Active LOW / High-Z Release)");
    Serial.println("==================================================");
    Serial.println("Commands: 1 (open), 0 (close), auto (cycle 3s), inv (invert polarity)");

    setSolenoid(false);
    lastToggleMs = millis();
}

void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toLowerCase();

        if (cmd == "1" || cmd == "open" || cmd == "on") {
            autoCycle = false;
            setSolenoid(true);
        } else if (cmd == "0" || cmd == "close" || cmd == "off") {
            autoCycle = false;
            setSolenoid(false);
        } else if (cmd == "inv") {
            activeLow = !activeLow;
            Serial.printf("[SOLENOID] Polarity changed: Active %s\n", activeLow ? "LOW" : "HIGH");
            setSolenoid(valveOpen);
        } else if (cmd == "auto") {
            autoCycle = true;
            Serial.println("[SOLENOID] Auto 3-second cycle resumed.");
        }
    }

    if (autoCycle && (millis() - lastToggleMs >= 3000)) {
        lastToggleMs = millis();
        setSolenoid(!valveOpen);
    }
}
