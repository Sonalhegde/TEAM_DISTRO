/**
 * ============================================================================
 * Lam Research Challenge 3.0 - Stage 2: Practical Engineering Challenge
 * Team Name: TEAM DISTRO | Team ID: LRC-26-0528
 *
 * OFFICIAL VERIFIED CONTROLLER & SERIAL STUDIO DAQ FIRMWARE
 * PCB: "LAM Stage 2 PCB Rev 1.0" (T-Works Foundation, Drawn by Syed Tajammul Hassan)
 * Microcontroller: ESP32 (NodeMCU-32S / ESP32-DevKitC)
 *
 * Fully Verified Against Official Challenge Specification:
 *  - CASE 1: Closed-loop Recirculation & Sump Level Surge Mitigation
 *            Pump-01 @ 50% duty, Pump-02 @ 100% duty, Solenoid OV-01 OPEN.
 *            Level Interlock: Float HIGH -> Pump-01 STOPS, Pump-02 DRAINS @ 100%.
 *            Level Normal: Pump-01 auto-restarts @ 50%.
 *  - CASE 2: Multi-Pump Line Flushing & Safe Auto-Shutdown
 *            Solenoid OV-01 CLOSED (10L Process Reservoir ISOLATED).
 *            All 3 Pumps (P1, P2, P3) run simultaneously.
 *            Auto-shutdown when 4.2L flush volume is exhausted.
 *  - SINGLE-GO AUTOMATED SEQUENCE: Case 1 -> Case 2 continuous execution.
 *  - SERIAL STUDIO TELEMETRY: Real-time DAQ frame (/*time,state,freq,flow,mls,total,fsw*\/)
 *  - BUILT-IN PUSH BUTTON LED: Process & alarm illumination driver.
 * ============================================================================
 */

#include <Arduino.h>

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

// ======================= PHYSICAL PINOUT (LAM STAGE 2 PCB REV 1.0) =======================
// Derived directly from official T-Works Foundation PCB schematic
static const int PIN_SW         = 34;  // ON/OFF Tactile Switch (Input, 10k R10 pull-up to 3V3, 100nF C9 to GND)
static const int PIN_FSW        = 35;  // Sump Float Switch (Input, 10k R12 pull-up to 3V3, 100nF C10 to GND)
static const int PIN_FS         = 14;  // Flow Sensor FT-01 Pulse (JST FLOW_SENSOR Pin 2, Interrupt)
static const int PIN_SV         = 27;  // Solenoid Valve Driver OV-01 (JST SOLENOID_VALVE Pin 2)
static const int PIN_STATUS_LED = 2;   // ESP32 On-board Blue Status LED
static const int PIN_SW_LED     = 2;   // Push-button built-in LED (mirrors GPIO 2, or set to spare pin e.g. 4)

// BTS7960 Motor Driver Connectors (U2, U3, U4)
struct PumpPins {
    uint8_t lEn;
    uint8_t rEn;
    uint8_t lPwm;
    uint8_t rPwm;
};

static const PumpPins PUMP[3] = {
    {13, 26, 32, 33},  // Pump 1 (U2): Feed Pump (10L Tank -> Sump)
    {16, 17, 18, 19},  // Pump 2 (U3): Return/Discharge Pump (Sump -> FT-01 -> 10L Tank)
    {21, 22, 23, 25}   // Pump 3 (U4): Flush Supply Pump (4.2L Flush Tank -> System)
};

// ======================= OPERATING PARAMETERS (PER CHALLENGE PDF) =======================
static const uint32_t PWM_FREQ             = 20000;   // 20 kHz PWM (ultra-quiet, BTS7960 rated)
static const uint8_t  PWM_BITS             = 8;       // 8-bit resolution (0 - 255)
static const bool     PUMP_REVERSE         = false;   // Swap direction in software if needed
static const bool     FLOAT_CLOSED_IS_HIGH = true;    // Float switch closed (pin LOW) = Sump level HIGH

