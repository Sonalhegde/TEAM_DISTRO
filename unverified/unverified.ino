/*
  Lam Research Challenge 3.0 - Stage 2  |  ESP32 (NodeMCU-32S) controller
  Pin map taken from "LAM Stage 2 PCB" schematic Rev 1.0

  Case 1: Control + Measurement
    Sump float switch HIGH -> open SOV, run pump(s) with PI control to hold
    3 L/min (YF-S401 feedback), totalize discharged volume. Float LOW -> stop.
  Case 2: Flush + Shutdown
    SOV open, all 3 pumps run together. When flow collapses (flush tank empty)
    pumps stop, then SOV closes -> OFF.

  Tactile switch (G34, active LOW):
    IDLE     : SHORT press = start Case 1 (auto level control)
               LONG  press = start Case 2 (flush + shutdown)
    RUNNING  : any press   = stop everything
    FAULT    : any press   = reset

  No delay() anywhere. No pump ramp (pumps jump straight to set duty).
  Serial 115200 commands: s1, s2, x, sp <L/min>, ppl <pulses/L>, cal0,
                          cal <litres>, rt, ?
*/
#include <Arduino.h>

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

// ======================= PIN MAP (from schematic) =======================
#define PIN_SW        34   // ON/OFF tactile switch (input-only, 10k pull-up on PCB)
#define PIN_FSW       35   // float switch          (input-only, 10k pull-up on PCB)
#define PIN_FS        14   // flow sensor pulse (FLOW_SENSOR connector)
#define PIN_SV        27   // solenoid MOSFET driver module PWM/IN pin
#define PIN_STATUS_LED 2   // on-board LED
#define PIN_SW_LED    -1   // LED inside tactile switch: no GPIO on schematic. Set a pin if you wire one.

struct PumpPins { uint8_t lEn, rEn, lPwm, rPwm; };
const PumpPins PUMP[3] = {
  {13, 26, 32, 33},   // Pump 1 -> U2 (BTS7960 #1)
  {16, 17, 18, 19},   // Pump 2 -> U3 (BTS7960 #2)
  {21, 22, 23, 25}    // Pump 3 -> U4 (BTS7960 #3)
};

// ======================= SETTINGS =======================
const bool  PUMP_REVERSE        = false;  // true = drive L_PWM instead of R_PWM (swap direction in software)
const bool  FLOAT_CLOSED_IS_HIGH = true;  // true: float contact closed (pin LOW) = sump level HIGH
const uint32_t PWM_FREQ         = 20000;  // BTS7960 ok up to 25 kHz
const uint8_t  PWM_BITS         = 8;

float targetLpm        = 3.0f;    // nominal target
float ppl              = 5880.0f; // YF-S401 pulses per litre (F=98*Q). CALIBRATE!
const float FLOW_MIN_VALID = 0.3f; // sensor lower limit (L/min)

// Case 1
const uint8_t  C1_PUMP_MASK     = 0b001;  // pumps used for Case 1 (bit0=P1, bit1=P2, bit2=P3)
const float    C1_DUTY_START    = 70.0f;  // % applied instantly at start
const float    C1_DUTY_MIN      = 30.0f;
const float    C1_DUTY_MAX      = 100.0f;
const float    KP               = 4.0f;   // % per (L/min) error
const float    KI               = 3.0f;   // % per (L/min*s)
const uint32_t C1_DRAIN_HOLD_MS = 0;      // extra run time after float goes low (0 = stop at once)
const uint32_t C1_GRACE_MS      = 4000;   // time before no-flow check starts
const uint32_t C1_NOFLOW_FAULT_MS = 6000; // pumping + no flow this long = FAULT

// Case 2
const uint8_t  C2_PUMP_MASK     = 0b111;  // all three pumps
const float    C2_DUTY          = 80.0f;  // 3 pumps ~10 A at 100%: check SMPS/fuse before raising
const uint32_t C2_GRACE_MS      = 3000;
const uint32_t C2_EXHAUST_MS    = 3000;   // flow below FLOW_MIN_VALID this long = flush supply exhausted
const uint32_t C2_MAX_RUN_MS    = 600000; // safety timeout 10 min
const uint32_t SOV_CLOSE_AFTER_PUMPS_MS = 500; // close SOV this long after pumps stop

