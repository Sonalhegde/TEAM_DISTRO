/**
 * ============================================================================
 * LAM RESEARCH CHALLENGE 3.0 (STAGE 2) - SOLENOID VALVE (OV-01) HARDWARE BENCH TEST
 * Team Name: TEAM DISTRO | Team ID: LRC-26-0528
 *
 * Use this sketch to isolate and diagnose why the Solenoid Valve is not clicking
 * on the PCB even though it works on direct 12V supply.
 * ============================================================================
 */

#include <Arduino.h>

const int PIN_SV_27 = 27; // PCB Rev 1.0 schematic pin (Pin 12 on header)
const int PIN_SV_25 = 25; // Alternate pin in wiring guide

bool activeHigh = true;   // Default: Active HIGH (3.3V gate = ON)
bool autoCycle  = true;   // Automatically toggles every 3 seconds

unsigned long lastToggleMs = 0;
bool valveState = false;

void setValve(bool state) {
    valveState = state;
    uint8_t lvl = activeHigh ? (state ? HIGH : LOW) : (state ? LOW : HIGH);

    digitalWrite(PIN_SV_27, lvl);
    digitalWrite(PIN_SV_25, lvl);

    Serial.printf("\n[SOLENOID] -> %s | Driving GPIO 27 & 25 to %s (%s)\n",
                  valveState ? "ENERGIZED (OPEN)" : "DE-ENERGIZED (CLOSED)",
                  lvl == HIGH ? "HIGH (3.3V)" : "LOW (0V)",
                  activeHigh ? "Active HIGH" : "Active LOW");
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(PIN_SV_27, OUTPUT);
    pinMode(PIN_SV_25, OUTPUT);

    Serial.println("\n============================================================");
    Serial.println("     LAM STAGE 2 - SOLENOID VALVE (OV-01) DIAGNOSTIC BENCH  ");
    Serial.println("============================================================");
    Serial.println("  1. DRIVING BOTH GPIO 27 & GPIO 25 simultaneously.");
    Serial.println("  2. Automatically toggling every 3 seconds (ON <-> OFF).");
    Serial.println("------------------------------------------------------------");
    Serial.println("  HOW TO WIRE THE 2-WIRE SOLENOID TO THE 3-PIN PCB HEADER (S/V/G):");
    Serial.println("    -> Solenoid Wire 1: Connect to 'V' (+12V DC)");
    Serial.println("    -> Solenoid Wire 2: Connect to 'S' (Switched Ground/Drain)");
    Serial.println("    -> DO NOT CONNECT TO 'G'! Connecting between S and G gives 0V!");
    Serial.println("------------------------------------------------------------");
    Serial.println("  MULTIMETER CHECK:");
    Serial.println("    1. Put meter on DC Volts.");
    Serial.println("    2. Red probe on 'V', Black probe on 'S'.");
    Serial.println("    3. When ON: you should measure ~12V DC.");
    Serial.println("    4. When OFF: you should measure 0V DC.");
    Serial.println("------------------------------------------------------------");
    Serial.println("  SERIAL COMMANDS:");
    Serial.println("    '1'    -> Force Solenoid ON");
    Serial.println("    '0'    -> Force Solenoid OFF");
    Serial.println("    'inv'  -> Invert polarity (test Active-LOW vs Active-HIGH)");
    Serial.println("    'auto' -> Resume automatic 3-second cycle");
    Serial.println("============================================================\n");

    setValve(false);
    lastToggleMs = millis();
}

void loop() {
    // Process serial commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toLowerCase();

        if (cmd == "1" || cmd == "on") {
            autoCycle = false;
            setValve(true);
        } else if (cmd == "0" || cmd == "off") {
            autoCycle = false;
            setValve(false);
        } else if (cmd == "inv") {
            activeHigh = !activeHigh;
            Serial.printf(">>> Polarity switched to Active %s\n", activeHigh ? "HIGH" : "LOW");
            setValve(valveState);
        } else if (cmd == "auto") {
            autoCycle = true;
            Serial.println(">>> Auto 3-second cycle resumed.");
        }
    }

    // Auto cycle
    if (autoCycle && (millis() - lastToggleMs >= 3000)) {
        lastToggleMs = millis();
        setValve(!valveState);
    }
}