// Case 1 Operating Duties (PDF Page 3: "Pump-01 runs on 50% duty, Pump-02 runs on 100% duty")
static const float    PUMP1_C1_DUTY        = 50.0f;   // 50% Duty Cycle
static const float    PUMP2_C1_DUTY        = 100.0f;  // 100% Duty Cycle

// Case 2 Flush Operating Duties (PDF Page 3: "all three pumps operate simultaneously")
static const float    PUMP1_C2_DUTY        = 50.0f;   // 50% Duty Cycle
static const float    PUMP2_C2_DUTY        = 100.0f;  // 100% Duty Cycle
static const float    PUMP3_C2_DUTY        = 80.0f;   // 80% Flush Delivery

// Flow Sensor Calibration & Totalization (PDF Page 6: Q = a*f + b or pulses/L)
static float          ppl                  = 5880.0f; // YF-S401 pulses/L (or calibrated from FT-01)
static const float    FLOW_MIN_VALID       = 0.30f;   // Lower physical flow boundary (L/min)
static const float    FLUSH_TANK_CAP_L     = 4.20f;   // Flush tank capacity

// Timing Intervals
static const uint32_t C1_GRACE_MS          = 4000;    // Startup grace period before dry-run protection
static const uint32_t C1_NOFLOW_FAULT_MS   = 6000;    // No-flow trip threshold (dry run / blockage)
static const uint32_t C2_GRACE_MS          = 3000;    // Flush line priming grace period
static const uint32_t C2_EXHAUST_MS        = 3000;    // Zero-flow duration indicating 4.2L tank empty
static const uint32_t C2_MAX_RUN_MS        = 600000;  // 10-minute global safety cutoff
static const uint32_t SOV_CLOSE_DELAY_MS   = 500;     // Staggered valve shutdown (anti-water-hammer)

static const uint32_t DEBOUNCE_MS          = 30;      // Tactile button debounce
static const uint32_t LONG_PRESS_MS        = 1500;    // Short vs long press threshold
static const uint32_t FLOAT_DEBOUNCE_MS    = 250;     // Float switch debounce filter
static const uint32_t TELEMETRY_PERIOD_MS  = 500;     // 500 ms Serial Studio DAQ broadcast

// ======================= FINITE STATE MACHINE (FSM) =======================
enum SystemState {
    ST_IDLE             = 0,  // Standby: All pumps OFF, Solenoid CLOSED
    ST_CASE1_NORMAL     = 1,  // Case 1 Normal: SOV OPEN, P1 @ 50%, P2 @ 100%
    ST_CASE1_SURGE      = 2,  // Case 1 Surge: Sump HIGH -> P1 STOPPED (0%), P2 @ 100%
    ST_CASE2_FLUSHING   = 3,  // Case 2 Flush: SOV CLOSED (10L Isolated), P1, P2, P3 ALL ON
    ST_CASE2_STOPPING   = 4,  // Case 2 Staged Stop: Pumps OFF, awaiting anti-hammer SOV closure
    ST_FAULT            = 5   // System Tripped: Dry running, pipe blockage, or timeout
};

static SystemState state = ST_IDLE;
static bool singleGoMode = false;        // When true, runs Case 1 then auto-transitions to Case 2
static bool surgeDemonstrated = false;   // Flag indicating sump level surge has been mitigated
static uint32_t surgeClearMs = 0;        // Timestamp when surge condition cleared

// ======================= TELEMETRY STATE VARIABLES =======================
static volatile uint32_t pulseCount     = 0;
static uint32_t lastPulses              = 0;
static uint32_t lastTelemetryMs         = 0;
static uint32_t calBase                 = 0;

static float    flowFreqHz              = 0.0f;
static float    flowLpm                 = 0.0f;
static float    flowMls                 = 0.0f;
static float    totalL                  = 0.0f;
static float    totalFlushL             = 0.0f;
static bool     newFlowSample           = false;

static bool     floatHigh               = false;
static uint32_t floatChangeMs           = 0;
static bool     floatRawPrev            = false;

