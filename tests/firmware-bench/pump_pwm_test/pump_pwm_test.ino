// Standalone bench test: Pump 1 BTS7960 PWM cycling
#include <Arduino.h>

static const int PIN_L_EN1     = 13;
static const int PIN_L_EN1_ALT = 12; // PCB ties GPIO 12 and 13
static const int PIN_R_EN1     = 26;
static const int PIN_L_PWM1    = 32;
static const int PIN_R_PWM1    = 33;

static const uint32_t PWM_FREQ_HZ  = 5000;
static const uint8_t  PWM_BITS     = 8;
static const uint32_t MOTOR_ON_MS  = 20000;
static const uint32_t MOTOR_OFF_MS = 3000;

void pumpOn() {
    digitalWrite(PIN_L_EN1, HIGH);
    digitalWrite(PIN_L_EN1_ALT, HIGH);
    digitalWrite(PIN_R_EN1, HIGH);
    ledcWrite(PIN_L_PWM1, 255);
    digitalWrite(PIN_R_PWM1, LOW);
    Serial.println("[PUMP 1] ON (100% duty)");
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
