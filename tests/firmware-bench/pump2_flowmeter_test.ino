/**
 * ============================================================================
 * ESP32-WROOM STANDALONE TEST: MOTOR 2 + FT-01 FLOW TRANSMITTER
 *
 * Board:  ESP32 Dev Module (or NodeMCU-32S) in Arduino IDE
 * Baud:   115200
 *
 * PWM RULE (strictly applied):
 *   - R_PWM2 trace is MISSING on PCB -> left completely UNTOUCHED.
 *   - L_PWM2 (GPIO 18) ALONE drives Pump 2.
 *   - L_EN2 (GPIO 16) + R_EN2 (GPIO 17) both asserted HIGH to enable BTS7960.
 *
 * FLOW SENSOR PIN (confirmed from schematic):
 *   - FS (Flow Sensor signal) -> G14/CLK label on NodeMCU-32S
 *   - Physical pin 12 on the header = GPIO 14
 *   - Open-collector output: needs INPUT_PULLUP, trigger on FALLING edge.
 * ============================================================================
 */

#include <Arduino.h>

// ======================== PIN ASSIGNMENTS ========================

// Pump 2 - BTS7960 Driver (U3)
const int PIN_L_EN2  = 16;  // L_EN2  -> NodeMCU-32S header pin 27
const int PIN_R_EN2  = 17;  // R_EN2  -> NodeMCU-32S header pin 28
const int PIN_L_PWM2 = 18;  // L_PWM2 -> NodeMCU-32S header pin 30
// R_PWM2 -> PCB trace absent -> NOT assigned, NOT touched.

// FT-01 Flow Transmitter
// Schematic label: FS -> G14/CLK -> Physical header pin 12 -> GPIO 14
const int PIN_FS = 14;

// ======================== PWM CONFIGURATION ======================

const uint32_t PWM_FREQ = 20000; // 20 kHz (ultrasonic, BTS7960 rated)
const uint8_t  PWM_BITS = 8;     // 8-bit resolution (0-255)

// Arduino-ESP32 Core 3.x: ledcAttach/ledcWrite use GPIO pin directly (no channel index)
// Arduino-ESP32 Core 2.x: ledcAttachPin + ledcSetup needed
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  #define PWM_ATTACH(pin)    ledcAttach(pin, PWM_FREQ, PWM_BITS)
  #define PWM_WRITE(pin, v)  ledcWrite(pin, v)
#else
  static uint8_t pwmCh = 0;
  #define PWM_ATTACH(pin)    do { ledcSetup(pwmCh, PWM_FREQ, PWM_BITS); ledcAttachPin(pin, pwmCh); } while(0)
  #define PWM_WRITE(pin, v)  ledcWrite(pwmCh, v)
#endif

// ======================== FLOW METER STATE =======================

// YF-S401 factory K-factor: 5880 pulses/L
// Adjust this value after running a measured calibration run.
float ppl = 5880.0f;

volatile uint32_t pulseCount = 0; // ISR-incremented pulse counter
uint32_t lastPulseCount = 0;
uint32_t lastPrintMs    = 0;
float    totalL         = 0.0f;

// ======================== MOTOR STATE ============================

float currentDutyPct = 100.0f;

// ======================== ISR ====================================

void IRAM_ATTR isrFlow() {
    pulseCount = pulseCount + 1; // avoids volatile++ compiler warning
}

// ======================== PUMP 2 CONTROL =========================

void setPump2(float dutyPct) {
    currentDutyPct = constrain(dutyPct, 0.0f, 100.0f);
    uint32_t pwmVal = (uint32_t)(currentDutyPct * 255.0f / 100.0f);

    if (pwmVal > 0) {
        digitalWrite(PIN_L_EN2, HIGH);
        digitalWrite(PIN_R_EN2, HIGH);
        PWM_WRITE(PIN_L_PWM2, pwmVal); // Only L_PWM2 driven; R_PWM2 left alone
    } else {
        PWM_WRITE(PIN_L_PWM2, 0);
        digitalWrite(PIN_L_EN2, LOW);
        digitalWrite(PIN_R_EN2, LOW);
    }
}

// ======================== SERIAL CLI =============================