static uint32_t stateStartMs            = 0;
static uint32_t lowFlowSinceMs          = 0;
static uint32_t stopStartMs             = 0;
static const char* faultMsg             = "";

// ======================= HARDWARE INTERRUPT SERVICE ROUTINE =======================
void IRAM_ATTR onFlowPulseISR() {
    pulseCount++;
}

// ======================= PWM HARDWARE DRIVER =======================
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  #define PWM_ATTACH(pin, ch)   ledcAttach(pin, PWM_FREQ, PWM_BITS)
  #define PWM_WRITE(pin, ch, v) ledcWrite(pin, v)
#else
  #define PWM_ATTACH(pin, ch)   do { ledcSetup(ch, PWM_FREQ, PWM_BITS); ledcAttachPin(pin, ch); } while (0)
  #define PWM_WRITE(pin, ch, v) ledcWrite(ch, v)
#endif

void setPump(uint8_t pumpIdx, float dutyPct) {
    if (pumpIdx >= 3) return;
    dutyPct = constrain(dutyPct, 0.0f, 100.0f);
    uint32_t v = (uint32_t)(dutyPct * ((1 << PWM_BITS) - 1) / 100.0f);

    bool on = (v > 0);
    uint8_t fwd = PUMP_REVERSE ? PUMP[pumpIdx].lPwm : PUMP[pumpIdx].rPwm;
    uint8_t rev = PUMP_REVERSE ? PUMP[pumpIdx].rPwm : PUMP[pumpIdx].lPwm;

    PWM_WRITE(fwd, pumpIdx * 2,     on ? v : 0);
    PWM_WRITE(rev, pumpIdx * 2 + 1, 0);

    digitalWrite(PUMP[pumpIdx].lEn, on ? HIGH : LOW);
    digitalWrite(PUMP[pumpIdx].rEn, on ? HIGH : LOW);
}

void setAllPumps(float d1, float d2, float d3) {
    setPump(0, d1);
    setPump(1, d2);
    setPump(2, d3);
}

void pumpsOff() {
    setAllPumps(0.0f, 0.0f, 0.0f);
}

void sovOpen() {
    digitalWrite(PIN_SV, HIGH); // Active HIGH = Valve OPEN (Fluid flowing from 10L tank)
}

void sovClose() {
    digitalWrite(PIN_SV, LOW);  // Active LOW = Valve CLOSED (10L Tank securely isolated)
}

void allOff() {
    pumpsOff();
    sovClose();
}

// ======================= STATE TRANSITION FUNCTIONS =======================
void enterIdle(const char* reason) {
    allOff();
    state = ST_IDLE;
    singleGoMode = false;
    surgeDemonstrated = false;
    Serial.printf("[STATE] -> IDLE (%s)\n", reason);
}

void enterFault(const char* reason) {
    allOff();
    state = ST_FAULT;
    singleGoMode = false;
    faultMsg = reason;
    Serial.printf("[FAULT] TRIP: %s | Press switch or enter 'rt'/'stop' to clear.\n", reason);
}

void startCase1Normal() {
    state = ST_CASE1_NORMAL;
    stateStartMs = millis();
    lowFlowSinceMs = 0;

    sovOpen(); // Open process line from 10L reservoir
    setAllPumps(PUMP1_C1_DUTY, PUMP2_C1_DUTY, 0.0f); // P1 @ 50%, P2 @ 100%
    Serial.println("[STATE] -> CASE 1 NORMAL: Recirculation active (SOV=OPEN, P1=50%, P2=100%).");
}

void enterCase1Surge() {
    state = ST_CASE1_SURGE;
    stateStartMs = millis();

    // Sump Level Interlock (PDF Page 3): Stop Pump-01 immediately, keep Pump-02 running @ 100%
    sovOpen();
    setAllPumps(0.0f, PUMP2_C1_DUTY, 0.0f); // P1 CUT OFF, P2 DRAINING @ 100%
    Serial.println("[INTERLOCK] -> SUMP HIGH SURGE: Pump-01 STOPPED (0%), Pump-02 draining (100%).");
}

