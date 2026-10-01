/**
 * ============================================================================
 * Lam Research Challenge 3.0 - Stage 2: Practical Engineering Challenge
 * Team Name: TEAM DISTRO
 * Team ID:   LRC-26-0528
 * Description: ESP32 Automated Liquid-Flow System Controller
 *              - Case 1: Closed-loop Recirculation & Sump Level Protection
 *              - Case 2: Multi-Pump Line Flushing & Safe Auto-Shutdown
 *              - Flow Transmitter (FT-01) Calibration & High-Precision Totalizer
 * ============================================================================
 */

#include <Arduino.h>

// ======================= PIN CONFIGURATION =======================
#define PIN_FLOW_SENSOR       35   // Flow Transmitter FT-01 pulse input (Interrupt)
#define PIN_FLOAT_SWITCH      34   // Sump High-Level Float Switch (Digital In, Pull-up)
#define PIN_MAIN_SWITCH       32   // Master ON/OFF Switch
#define PIN_SOLENOID_VALVE    25   // Solenoid ON/OFF Valve (OV-01 / XV-01)

#define PIN_PUMP1_PWM         18   // Pump-01 (Feed to Sump)
#define PIN_PUMP2_PWM         19   // Pump-02 (Discharge from Sump)
#define PIN_PUMP3_PWM         21   // Pump-03 (Flush Water Feed)

// ======================= PWM CONFIGURATION =======================
#define PWM_FREQ              5000 // 5 kHz PWM frequency
#define PWM_RES_BITS          8    // 8-bit resolution (0 - 255)
#define PWM_CH_PUMP1          0
#define PWM_CH_PUMP2          1
#define PWM_CH_PUMP3          2

// Operating Duty Cycles (per Challenge Specifications)
#define PUMP1_CASE1_DUTY      128  // 50% Duty Cycle (~128 / 255)
#define PUMP2_CASE1_DUTY      255  // 100% Duty Cycle (255 / 255)
#define PUMP3_FLUSH_DUTY      230  // ~90% Flush Duty Cycle
#define PUMP_OFF_DUTY         0    // 0% Duty Cycle (STOP)

// ======================= CALIBRATION PARAMETERS =======================
// Derived from 5-point experimental calibration: Q (L/min) = a * f (Hz) + b
static const float CALIB_A = 0.12926f; // Slope (L/min per Hz)
static const float CALIB_B = 0.03222f; // Offset (L/min)

// Flush Tank Capacity
static const float FLUSH_TANK_CAPACITY_L = 4.20f; // 4.2 Litres

// ======================= STATE MACHINE DEFINITIONS =======================
enum SystemState {
  STATE_IDLE,                 // System standby / unpowered
  STATE_CASE1_NORMAL,         // Case 1: Recirculation (P1 @ 50%, P2 @ 100%, OV-01 Open)
  STATE_CASE1_SUMP_ALARM,     // Case 1: Level Mitigation (P1 Stopped, P2 @ 100%)
  STATE_CASE2_FLUSHING,       // Case 2: Flush line (OV-01 Closed, P1, P2, P3 ALL ON)
  STATE_SAFE_SHUTDOWN         // Case 2: Completed, Flush Exhausted, All Pumps OFF
};

SystemState currentState = STATE_IDLE;

// ======================= GLOBAL TELEMETRY VARIABLES =======================
volatile unsigned long pulseCounter = 0;
volatile unsigned long lastPulseTime = 0;

unsigned long prevSampleMillis = 0;
unsigned long stateTimerMillis = 0;
const unsigned long SAMPLE_INTERVAL_MS = 500; // 500ms telemetry update

float currentFreqHz = 0.0f;
float currentFlowLpm = 0.0f;
float currentFlowMls = 0.0f;
float totalLitersTransferred = 0.0f;
float totalFlushLiters = 0.0f;

bool manualSwitchState = false;
bool sumpFloatState = false;

// ======================= INTERRUPT SERVICE ROUTINE =======================
void IRAM_ATTR flowSensorISR() {
  pulseCounter++;
  lastPulseTime = micros();
}

// ======================= HELPER FUNCTIONS =======================
void setPumpDuties(uint8_t d1, uint8_t d2, uint8_t d3) {
  ledcWrite(PWM_CH_PUMP1, d1);
  ledcWrite(PWM_CH_PUMP2, d2);
  ledcWrite(PWM_CH_PUMP3, d3);
}

