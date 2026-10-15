/*
 * ============================================================================
 * LAM RESEARCH CHALLENGE 3.0 - STAGE 2
 * ESP32 Standalone Hardware Bench Test: Pump 1 BTS7960 Full-Capacity PWM Test
 *
 * NOTE: This is a standalone Arduino IDE diagnostic sketch (Arduino-ESP32 Core 3.x),
 * not part of the ESP-IDF production firmware build.
 *
 * Pin assignments match firmware/main/config.h (LAM Stage 2 PCB Rev 1.0):
 *   L_EN1     -> GPIO 13 (and GPIO 12 tied on PCB)
 *   R_EN1     -> GPIO 26
 *   L_PWM1    -> GPIO 32
 *   R_PWM1    -> GPIO 33
 *
 * Operation:
 *   Runs Pump 1 at 100% duty cycle for 20 s, rests for 3 s, repeats indefinitely.
 * ============================================================================
 */

#include <Arduino.h>

static const int PIN_L_EN1    = 13;
static const int PIN_L_EN1_ALT= 12; // Tied secondary enable on PCB
static const int PIN_R_EN1    = 26;
static const int PIN_L_PWM1   = 32;
static const int PIN_R_PWM1   = 33;

static const uint32_t PWM_FREQ_HZ = 5000;
static const uint8_t  PWM_BITS    = 8;
static const uint32_t MOTOR_ON_MS  = 20000;
static const uint32_t MOTOR_OFF_MS = 3000;

void pumpOn() {
    digitalWrite(PIN_L_EN1, HIGH);
    digitalWrite(PIN_L_EN1_ALT, HIGH);
    digitalWrite(PIN_R_EN1, HIGH);
    ledcWrite(PIN_L_PWM1, 255);
    digitalWrite(PIN_R_PWM1, LOW);
    Serial.println("[PUMP 1] ON @ 100% duty (GPIO 32 PWM, L_EN=13&12, R_EN=26)");
}

void pumpOff() {
    ledcWrite(PIN_L_PWM1, 0);
    digitalWrite(PIN_L_EN1, LOW);
    digitalWrite(PIN_L_EN1_ALT, LOW);
    digitalWrite(PIN_R_EN1, LOW);
    digitalWrite(PIN_R_PWM1, LOW);
    Serial.println("[PUMP 1] OFF");
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("==================================================");
    Serial.println("  PUMP 1 STANDALONE BENCH TEST (Core 3.x)");
    Serial.println("==================================================");

    pinMode(PIN_L_EN1, OUTPUT);
    pinMode(PIN_L_EN1_ALT, OUTPUT);
    pinMode(PIN_R_EN1, OUTPUT);
    pinMode(PIN_R_PWM1, OUTPUT);

    ledcAttach(PIN_L_PWM1, PWM_FREQ_HZ, PWM_BITS);

    pumpOff();
    delay(100);
    pumpOn();
}

void loop() {
    delay(MOTOR_ON_MS);
    pumpOff();
    delay(MOTOR_OFF_MS);
    pumpOn();
}