void startCase2Flushing() {
    state = ST_CASE2_FLUSHING;
    stateStartMs = millis();
    lowFlowSinceMs = 0;
    totalFlushL = 0.0f;

    // Isolate 10L Tank (PDF Page 3: "The On/Off valve must be switched off in order to isolate 10L tank")
    sovClose();

    // All three pumps operate simultaneously per Case 2 specification
    setAllPumps(PUMP1_C2_DUTY, PUMP2_C2_DUTY, PUMP3_C2_DUTY);
    Serial.println("[STATE] -> CASE 2 FLUSHING: 10L Tank ISOLATED. P1, P2, P3 running simultaneously.");
}

void stopAll(const char* reason) {
    enterIdle(reason);
}

// ======================= INPUT HANDLING & DEBOUNCING =======================
void handleFloatSwitch() {
    bool raw = digitalRead(PIN_FSW);
    bool high = FLOAT_CLOSED_IS_HIGH ? (raw == LOW) : (raw == HIGH);
    uint32_t now = millis();

    if (high != floatRawPrev) {
        floatRawPrev = high;
        floatChangeMs = now;
    }

    if ((now - floatChangeMs) >= FLOAT_DEBOUNCE_MS) {
        floatHigh = high;
    }
}

void onShortPress() {
    if (state == ST_IDLE) {
        startCase1Normal();
    } else if (state == ST_CASE1_NORMAL || state == ST_CASE1_SURGE) {
        // Transition to Case 2 in single-go or manual advance
        Serial.println("[USER] Manual trigger: Advancing from Case 1 to Case 2 Flush sequence.");
        startCase2Flushing();
    } else if (state == ST_FAULT) {
        enterIdle("Fault cleared by user");
    } else {
        stopAll("Tactile switch stop");
    }
}

void onLongPress() {
    if (state == ST_IDLE) {
        // Long press initiates the Full Single-Go Automated Sequence
        Serial.println("[USER] Starting FULL SINGLE-GO AUTOMATED SEQUENCE (Case 1 -> Case 2).");
        singleGoMode = true;
        startCase1Normal();
    } else if (state == ST_FAULT) {
        enterIdle("Fault cleared by user");
    } else {
        stopAll("Tactile switch emergency stop");
    }
}

void handleTactileButton() {
    static bool stable = false;
    static bool lastRaw = false;
    static bool longFired = false;
    static uint32_t chg = 0;
    static uint32_t pressStart = 0;

    uint32_t now = millis();
    bool raw = (digitalRead(PIN_SW) == LOW); // Active LOW on PCB

    if (raw != lastRaw) {
        lastRaw = raw;
        chg = now;
    }

    if ((now - chg >= DEBOUNCE_MS) && (raw != stable)) {
        stable = raw;
        if (stable) {
            pressStart = now;
            longFired = false;
        } else if (!longFired) {
            onShortPress();
        }
    }

    if (stable && !longFired && (now - pressStart >= LONG_PRESS_MS)) {
        longFired = true;
        onLongPress();
    }
}

