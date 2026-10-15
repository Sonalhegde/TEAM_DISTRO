// Standalone bench test: Pump 2 and FT-01 pulse flow sensor
#include <Arduino.h>

static const int PIN_L_EN2  = 16;
static const int PIN_R_EN2  = 17;
static const int PIN_L_PWM2 = 18;
static const int PIN_R_PWM2 = 19;
static const int PIN_FS     = 14;

static const uint32_t PWM_FREQ_HZ = 5000;
static const uint8_t  PWM_BITS    = 8;

// Calibrated relationship: Q = CAL_A * f + CAL_B
static const double CAL_A = 0.009244;
static const double CAL_B = 0.040111;

static volatile uint32_t isrPulseCount = 0;
static uint32_t lastPulseCount = 0;
static uint32_t lastPrintMs    = 0;
static float    currentDuty    = 100.0f;
static double   totalVolumeL   = 0.0;

void IRAM_ATTR isrFlowSensor() {
    isrPulseCount++;
}

void setPump2Duty(float dutyPct) {
    currentDuty = constrain(dutyPct, 0.0f, 100.0f);
    uint32_t pwmVal = (uint32_t)(currentDuty * 255.0f / 100.0f);

    if (pwmVal > 0) {
        digitalWrite(PIN_L_EN2, HIGH);
        digitalWrite(PIN_R_EN2, HIGH);
        ledcWrite(PIN_L_PWM2, pwmVal);
        digitalWrite(PIN_R_PWM2, LOW);
    } else {
        ledcWrite(PIN_L_PWM2, 0);
        digitalWrite(PIN_L_EN2, LOW);
        digitalWrite(PIN_R_EN2, LOW);
        digitalWrite(PIN_R_PWM2, LOW);
    }
}

void handleSerial() {
    if (!Serial.available()) return;
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    if (cmd.length() == 0) return;

    if (cmd == "stop" || cmd == "0") {
        setPump2Duty(0.0f);
        Serial.println("[CMD] Pump 2 STOPPED.");
    } else if (cmd == "reset" || cmd == "rt") {
        noInterrupts();
        isrPulseCount = 0;
        interrupts();
        lastPulseCount = 0;
        totalVolumeL = 0.0;
        Serial.println("[CMD] Totalizers reset.");
    } else {
        float val = cmd.toFloat();
        if (val > 0.0f && val <= 100.0f) {
            setPump2Duty(val);
            Serial.printf("[CMD] Pump 2 set to %.1f%% duty.\n", val);
        } else {
            Serial.println("Commands: 0-100 (duty), stop, reset");
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    pinMode(PIN_L_EN2, OUTPUT);
    pinMode(PIN_R_EN2, OUTPUT);
    pinMode(PIN_R_PWM2, OUTPUT);
    digitalWrite(PIN_L_EN2, LOW);
    digitalWrite(PIN_R_EN2, LOW);
    digitalWrite(PIN_R_PWM2, LOW);

    ledcAttach(PIN_L_PWM2, PWM_FREQ_HZ, PWM_BITS);

    pinMode(PIN_FS, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_FS), isrFlowSensor, RISING);

    setPump2Duty(100.0f);
    lastPrintMs = millis();
}

void loop() {
    handleSerial();

    uint32_t now = millis();
    if (now - lastPrintMs >= 1000) {
        uint32_t dt_ms = now - lastPrintMs;
        lastPrintMs = now;

        noInterrupts();
        uint32_t p = isrPulseCount;
        interrupts();

        uint32_t delta = p - lastPulseCount;
        lastPulseCount = p;

        double f_Hz = (double)delta * 1000.0 / (double)dt_ms;
        double Q_Lmin = (f_Hz < 1.0) ? 0.0 : ((CAL_A * f_Hz) + CAL_B);
        totalVolumeL += (Q_Lmin / 60.0) * ((double)dt_ms / 1000.0);

        Serial.printf("[P2:%3.0f%%] Pulses: +%4lu | Total: %7lu | Freq: %6.1f Hz | Flow: %5.3f L/min | Volume: %.4f L\n",
                      currentDuty, (unsigned long)delta, (unsigned long)p, f_Hz, Q_Lmin, totalVolumeL);
    }

    delay(2);
}
