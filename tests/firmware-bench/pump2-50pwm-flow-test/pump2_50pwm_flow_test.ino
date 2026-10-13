/**
 * ============================================================================
 * LAM RESEARCH CHALLENGE 3.0 (STAGE 2) - PUMP 2 & FLOW SENSOR DIAGNOSTIC BENCH
 * Team Name: TEAM DISTRO | Team ID: LRC-26-0528
 *
 * DUAL GPIO PROBE (Monitors GPIO 14 AND GPIO 35 simultaneously)
 * Resolves schematic ambiguity:
 *   - Schematic / Wiring Guide: FLOW_SENSOR = GPIO 35
 *   - Alternative Board Trace:  FLOW_SENSOR = GPIO 14
 *
 * NO ledcWrite - Pure standard Arduino functions
 * ============================================================================
 */

#include <Arduino.h>

// ======================== PIN ASSIGNMENTS ========================
// Pump 2 Driver Pins (BTS7960 U3)
const int PIN_L_EN2  = 16;  // Pump 2 Left Enable  (Header Pin 27)
const int PIN_R_EN2  = 17;  // Pump 2 Right Enable (Header Pin 28)
const int PIN_L_PWM2 = 18;  // Pump 2 Drive Pin    (Header Pin 30)

// Flow Sensor Candidate Pins
const int PIN_FS_14  = 14;  // Candidate 1: GPIO 14 (Has internal pull-up)
const int PIN_FS_35  = 35;  // Candidate 2: GPIO 35 (Official Wiring Guide & Schematic)

// ======================== FLOW CALIBRATION CONSTANTS =============
const float CALIB_A      = 0.12926f; // Slope (L/min per Hz)
const float CALIB_B      = 0.03222f; // Offset (L/min)
const float K_FACTOR_PPL = 5880.0f;  // Pulses per Liter totalizer factor

// ======================== INTERRUPT COUNTERS =====================
volatile uint32_t pulses14 = 0;
volatile uint32_t pulses35 = 0;

void IRAM_ATTR isrPin14() { pulses14++; }
void IRAM_ATTR isrPin35() { pulses35++; }

// ======================== STATE TRACKING =========================
uint32_t lastSnapshot14   = 0;
uint32_t lastSnapshot35   = 0;
uint32_t lastReportMillis = 0;
float totalLiters         = 0.0f;
bool pumpRunning          = true;

// ======================== SIMPLE PUMP CONTROLS ===================
void pumpON(int speedVal = 128) {
    digitalWrite(PIN_L_EN2, HIGH);
    digitalWrite(PIN_R_EN2, HIGH);

    if (speedVal >= 255) {
        digitalWrite(PIN_L_PWM2, HIGH);
    } else {
        analogWrite(PIN_L_PWM2, speedVal);
    }
    pumpRunning = true;
}

void pumpOFF() {
    digitalWrite(PIN_L_PWM2, LOW);
    digitalWrite(PIN_L_EN2, LOW);
    digitalWrite(PIN_R_EN2, LOW);
    pumpRunning = false;
}

// ======================== SERIAL COMMANDS ========================
void handleSerialCommands() {
    if (!Serial.available()) return;
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    if (cmd.length() == 0) return;

    if (cmd == "on" || cmd == "1" || cmd == "100") {
        pumpON(255);
        Serial.println("\n>>> [PUMP 2] ON @ 100% Full Speed");
    } else if (cmd == "50") {
        pumpON(128);
        Serial.println("\n>>> [PUMP 2] ON @ 50% Speed (PWM ~128/255)");
    } else if (cmd == "off" || cmd == "0" || cmd == "stop") {
        pumpOFF();
        Serial.println("\n>>> [PUMP 2] OFF");
    } else if (cmd == "reset" || cmd == "r") {
        noInterrupts();
        pulses14 = 0;
        pulses35 = 0;
        interrupts();
        lastSnapshot14 = 0;
        lastSnapshot35 = 0;
        totalLiters = 0.0f;
        Serial.println("\n>>> [RESET] Counters zeroed.");
    }
}

// ======================== SETUP ==================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n============================================================");
    Serial.println("  TEAM DISTRO - FLOW SENSOR & PUMP 2 DUAL-PIN DIAGNOSTIC    ");
    Serial.println("============================================================");
    Serial.println("  Checking both candidate GPIOs simultaneously:");
    Serial.println("   -> GPIO 14 (Internal Pull-Up enabled)");
    Serial.println("   -> GPIO 35 (Schematic/Wiring Guide Flow Pin)");
    Serial.println("------------------------------------------------------------");
    Serial.println("  Commands: '50' (50%), 'on' (100%), 'off' (stop), 'reset'");
    Serial.println("============================================================\n");

    // Configure Pump Pins
    pinMode(PIN_L_EN2, OUTPUT);
    pinMode(PIN_R_EN2, OUTPUT);
    pinMode(PIN_L_PWM2, OUTPUT);

    // Attach interrupts on BOTH GPIO 14 and GPIO 35
    pinMode(PIN_FS_14, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_FS_14), isrPin14, CHANGE);

    // Note: GPIO 35 is input-only (requires external pull-up resistor on PCB or breadboard)
    pinMode(PIN_FS_35, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_FS_35), isrPin35, CHANGE);

    // Start Pump 2 at 50% speed
    Serial.println("[STARTUP] Turning Pump 2 ON at 50% speed...");
    pumpON(128);

    lastReportMillis = millis();
}

// ======================== MAIN LOOP ==============================
void loop() {
    handleSerialCommands();

    uint32_t now = millis();
    if (now - lastReportMillis >= 500) {
        uint32_t dtMs = now - lastReportMillis;
        lastReportMillis = now;

        // Atomically snapshot pulse counts
        noInterrupts();
        uint32_t p14 = pulses14;
        uint32_t p35 = pulses35;
        interrupts();

        uint32_t d14 = p14 - lastSnapshot14;
        uint32_t d35 = p35 - lastSnapshot35;
        lastSnapshot14 = p14;
        lastSnapshot35 = p35;

        // Read instantaneous pin digital levels (0 or 1)
        int lvl14 = digitalRead(PIN_FS_14);
        int lvl35 = digitalRead(PIN_FS_35);

        // Determine active pin
        uint32_t activeDelta = 0;
        const char* activePin = "NONE (0 Pulses)";

        if (d35 > 0) {
            activeDelta = d35;
            activePin = "GPIO 35 (ACTIVE!)";
        } else if (d14 > 0) {
            activeDelta = d14;
            activePin = "GPIO 14 (ACTIVE!)";
        }

        // Calculate flow rate
        // Note: CHANGE interrupt catches 2 edges per cycle, so frequency = (pulses / 2) / time
        float freqHz = ((float)activeDelta / 2.0f * 1000.0f) / (float)dtMs;
        float flowLpm = 0.0f;
        if (freqHz > 0.5f) {
            flowLpm = (CALIB_A * freqHz) + CALIB_B;
        }
        float flowMls = (flowLpm * 1000.0f) / 60.0f;
        totalLiters += ((float)activeDelta / 2.0f / K_FACTOR_PPL);

        // Live diagnostic log
        Serial.printf("[P2:%s] Pin Levels:[G14=%d, G35=%d] | Pulses:[G14=%2lu, G35=%2lu] -> Sensor on %-17s | Flow: %4.2f L/min (%5.1f mL/s)\n",
                      pumpRunning ? "50%" : "OFF",
                      lvl14, lvl35,
                      (unsigned long)d14, (unsigned long)d35,
                      activePin,
                      flowLpm, flowMls);
    }
}