void handleSerialInput() {
    if (!Serial.available()) return;
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    if (cmd.length() == 0) return;

    Serial.printf("[CMD] '%s'\n", cmd.c_str());

    if (cmd == "stop" || cmd == "0") {
        setPump2(0.0f);
        Serial.println("[CMD] Motor 2 STOPPED.");

    } else if (cmd == "reset" || cmd == "rt") {
        noInterrupts();
        pulseCount = 0;
        interrupts();
        lastPulseCount = 0;
        totalL = 0.0f;
        Serial.println("[CMD] Pulse counter and totalizer reset.");

    } else if (cmd.startsWith("ppl ")) {
        float v = cmd.substring(4).toFloat();
        if (v > 10.0f) {
            ppl = v;
            Serial.printf("[CMD] K-factor set to %.1f pulses/L.\n", ppl);
        }

    } else {
        float val = cmd.toFloat();
        if ((val > 0.0f && val <= 100.0f) || cmd == "0") {
            setPump2(val);
            Serial.printf("[CMD] Motor 2 -> %.1f%% duty cycle.\n", val);
        } else {
            Serial.println("---- COMMANDS ----");
            Serial.println("  0-100       : Set duty % (e.g. '50', '100', '0')");
            Serial.println("  stop        : Stop motor");
            Serial.println("  reset / rt  : Reset pulse counter & total");
            Serial.println("  ppl <value> : Set K-factor (e.g. ppl 5880)");
        }
    }
}

// ======================== SETUP ==================================

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("======================================================");
    Serial.println("  PUMP 2 + FT-01 FLOW TRANSMITTER STANDALONE TEST");
    Serial.println("  L_PWM2  -> GPIO 18  |  L_EN2 -> GPIO 16  |  R_EN2 -> GPIO 17");
    Serial.println("  R_PWM2  -> UNTOUCHED (PCB trace missing)");
    Serial.println("  FS      -> GPIO 14  (G14/CLK, physical pin 12 on NodeMCU-32S)");
    Serial.println("======================================================");

    // Pump 2 pins
    pinMode(PIN_L_EN2, OUTPUT);
    pinMode(PIN_R_EN2, OUTPUT);
    digitalWrite(PIN_L_EN2, LOW);
    digitalWrite(PIN_R_EN2, LOW);
    PWM_ATTACH(PIN_L_PWM2);
    PWM_WRITE(PIN_L_PWM2, 0);

    // Flow sensor: GPIO 14, open-collector -> INPUT_PULLUP, FALLING edge
    pinMode(PIN_FS, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_FS), isrFlow, CHANGE); // CHANGE catches both RISING and FALLING

    Serial.println("[INIT] Flow sensor interrupt attached on GPIO 14 (FALLING).");
    Serial.println("[INIT] Starting Pump 2 at 100% duty on L_PWM2 (GPIO 18)...");
    setPump2(100.0f);

    lastPrintMs = millis();
}

// ======================== LOOP ===================================

void loop() {
    handleSerialInput();

    uint32_t now = millis();
    if (now - lastPrintMs >= 500) {
        uint32_t dt = now - lastPrintMs;
        lastPrintMs  = now;

        // Atomic snapshot
        noInterrupts();
        uint32_t p = pulseCount;
        interrupts();

        uint32_t delta = p - lastPulseCount;
        lastPulseCount = p;

        float freqHz  = (float)delta * 1000.0f / (float)dt;
        float flowLpm = freqHz * 60.0f / ppl;
        float flowMls = flowLpm * 1000.0f / 60.0f;
        totalL       += (float)delta / ppl;

        int gpio14_level = digitalRead(PIN_FS);

        Serial.printf(
            "[P2:%3.0f%%] GPIO14=%d | +pulses=%4lu | Total=%7lu | "
            "Freq=%6.1fHz | Flow=%5.3f L/min (%4.1f mL/s) | Total=%.4f L\n",
            currentDutyPct,
            gpio14_level,
            (unsigned long)delta,
            (unsigned long)p,
            freqHz,
            flowLpm, flowMls,
            totalL
        );
    }

    delay(2); // Yield to FreeRTOS IDLE
}