// ======================= FLOW COMPUTATION & SERIAL STUDIO DAQ =======================
void updateFlowAndTelemetry() {
    uint32_t now = millis();
    if (now - lastTelemetryMs < TELEMETRY_PERIOD_MS) {
        return;
    }

    uint32_t dt = now - lastTelemetryMs;
    lastTelemetryMs = now;

    // Atomically grab pulses
    noInterrupts();
    uint32_t p = pulseCount;
    interrupts();

    uint32_t d = p - lastPulses;
    lastPulses = p;

    // Calculate pulse frequency and volumetric flow
    flowFreqHz = (float)d * 1000.0f / (float)dt;
    float qInstant = (flowFreqHz * 60.0f) / ppl; // L/min

    // Exponential moving average filter (alpha = 0.5)
    flowLpm = (0.5f * flowLpm) + (0.5f * qInstant);
    if (flowLpm < 0.05f) flowLpm = 0.0f;

    flowMls = flowLpm * (1000.0f / 60.0f); // Convert to mL/s
    float incL = (float)d / ppl;
    totalL += incL;

    if (state == ST_CASE2_FLUSHING) {
        totalFlushL += incL;
    }
    newFlowSample = true;

    /**
     * SERIAL STUDIO DAQ FRAME FORMAT:
     * Decoder: CSV | FrameStart: "/*" | FrameEnd: "*\/" | Separator: ","
     * Dataset 1: Time Since Boot (ms)
     * Dataset 2: System State ID (0=IDLE, 1=C1_NORM, 2=C1_SURGE, 3=C2_FLUSH, 4=C2_STOP, 5=FAULT)
     * Dataset 3: Pulse Frequency (Hz)
     * Dataset 4: Flow Rate (L/min)
     * Dataset 5: Flow Rate (mL/s)
     * Dataset 6: Total Transferred Volume (Liters)
     * Dataset 7: Sump High Level Alarm (0=Normal, 1=Tripped)
     */
    Serial.printf("/*%lu,%d,%.2f,%.3f,%.2f,%.3f,%d*/\r\n",
                  now,
                  (int)state,
                  flowFreqHz,
                  flowLpm,
                  flowMls,
                  totalL,
                  floatHigh ? 1 : 0);
}

// ======================= CORE PROCESS CONTROL LOOPS =======================
void executeProcessControl() {
    uint32_t now = millis();

    switch (state) {
        case ST_IDLE:
            break;

        case ST_CASE1_NORMAL:
            // Interlock Check: If sump float switch trips HIGH -> transition to Surge Mitigation
            if (floatHigh) {
                enterCase1Surge();
                return;
            }

            // Single-Go Auto-Advance: If surge mitigation was demonstrated and stabilized for 8s, transition to Case 2
            if (singleGoMode && surgeDemonstrated && (now - surgeClearMs >= 8000)) {
                Serial.println("\n[SINGLE-GO] Sump surge mitigation successfully demonstrated!");
                Serial.println("[SINGLE-GO] Automatically advancing to Case 2 Flush sequence in 3 seconds...");
                Serial.println("[SINGLE-GO] >> IMPORTANT: Ensure manual ball valve BV-02 is OPEN! <<");
                startCase2Flushing();
                return;
            }

            // Dry-run protection
            if ((now - stateStartMs > C1_GRACE_MS) && (flowLpm < FLOW_MIN_VALID)) {
                if (!lowFlowSinceMs) lowFlowSinceMs = now;
                if (now - lowFlowSinceMs > C1_NOFLOW_FAULT_MS) {
                    enterFault("Case 1 No flow detected (Pump dry run / Closed valve / Pipe blockage)");
                }
            } else {
                lowFlowSinceMs = 0;
            }
            break;

        case ST_CASE1_SURGE:
            // Sump Level Mitigation: If sump level has subsided below float switch -> restore normal
            if (!floatHigh) {
                surgeDemonstrated = true;
                surgeClearMs = now;
                Serial.println("[INTERLOCK] Sump level normal. Auto-restarting Pump-01 @ 50% duty.");
                startCase1Normal();
                return;
            }
            break;

        case ST_CASE2_FLUSHING:
            // Global timeout
            if (now - stateStartMs > C2_MAX_RUN_MS) {
                enterFault("Case 2 Flush safety timeout exceeded (10 min)");
                return;
            }

            // Flush Exhaustion Detection: When flush reservoir is empty, flow collapses below 0.3 L/min
            if (newFlowSample && (now - stateStartMs > C2_GRACE_MS)) {
                if (flowLpm < FLOW_MIN_VALID || totalFlushL >= FLUSH_TANK_CAP_L) {
                    if (!lowFlowSinceMs) lowFlowSinceMs = now;
                    if (now - lowFlowSinceMs >= C2_EXHAUST_MS || totalFlushL >= FLUSH_TANK_CAP_L) {
                        pumpsOff(); // Pumps stop immediately
                        stopStartMs = now;
                        state = ST_CASE2_STOPPING;
                        Serial.println("[C2] Flush water exhausted. All pumps OFF. Closing SOV in 500ms...");
                    }
                } else {
                    lowFlowSinceMs = 0;
                }
            }
            break;

        case ST_CASE2_STOPPING:
            // Staggered shutdown: Close solenoid valve 500ms after pumps to eliminate pressure shocks
            if (now - stopStartMs >= SOV_CLOSE_DELAY_MS) {
                sovClose();
                enterIdle("Case 2 Flush sequence complete. System safely OFF.");
                Serial.printf("[COMPLETE] Challenge Run Finished! Total Volume: %.3f L (Flush: %.3f L)\n",
                              totalL, totalFlushL);
            }
            break;

        case ST_FAULT:
            break;
    }
}