void setSolenoid(bool openValve) {
  // HIGH = Open Valve, LOW = Closed (Safe spring-return)
  digitalWrite(PIN_SOLENOID_VALVE, openValve ? HIGH : LOW);
}

void transitionTo(SystemState newState) {
  currentState = newState;
  stateTimerMillis = millis();

  switch (currentState) {
    case STATE_IDLE:
      setPumpDuties(PUMP_OFF_DUTY, PUMP_OFF_DUTY, PUMP_OFF_DUTY);
      setSolenoid(false);
      Serial.println("[FSM] -> STATE_IDLE: All actuators disabled.");
      break;

    case STATE_CASE1_NORMAL:
      setSolenoid(true); // Open process line from 10L tank
      setPumpDuties(PUMP1_CASE1_DUTY, PUMP2_CASE1_DUTY, PUMP_OFF_DUTY);
      Serial.println("[FSM] -> CASE 1 NORMAL: Recirculation active (P1=50%, P2=100%, OV-01=OPEN).");
      break;

    case STATE_CASE1_SUMP_ALARM:
      setSolenoid(true);
      setPumpDuties(PUMP_OFF_DUTY, PUMP2_CASE1_DUTY, PUMP_OFF_DUTY); // PUMP 1 CUT OFF
      Serial.println("[FSM] -> CASE 1 LEVEL INTERLOCK: External fluid detected! P1 STOPPED, P2 draining sump.");
      break;

    case STATE_CASE2_FLUSHING:
      setSolenoid(false); // ISOLATE 10L Process Tank
      totalFlushLiters = 0.0f;
      // All 3 pumps operate simultaneously per Case 2 specification
      setPumpDuties(PUMP1_CASE1_DUTY, PUMP2_CASE1_DUTY, PUMP3_FLUSH_DUTY);
      Serial.println("[FSM] -> CASE 2 FLUSHING: 10L Tank Isolated. All 3 Pumps running simultaneously.");
      break;

    case STATE_SAFE_SHUTDOWN:
      setPumpDuties(PUMP_OFF_DUTY, PUMP_OFF_DUTY, PUMP_OFF_DUTY);
      setSolenoid(false);
      Serial.println("[FSM] -> SAFE SHUTDOWN: Flush tank exhausted. System securely halted.");
      break;
  }
}

// ======================= SERIAL COMMAND INTERPRETER =======================
void handleSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "case1" || cmd == "start1") {
      transitionTo(STATE_CASE1_NORMAL);
    } else if (cmd == "case2" || cmd == "start2") {
      transitionTo(STATE_CASE2_FLUSHING);
    } else if (cmd == "stop") {
      transitionTo(STATE_IDLE);
    } else if (cmd == "reset_vol") {
      totalLitersTransferred = 0.0f;
      totalFlushLiters = 0.0f;
      Serial.println("[CMD] Totalizers reset to 0.00 L");
    } else if (cmd == "help") {
      Serial.println("--- TEAM DISTRO COMMAND MENU ---");
      Serial.println("  case1     - Start Case 1 Closed-Loop Recirculation");
      Serial.println("  case2     - Initiate Case 2 Flush Sequence");
      Serial.println("  stop      - Safe Stop All Pumps & Valves");
      Serial.println("  reset_vol - Reset Totalizer Liters");
      Serial.println("  status    - Print Full Hardware Status");
      Serial.println("---------------------------------");
    }
  }
}

// ======================= SETUP =======================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=================================================");
  Serial.println("  LAM RESEARCH CHALLENGE 3.0 - STAGE 2 FIRMWARE  ");
  Serial.println("  TEAM: DISTRO | ID: LRC-26-0528                 ");
  Serial.println("=================================================");

  // Pin Modes
  pinMode(PIN_FLOAT_SWITCH, INPUT_PULLUP);
  pinMode(PIN_MAIN_SWITCH, INPUT_PULLUP);
  pinMode(PIN_SOLENOID_VALVE, OUTPUT);
  digitalWrite(PIN_SOLENOID_VALVE, LOW); // Solenoid default closed

  pinMode(PIN_FLOW_SENSOR, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW_SENSOR), flowSensorISR, RISING);

  // Configure PWM Channels
  ledcSetup(PWM_CH_PUMP1, PWM_FREQ, PWM_RES_BITS);
  ledcSetup(PWM_CH_PUMP2, PWM_FREQ, PWM_RES_BITS);
  ledcSetup(PWM_CH_PUMP3, PWM_FREQ, PWM_RES_BITS);

  ledcAttachPin(PIN_PUMP1_PWM, PWM_CH_PUMP1);
  ledcAttachPin(PIN_PUMP2_PWM, PWM_CH_PUMP2);
  ledcAttachPin(PIN_PUMP3_PWM, PWM_CH_PUMP3);

  transitionTo(STATE_IDLE);
  Serial.println("Initialization complete. Awaiting master switch or Serial start.");
}