// Switch / float timing
const uint32_t DEBOUNCE_MS   = 30;
const uint32_t LONG_PRESS_MS = 1500;
const uint32_t FLOAT_DEBOUNCE_MS = 300;
const uint32_t FLOW_PERIOD_MS = 500;
const uint32_t PRINT_PERIOD_MS = 1000;

// ======================= STATE =======================
enum State { ST_IDLE, ST_C1_STANDBY, ST_C1_PUMPING, ST_C2_FLUSH, ST_C2_STOPPING, ST_FAULT };
State state = ST_IDLE;

volatile uint32_t pulseCount = 0;
void IRAM_ATTR onPulse() { pulseCount++; }

uint32_t lastPulses = 0, lastFlowMs = 0, calBase = 0;
float flowLpm = 0, totalL = 0;
bool  newFlowSample = false;

bool floatHigh = false;
uint32_t floatChangeMs = 0; bool floatRawPrev = false;

float integ = 0, duty = 0;
uint32_t stateStartMs = 0, lowFlowSinceMs = 0, lastHighMs = 0, stopStartMs = 0;
uint32_t lastPrintMs = 0;
const char* faultMsg = "";

// ======================= PWM helpers =======================
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  #define PWM_ATTACH(pin, ch) ledcAttach(pin, PWM_FREQ, PWM_BITS)
  #define PWM_WRITE(pin, ch, v) ledcWrite(pin, v)
#else
  #define PWM_ATTACH(pin, ch) do { ledcSetup(ch, PWM_FREQ, PWM_BITS); ledcAttachPin(pin, ch); } while (0)
  #define PWM_WRITE(pin, ch, v) ledcWrite(ch, v)
#endif

void setPumps(uint8_t mask, float dutyPct) {
  dutyPct = constrain(dutyPct, 0.0f, 100.0f);
  uint32_t v = (uint32_t)(dutyPct * ((1 << PWM_BITS) - 1) / 100.0f);
  for (int i = 0; i < 3; i++) {
    bool on = (mask & (1 << i)) && v > 0;
    uint8_t fwd = PUMP_REVERSE ? PUMP[i].lPwm : PUMP[i].rPwm;
    uint8_t rev = PUMP_REVERSE ? PUMP[i].rPwm : PUMP[i].lPwm;
    PWM_WRITE(fwd, i * 2,     on ? v : 0);
    PWM_WRITE(rev, i * 2 + 1, 0);
    digitalWrite(PUMP[i].lEn, on ? HIGH : LOW);
    digitalWrite(PUMP[i].rEn, on ? HIGH : LOW);
  }
}
void pumpsOff()  { setPumps(0, 0); duty = 0; }
void sovOpen()   { digitalWrite(PIN_SV, HIGH); }
void sovClose()  { digitalWrite(PIN_SV, LOW); }

void allOff() { pumpsOff(); sovClose(); }

// ======================= State transitions =======================
void enterIdle(const char* why) {
  allOff(); state = ST_IDLE;
  Serial.printf("[STATE] IDLE (%s)\n", why);
}
void enterFault(const char* why) {
  allOff(); state = ST_FAULT; faultMsg = why;
  Serial.printf("[FAULT] %s  - press switch to reset\n", why);
}
void startCase1() {
  allOff(); state = ST_C1_STANDBY; stateStartMs = millis();
  Serial.println("[STATE] CASE 1 armed: waiting for sump HIGH level");
}
void startC1Pumping() {
  integ = 0; duty = C1_DUTY_START;
  lowFlowSinceMs = 0; stateStartMs = millis(); lastHighMs = millis();
  sovOpen();
  setPumps(C1_PUMP_MASK, duty);     // instant start, no ramp
  state = ST_C1_PUMPING;
  Serial.println("[STATE] CASE 1 pumping out sump");
}
void startCase2() {
  allOff(); lowFlowSinceMs = 0; stateStartMs = millis();
  sovOpen();
  setPumps(C2_PUMP_MASK, C2_DUTY);
  state = ST_C2_FLUSH;
  Serial.println("[STATE] CASE 2 flushing, all pumps ON");
}
void stopAll(const char* why) { enterIdle(why); }

