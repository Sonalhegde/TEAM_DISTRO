// Standalone bench test: Solenoid Valve OV-01 driver
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
            pinMode(PIN_SV, INPUT); // High-Z release lets external relay pull-up close valve safely
        }
    } else {
        pinMode(PIN_SV, OUTPUT);
        digitalWrite(PIN_SV, open ? HIGH : LOW);
    }

    Serial.printf("[SOLENOID] OV-01 (GPIO %d) -> %s\n", PIN_SV, valveOpen ? "OPEN" : "CLOSED");
}

void setup() {
    Serial.begin(115200);
    delay(500);

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
            Serial.printf("[SOLENOID] Polarity: Active %s\n", activeLow ? "LOW" : "HIGH");
            setSolenoid(valveOpen);
        } else if (cmd == "auto") {
            autoCycle = true;
            Serial.println("[SOLENOID] Auto 3s cycle active.");
        }
    }

    if (autoCycle && (millis() - lastToggleMs >= 3000)) {
        lastToggleMs = millis();
        setSolenoid(!valveOpen);
    }
}