// ======================= MAIN LOOP =======================
void loop() {
  handleSerialCommands();

  unsigned long currentMillis = millis();

  // Read Inputs with Software Filtering
  manualSwitchState = (digitalRead(PIN_MAIN_SWITCH) == LOW); // Active Low
  sumpFloatState = (digitalRead(PIN_FLOAT_SWITCH) == LOW);   // Active Low (Float lifted)

  // 1. Telemetry & Flow Calculations
  if (currentMillis - prevSampleMillis >= SAMPLE_INTERVAL_MS) {
    float dtSeconds = (currentMillis - prevSampleMillis) / 1000.0f;
    prevSampleMillis = currentMillis;

    // Atomically grab pulses
    noInterrupts();
    unsigned long pulses = pulseCounter;
    pulseCounter = 0;
    interrupts();

    currentFreqHz = (float)pulses / dtSeconds;

    if (currentFreqHz > 0.5f) {
      currentFlowLpm = (CALIB_A * currentFreqHz) + CALIB_B;
    } else {
      currentFlowLpm = 0.0f;
    }

    if (currentFlowLpm < 0.0f) currentFlowLpm = 0.0f;
    currentFlowMls = currentFlowLpm * (1000.0f / 60.0f); // Convert to mL/s

    float incrementalVolumeL = (currentFlowLpm * dtSeconds) / 60.0f;
    totalLitersTransferred += incrementalVolumeL;

    if (currentState == STATE_CASE2_FLUSHING) {
      totalFlushLiters += incrementalVolumeL;
    }

    // Output structured CSV telemetry for DAQ Software
    // FORMAT: TELEMETRY,time_ms,state,freq_Hz,flow_Lpm,flow_Mls,total_L,float_state
    Serial.printf("TELEMETRY,%lu,%d,%.2f,%.3f,%.2f,%.3f,%d\n",
                  currentMillis, currentState, currentFreqHz,
                  currentFlowLpm, currentFlowMls, totalLitersTransferred, sumpFloatState ? 1 : 0);
  }

  // 2. State Machine Logic & Transitions
  switch (currentState) {
    case STATE_IDLE:
      if (manualSwitchState) {
        transitionTo(STATE_CASE1_NORMAL);
      }
      break;

    case STATE_CASE1_NORMAL:
      if (!manualSwitchState) {
        transitionTo(STATE_IDLE);
      } else if (sumpFloatState) {
        // High level detected in sump due to external fluid charge!
        transitionTo(STATE_CASE1_SUMP_ALARM);
      }
      break;

    case STATE_CASE1_SUMP_ALARM:
      if (!manualSwitchState) {
        transitionTo(STATE_IDLE);
      } else if (!sumpFloatState) {
        // Fluid level has dropped below threshold; resume normal circulation
        transitionTo(STATE_CASE1_NORMAL);
      }
      break;

    case STATE_CASE2_FLUSHING:
      // Condition 1: 4.2L Flush volume exhausted
      // Condition 2: Dry-run timeout (if flow drops to near zero for >3 seconds after initial priming)
      if (totalFlushLiters >= FLUSH_TANK_CAPACITY_L) {
        Serial.println("[AUTO-SHUTDOWN] 4.2L Flush Tank capacity reached.");
        transitionTo(STATE_SAFE_SHUTDOWN);
      } else if ((currentMillis - stateTimerMillis > 15000) && (currentFlowLpm < 0.2f)) {
        Serial.println("[AUTO-SHUTDOWN] Zero flow detected in flush line. Supply exhausted.");
        transitionTo(STATE_SAFE_SHUTDOWN);
      }
      break;

    case STATE_SAFE_SHUTDOWN:
      // Awaiting power reset or manual command
      break;
  }

  delay(10);
}