// ======================= Inputs =======================
void handleFloat() {
  bool raw = digitalRead(PIN_FSW);
  bool high = FLOAT_CLOSED_IS_HIGH ? (raw == LOW) : (raw == HIGH);
  uint32_t now = millis();
  if (high != floatRawPrev) { floatRawPrev = high; floatChangeMs = now; }
  if (now - floatChangeMs >= FLOAT_DEBOUNCE_MS) floatHigh = high;
}

void onShort() {
  if (state == ST_IDLE) startCase1();
  else if (state == ST_FAULT) enterIdle("fault reset");
  else stopAll("switch stop");
}
void onLong() {
  if (state == ST_IDLE) startCase2();
  else if (state == ST_FAULT) enterIdle("fault reset");
  else stopAll("switch stop");
}

void handleButton() {
  static bool stable = false, lastRaw = false, longFired = false;
  static uint32_t chg = 0, pressStart = 0;
  uint32_t now = millis();
  bool raw = (digitalRead(PIN_SW) == LOW);
  if (raw != lastRaw) { lastRaw = raw; chg = now; }
  if (now - chg >= DEBOUNCE_MS && raw != stable) {
    stable = raw;
    if (stable) { pressStart = now; longFired = false; }
    else if (!longFired) onShort();
  }
  if (stable && !longFired && now - pressStart >= LONG_PRESS_MS) {
    longFired = true; onLong();
  }
}

// ======================= Flow =======================
void updateFlow() {
  uint32_t now = millis();
  if (now - lastFlowMs < FLOW_PERIOD_MS) return;
  uint32_t dt = now - lastFlowMs; lastFlowMs = now;
  uint32_t p = pulseCount; uint32_t d = p - lastPulses; lastPulses = p;
  float hz = d * 1000.0f / dt;
  float q = hz * 60.0f / ppl;                 // L/min
  flowLpm = 0.5f * flowLpm + 0.5f * q;        // light smoothing
  totalL += d / ppl;                          // totalizer (litres)
  newFlowSample = true;
}

// ======================= Control =======================
void runCase1() {
  uint32_t now = millis();
  if (state == ST_C1_STANDBY) {
    if (floatHigh) startC1Pumping();
    return;
  }
  // ST_C1_PUMPING
  if (floatHigh) lastHighMs = now;
  if (!floatHigh && now - lastHighMs >= C1_DRAIN_HOLD_MS) {
    pumpsOff(); sovClose(); state = ST_C1_STANDBY;
    Serial.printf("[C1] Sump low - pumps stopped. Total = %.3f L\n", totalL);
    return;
  }
  if (newFlowSample) {
    float dt = FLOW_PERIOD_MS / 1000.0f;
    float err = targetLpm - flowLpm;
    integ += KI * err * dt;
    integ = constrain(integ, -40.0f, 40.0f);
    duty = constrain(C1_DUTY_START + KP * err + integ, C1_DUTY_MIN, C1_DUTY_MAX);
    setPumps(C1_PUMP_MASK, duty);
    // no-flow / blocked protection
    if (now - stateStartMs > C1_GRACE_MS && flowLpm < FLOW_MIN_VALID) {
      if (!lowFlowSinceMs) lowFlowSinceMs = now;
      if (now - lowFlowSinceMs > C1_NOFLOW_FAULT_MS) enterFault("No flow while pumping (valve closed / dry / blocked)");
    } else lowFlowSinceMs = 0;
  }
}

void runCase2() {
  uint32_t now = millis();
  if (state == ST_C2_FLUSH) {
    if (now - stateStartMs > C2_MAX_RUN_MS) { enterFault("Flush timeout"); return; }
    if (newFlowSample && now - stateStartMs > C2_GRACE_MS) {
      if (flowLpm < FLOW_MIN_VALID) {
        if (!lowFlowSinceMs) lowFlowSinceMs = now;
        if (now - lowFlowSinceMs >= C2_EXHAUST_MS) {
          pumpsOff(); stopStartMs = now; state = ST_C2_STOPPING;   // pumps first
          Serial.println("[C2] Flush supply exhausted - pumps OFF");
        }
      } else lowFlowSinceMs = 0;
    }
  } else if (state == ST_C2_STOPPING) {
    if (now - stopStartMs >= SOV_CLOSE_AFTER_PUMPS_MS) {
      sovClose(); state = ST_IDLE;
      Serial.printf("[C2] SOV closed - system OFF. Total = %.3f L\n", totalL);
    }
  }
}