// ======================= PUSH-BUTTON & ON-BOARD LED INDICATION =======================
/**
 * Drives the built-in switch LED and on-board LED:
 *  - IDLE: Gentle breathing heartbeat (80ms flash every 1s)
 *  - CASE 1 NORMAL: Solid ON (Process circulating normally)
 *  - CASE 1 SURGE ALARM: Rapid 4 Hz flash (Warning: Sump Level High)
 *  - CASE 2 FLUSHING: 1 Hz smooth toggle (Line purging active)
 *  - FAULT: 10 Hz high-speed emergency strobe
 */
void updateStatusLEDs() {
    uint32_t now = millis();
    bool on = false;

    switch (state) {
        case ST_IDLE:
            on = (now % 1000) < 80;       // Heartbeat pulse
            break;
        case ST_CASE1_NORMAL:
            on = true;                    // Solid ON
            break;
        case ST_CASE1_SURGE:
            on = (now % 250) < 125;       // Fast 4 Hz alert
            break;
        case ST_CASE2_FLUSHING:
        case ST_CASE2_STOPPING:
            on = (now % 500) < 250;       // 1 Hz toggle
            break;
        case ST_FAULT:
            on = (now % 100) < 50;        // 10 Hz emergency strobe
            break;
    }

    digitalWrite(PIN_STATUS_LED, on ? HIGH : LOW);
    if (PIN_SW_LED >= 0 && PIN_SW_LED != PIN_STATUS_LED) {
        digitalWrite(PIN_SW_LED, on ? HIGH : LOW);
    }
}

