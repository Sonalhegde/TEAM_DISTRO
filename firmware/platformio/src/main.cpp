#include <Arduino.h>

#define PIN_FLOW_SENSOR       35
#define PIN_FLOAT_SWITCH      34
#define PIN_MAIN_SWITCH       32
#define PIN_SOLENOID_VALVE    25

#define PIN_PUMP1_PWM         18
#define PIN_PUMP2_PWM         19
#define PIN_PUMP3_PWM         21

#define PWM_FREQ              5000
#define PWM_RES_BITS          8
#define PWM_CH_PUMP1          0
#define PWM_CH_PUMP2          1
#define PWM_CH_PUMP3          2

#define PUMP1_CASE1_DUTY      128  // 50% Duty Cycle
#define PUMP2_CASE1_DUTY      255  // 100% Duty Cycle
#define PUMP3_FLUSH_DUTY      230
#define PUMP_OFF_DUTY         0

static const float CALIB_A = 0.12926f;
static const float CALIB_B = 0.03222f;
static const float FLUSH_TANK_CAPACITY_L = 4.20f;

enum SystemState {
  STATE_IDLE,
  STATE_CASE1_NORMAL,
  STATE_CASE1_SUMP_ALARM,
  STATE_CASE2_FLUSHING,
  STATE_SAFE_SHUTDOWN
};

SystemState currentState = STATE_IDLE;

volatile unsigned long pulseCounter = 0;
unsigned long prevSampleMillis = 0;
unsigned long stateTimerMillis = 0;
const unsigned long SAMPLE_INTERVAL_MS = 500;

float currentFreqHz = 0.0f;
float currentFlowLpm = 0.0f;
float currentFlowMls = 0.0f;
float totalLitersTransferred = 0.0f;
float totalFlushLiters = 0.0f;

bool manualSwitchState = false;
bool sumpFloatState = false;

void IRAM_ATTR flowSensorISR() {
  pulseCounter++;
}

void setPumpDuties(uint8_t d1, uint8_t d2, uint8_t d3) {
  ledcWrite(PWM_CH_PUMP1, d1);
  ledcWrite(PWM_CH_PUMP2, d2);
  ledcWrite(PWM_CH_PUMP3, d3);
}

void setSolenoid(bool openValve) {
  digitalWrite(PIN_SOLENOID_VALVE, openValve ? HIGH : LOW);
}

void transitionTo(SystemState newState) {
  currentState = newState;
  stateTimerMillis = millis();

  switch (currentState) {
    case STATE_IDLE:
      setPumpDuties(PUMP_OFF_DUTY, PUMP_OFF_DUTY, PUMP_OFF_DUTY);
      setSolenoid(false);
      Serial.println("[FSM] STATE_IDLE");
      break;

    case STATE_CASE1_NORMAL:
      setSolenoid(true);
      setPumpDuties(PUMP1_CASE1_DUTY, PUMP2_CASE1_DUTY, PUMP_OFF_DUTY);
      Serial.println("[FSM] CASE 1 NORMAL: Recirculation active");
      break;

    case STATE_CASE1_SUMP_ALARM:
      setSolenoid(true);
      setPumpDuties(PUMP_OFF_DUTY, PUMP2_CASE1_DUTY, PUMP_OFF_DUTY);
      Serial.println("[FSM] CASE 1 LEVEL INTERLOCK: Pump 1 STOPPED, Pump 2 running");
      break;

    case STATE_CASE2_FLUSHING:
      setSolenoid(false);
      totalFlushLiters = 0.0f;
      setPumpDuties(PUMP1_CASE1_DUTY, PUMP2_CASE1_DUTY, PUMP3_FLUSH_DUTY);
      Serial.println("[FSM] CASE 2 FLUSHING: 3 Pumps operating simultaneously");
      break;

    case STATE_SAFE_SHUTDOWN:
      setPumpDuties(PUMP_OFF_DUTY, PUMP_OFF_DUTY, PUMP_OFF_DUTY);
      setSolenoid(false);
      Serial.println("[FSM] SAFE SHUTDOWN: Flush exhausted. System safely OFF");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_FLOAT_SWITCH, INPUT_PULLUP);
  pinMode(PIN_MAIN_SWITCH, INPUT_PULLUP);
  pinMode(PIN_SOLENOID_VALVE, OUTPUT);
  digitalWrite(PIN_SOLENOID_VALVE, LOW);

  pinMode(PIN_FLOW_SENSOR, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW_SENSOR), flowSensorISR, RISING);

  ledcSetup(PWM_CH_PUMP1, PWM_FREQ, PWM_RES_BITS);
  ledcSetup(PWM_CH_PUMP2, PWM_FREQ, PWM_RES_BITS);
  ledcSetup(PWM_CH_PUMP3, PWM_FREQ, PWM_RES_BITS);

  ledcAttachPin(PIN_PUMP1_PWM, PWM_CH_PUMP1);
  ledcAttachPin(PIN_PUMP2_PWM, PWM_CH_PUMP2);
  ledcAttachPin(PIN_PUMP3_PWM, PWM_CH_PUMP3);

  transitionTo(STATE_IDLE);
}

void loop() {
  unsigned long currentMillis = millis();
  manualSwitchState = (digitalRead(PIN_MAIN_SWITCH) == LOW);
  sumpFloatState = (digitalRead(PIN_FLOAT_SWITCH) == LOW);

  if (currentMillis - prevSampleMillis >= SAMPLE_INTERVAL_MS) {
    float dtSeconds = (currentMillis - prevSampleMillis) / 1000.0f;
    prevSampleMillis = currentMillis;

    noInterrupts();
    unsigned long pulses = pulseCounter;
    pulseCounter = 0;
    interrupts();

    currentFreqHz = (float)pulses / dtSeconds;
    currentFlowLpm = (currentFreqHz > 0.5f) ? ((CALIB_A * currentFreqHz) + CALIB_B) : 0.0f;
    if (currentFlowLpm < 0.0f) currentFlowLpm = 0.0f;
    currentFlowMls = currentFlowLpm * (1000.0f / 60.0f);

    float incrementalVolumeL = (currentFlowLpm * dtSeconds) / 60.0f;
    totalLitersTransferred += incrementalVolumeL;
    if (currentState == STATE_CASE2_FLUSHING) {
      totalFlushLiters += incrementalVolumeL;
    }

    Serial.printf("TELEMETRY,%lu,%d,%.2f,%.3f,%.2f,%.3f,%d\n",
                  currentMillis, currentState, currentFreqHz,
                  currentFlowLpm, currentFlowMls, totalLitersTransferred, sumpFloatState ? 1 : 0);
  }

  switch (currentState) {
    case STATE_IDLE:
      if (manualSwitchState) transitionTo(STATE_CASE1_NORMAL);
      break;

    case STATE_CASE1_NORMAL:
      if (!manualSwitchState) transitionTo(STATE_IDLE);
      else if (sumpFloatState) transitionTo(STATE_CASE1_SUMP_ALARM);
      break;

    case STATE_CASE1_SUMP_ALARM:
      if (!manualSwitchState) transitionTo(STATE_IDLE);
      else if (!sumpFloatState) transitionTo(STATE_CASE1_NORMAL);
      break;

    case STATE_CASE2_FLUSHING:
      if (totalFlushLiters >= FLUSH_TANK_CAPACITY_L || 
         ((currentMillis - stateTimerMillis > 15000) && (currentFlowLpm < 0.2f))) {
        transitionTo(STATE_SAFE_SHUTDOWN);
      }
      break;

    case STATE_SAFE_SHUTDOWN:
      break;
  }
  delay(10);
}