// ======================= LED =======================
void updateLed() {
  uint32_t now = millis(); bool on = false;
  switch (state) {
    case ST_IDLE:        on = (now % 1000) < 80; break;     // heartbeat
    case ST_C1_STANDBY:  on = true; break;                  // solid
    case ST_C1_PUMPING:  on = (now % 250) < 125; break;     // 2 Hz
    case ST_C2_FLUSH:
    case ST_C2_STOPPING: on = (now % 500) < 250; break;     // 1 Hz
    case ST_FAULT:       on = (now % 100) < 50; break;      // fast
  }
  digitalWrite(PIN_STATUS_LED, on);
  if (PIN_SW_LED >= 0) digitalWrite(PIN_SW_LED, on);
}

// ======================= Serial =======================
void handleSerial() {
  static String buf;
  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n' && c != '\r') { buf += c; continue; }
    buf.trim();
    if (buf == "s1") { if (state == ST_IDLE) startCase1(); }
    else if (buf == "s2") { if (state == ST_IDLE) startCase2(); }
    else if (buf == "x") stopAll("serial stop");
    else if (buf.startsWith("sp ")) { targetLpm = constrain(buf.substring(3).toFloat(), 1.0f, 6.0f); Serial.printf("Target = %.2f L/min\n", targetLpm); }
    else if (buf.startsWith("ppl ")) { float v = buf.substring(4).toFloat(); if (v > 10) ppl = v; Serial.printf("ppl = %.1f\n", ppl); }
    else if (buf == "cal0") { calBase = pulseCount; Serial.println("Cal counter zeroed. Run a known volume, then: cal <litres>"); }
    else if (buf.startsWith("cal ")) {
      float L = buf.substring(4).toFloat(); uint32_t n = pulseCount - calBase;
      if (L > 0 && n > 0) { ppl = n / L; Serial.printf("Pulses=%lu  -> ppl = %.1f\n", (unsigned long)n, ppl); }
    }
    else if (buf == "rt") { totalL = 0; Serial.println("Total reset"); }
    else if (buf == "?") Serial.println("s1 s2 x | sp <L/min> | ppl <n> | cal0 | cal <litres> | rt");
    buf = "";
  }
}

void printStatus() {
  uint32_t now = millis();
  if (now - lastPrintMs < PRINT_PERIOD_MS) return;
  lastPrintMs = now;
  const char* names[] = {"IDLE", "C1_STANDBY", "C1_PUMPING", "C2_FLUSH", "C2_STOPPING", "FAULT"};
  Serial.printf("%s | float=%s | flow=%.2f L/min | duty=%.0f%% | total=%.3f L\n",
                names[state], floatHigh ? "HIGH" : "low", flowLpm, duty, totalL);
}

// ======================= Setup / loop =======================
void setup() {
  Serial.begin(115200);

  // Outputs to safe state FIRST
  pinMode(PIN_SV, OUTPUT); digitalWrite(PIN_SV, LOW);
  pinMode(PIN_STATUS_LED, OUTPUT);
  if (PIN_SW_LED >= 0) pinMode(PIN_SW_LED, OUTPUT);
  for (int i = 0; i < 3; i++) {
    pinMode(PUMP[i].lEn, OUTPUT); digitalWrite(PUMP[i].lEn, LOW);
    pinMode(PUMP[i].rEn, OUTPUT); digitalWrite(PUMP[i].rEn, LOW);
    PWM_ATTACH(PUMP[i].rPwm, i * 2);
    PWM_ATTACH(PUMP[i].lPwm, i * 2 + 1);
  }
  pumpsOff();

  pinMode(PIN_SW, INPUT);     // G34/G35 have no internal pull-ups; PCB has 10k to 3V3
  pinMode(PIN_FSW, INPUT);
  pinMode(PIN_FS, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FS), onPulse, RISING);

  lastFlowMs = millis();
  Serial.println("\nLam Stage 2 controller ready. Short press = Case 1, Long press = Case 2. '?' for commands.");
}

void loop() {
  handleSerial();
  handleButton();
  handleFloat();
  updateFlow();

  if (state == ST_C1_STANDBY || state == ST_C1_PUMPING) runCase1();
  else if (state == ST_C2_FLUSH || state == ST_C2_STOPPING) runCase2();

  newFlowSample = false;
  updateLed();
  printStatus();
}