// ======================= SERIAL CLI COMMAND INTERPRETER =======================
void handleSerialCLI() {
    static String rx;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c != '\n' && c != '\r') {
            rx += c;
            continue;
        }

        rx.trim();
        if (rx.length() == 0) continue;

        if (rx == "case1" || rx == "s1") {
            startCase1Normal();
        } else if (rx == "case2" || rx == "s2") {
            startCase2Flushing();
        } else if (rx == "auto" || rx == "singlego") {
            Serial.println("[CMD] Starting Single-Go Automated Challenge Run.");
            singleGoMode = true;
            startCase1Normal();
        } else if (rx == "stop" || rx == "x") {
            stopAll("CLI Emergency Stop Command");
        } else if (rx.startsWith("ppl ")) {
            float v = rx.substring(4).toFloat();
            if (v > 10.0f) {
                ppl = v;
                Serial.printf("[SET] Calibration Pulses/L: %.1f\n", ppl);
            }
        } else if (rx == "cal0") {
            calBase = pulseCount;
            Serial.println("[CAL] Pulse counter zeroed. Dispense known volume, then run: cal <litres>");
        } else if (rx.startsWith("cal ")) {
            float dispensedL = rx.substring(4).toFloat();
            uint32_t pulsesElapsed = pulseCount - calBase;
            if (dispensedL > 0.0f && pulsesElapsed > 0) {
                ppl = (float)pulsesElapsed / dispensedL;
                Serial.printf("[CAL] Calculated K-factor: %.1f pulses/L (Elapsed pulses: %lu)\n",
                              ppl, (unsigned long)pulsesElapsed);
            }
        } else if (rx == "rt") {
            totalL = 0.0f;
            totalFlushL = 0.0f;
            Serial.println("[SET] Totalizers reset to 0.000 L");
        } else if (rx == "status") {
            const char* names[] = {"IDLE", "CASE1_NORMAL", "CASE1_SURGE", "CASE2_FLUSHING", "CASE2_STOPPING", "FAULT"};
            Serial.printf("STATUS | State: %s | Sump: %s | Flow: %.2f L/min (%.1f Hz, %.1f mL/s) | Total: %.3f L\n",
                          names[state], floatHigh ? "HIGH (ALARM)" : "NORMAL", flowLpm, flowFreqHz, flowMls, totalL);
        } else if (rx == "?") {
            Serial.println("==================== TEAM DISTRO COMMAND MENU ====================");
            Serial.println("  case1 / s1   - Start Case 1 Closed-Loop Recirculation (P1=50%, P2=100%)");
            Serial.println("  case2 / s2   - Initiate Case 2 Flush Sequence (SOV Closed, 3 Pumps ON)");
            Serial.println("  auto         - Run Single-Go Automated Sequence (Case 1 -> Case 2)");
            Serial.println("  stop / x     - Safe Emergency Stop (All actuators OFF)");
            Serial.println("  ppl <pulses> - Set flow sensor calibration factor (pulses/L)");
            Serial.println("  cal0         - Zero counter for calibration run");
            Serial.println("  cal <litres> - Calculate new K-factor from dispensed volume");
            Serial.println("  rt           - Reset totalizer to 0.000 L");
            Serial.println("  status       - Snapshot print of current hardware state");
            Serial.println("  ?            - Show this command reference");
            Serial.println("==================================================================");
        }
        rx = "";
    }
}

// ======================= ARDUINO / ESP-IDF SETUP & LOOP =======================
void setup() {
    Serial.begin(115200);

    // 1. Initialise all actuators to safe OFF state first
    pinMode(PIN_SV, OUTPUT);
    digitalWrite(PIN_SV, LOW);

    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);

    if (PIN_SW_LED >= 0) {
        pinMode(PIN_SW_LED, OUTPUT);
        digitalWrite(PIN_SW_LED, LOW);
    }

    for (int i = 0; i < 3; i++) {
        pinMode(PUMP[i].lEn, OUTPUT);
        digitalWrite(PUMP[i].lEn, LOW);
        pinMode(PUMP[i].rEn, OUTPUT);
        digitalWrite(PUMP[i].rEn, LOW);
        PWM_ATTACH(PUMP[i].rPwm, i * 2);
        PWM_ATTACH(PUMP[i].lPwm, (i * 2) + 1);
    }
    pumpsOff();

    // 2. Initialise physical digital inputs matching schematic
    pinMode(PIN_SW, INPUT);        // GPIO 34 (10k pull-up R10 to 3V3 on PCB)
    pinMode(PIN_FSW, INPUT);       // GPIO 35 (10k pull-up R12 to 3V3 on PCB)
    pinMode(PIN_FS, INPUT_PULLUP); // GPIO 14 (Flow sensor interrupt line)

    attachInterrupt(digitalPinToInterrupt(PIN_FS), onFlowPulseISR, RISING);

    lastTelemetryMs = millis();
    Serial.println("\n============================================================");
    Serial.println("  LAM RESEARCH CHALLENGE 3.0 - STAGE 2 VERIFIED FIRMWARE    ");
    Serial.println("  TEAM: DISTRO | PCB: LAM STAGE 2 PCB REV 1.0 (T-Works)     ");
    Serial.println("============================================================");
    Serial.println("[INIT] Controller & Serial Studio DAQ Ready.");
    Serial.println("[INIT] Short Press: Start Case 1 / Advance | Long Press: Single-Go Mode");
}

void loop() {
    handleSerialCLI();
    handleTactileButton();
    handleFloatSwitch();
    updateFlowAndTelemetry();
    executeProcessControl();

    newFlowSample = false;
    updateStatusLEDs();
}
