
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
 *  - SERIAL STUDIO TELEMETRY: Real-time DAQ frame (format: / * ... * / containing time,state,freq,flow,mls,total,fsw)
 *  - BUILT-IN PUSH BUTTON LED: Process & alarm illumination driver.
 * ============================================================================
 */

#include <Arduino.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/pulse_cnt.h"

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

// ======================= PHYSICAL PINOUT (LAM STAGE 2 PCB REV 1.0) =======================
// Strict alignment with official schematic drawn by Syed Tajammul Hassan (T-Works Foundation):
//  - SW (Pin 5): GPIO 34 (ON/OFF Switch with 10k R2 pull-up to 3V3 & 100nF C5)
//  - FSW (Pin 6): GPIO 35 (Sump Float Switch with 10k R10 pull-up to 3V3 & 100nF C9)
//  - FS (Pin 12): GPIO 14 (Flow Sensor FT-01 Primary connected to JST XH 3-Pin Pin 1)
//  - SV (Pin 11): GPIO 27 (Solenoid Valve OV-01 Primary connected to JST XH 3-Pin Pin 2)
static const int PIN_SW         = 34;  // ON/OFF Tactile Switch (GPIO 34, Pin 5 on U7)
static const int PIN_FSW        = 35;  // Sump Float Switch (GPIO 35, Pin 6 on U7, 10k R10 pullup)
static const int PIN_FS         = 14;  // Flow Sensor FT-01 Primary (GPIO 14, Pin 12 on U7 labeled FS)
static const int PIN_SV         = 27;  // Solenoid Valve Driver OV-01 Primary (GPIO 27, Pin 11 on U7)

// BTS7960 Motor Driver Connectors (U2, U3, U4)
// Schematic:
//  - U2 (Pump 1): L_EN1=GPIO 13 (Pin 15 on U7), R_EN1=26 (Pin 10), L_PWM1=32 (Pin 7), R_PWM1=33 (Pin 8)
//  - U3 (Pump 2): L_EN2=16 (Pin 27), R_EN2=17 (Pin 28), L_PWM2=18 (Pin 30), R_PWM2=19 (Pin 31)
//  - U4 (Pump 3): L_EN3=21 (Pin 33), R_EN3=22 (Pin 36), L_PWM3=23 (Pin 37), R_PWM3=25 (Pin 9)
struct PumpPins {
    uint8_t lEn;
    uint8_t rEn;
    uint8_t lPwm;
    uint8_t rPwm;
};

static const PumpPins PUMP[3] = {
    {13, 26, 32, 33},  // Pump 1 (U2): L_EN1=GPIO 13 (Pin 15 on U7), R_EN1=26, L_PWM1=32, R_PWM1=33
    {16, 17, 18, 19},  // Pump 2 (U3): L_EN2=16, R_EN2=17, L_PWM2=18, R_PWM2=19
    {21, 22, 23, 25}   // Pump 3 (U4): L_EN3=21, R_EN3=22, L_PWM3=23, R_PWM3=25
};

// ======================= OPERATING PARAMETERS (PER CHALLENGE PDF) =======================
// Switch & Startup Configuration
static const bool     USE_PHYSICAL_SWITCH  = true;    // true = push-button acts as Master Start / Safe Stop
static const bool     AUTO_START_ON_BOOT   = false;   // false = waits on standby for button press or serial command

static const uint32_t PWM_FREQ             = 5000;    // 5 kHz PWM (reliable for BTS7960 optocoupler inputs)
static const uint8_t  PWM_BITS             = 8;       // 8-bit resolution (0 - 255)
static bool           PUMP_REVERSE         = false;   // Swap direction in software (toggles GPIO 32 vs 33)
static const bool     FLOAT_CLOSED_IS_HIGH = true;    // Float switch closed (pin LOW) = Sump level HIGH

// Case 1 Operating Duties (PDF Page 3: "Pump-01 runs on 50% duty, Pump-02 runs on 100% duty")
static const float    PUMP1_C1_DUTY        = 50.0f;   // 50% Duty Cycle
static const float    PUMP2_C1_DUTY        = 100.0f;  // 100% Duty Cycle

// Case 2 Flush Operating Duties (PDF Page 3: "all three pumps operate simultaneously")
static const float    PUMP1_C2_DUTY        = 50.0f;   // 50% Duty Cycle
static const float    PUMP2_C2_DUTY        = 100.0f;  // 100% Duty Cycle
static const float    PUMP3_C2_DUTY        = 100.0f;  // 100% Flush Delivery (paper gives no Case 2 duty; team choice)

// Flow Sensor Calibration: Q_Lmin = CAL_A * freq_Hz + CAL_B (fitted constants below; factory YF-S401 datasheet was F = 98*Q - 3)
static float          ppl                  = 5880.0f;   // 98 Hz * 60 s = 5880 pulses/L
static const float    FLOW_CUTOFF_HZ       = 1.0f;      // Noise gate: ignore rotor vibrations < 1 Hz
static const float    FLOW_MIN_VALID       = 0.50f;     // Lower physical flow boundary (L/min) - tuned above 0.285 offset for clean dry detection
static const float    C2_FLOW_COLLAPSE_LPM = 0.35f;     // Air ingestion collapse threshold (L/min): true air collapse drops near 0 L/min (< 0.35 L/min)
static const float    FLUSH_CEILING_L      = 4.15f;     // PDF Spec: Stop as soon as 4.2L flush tank finishes (4.15L accounts for line hold-up)
static const uint32_t C2_STARTUP_GRACE_MS  = 10000;    // 10s startup grace window: prevents ramp-up transients from tripping air-suction
static const float    C2_MIN_FLUSH_ARM_L   = 0.30f;     // Minimum 300 mL delivered before air suction cutoff is armed

// Timing Intervals
static const uint32_t C1_GRACE_MS          = 4000;    // Startup grace period before dry-run protection
static const uint32_t C1_NOFLOW_FAULT_MS   = 6000;    // No-flow trip threshold (dry run / blockage)
static const uint32_t C2_PRIME_MS          = 25000;   // 25s flush line priming window (allows water to fill dry pipes to FT-01)
static const uint32_t C2_EXHAUST_MS        = 1000;    // Fast 1.0s zero-flow / air-suction persistence
static const uint32_t C1_RECOVERY_DWELL_MS = 20000;   // After surge recovery: keep normal Case 1 recirculation for 20s, then transition directly to Case 2 flush
static const uint32_t C1_DRAIN_EMPTY_MS    = 3000;    // Drain phase: flow below FLOW_MIN_VALID for this long => sump is empty
static const uint32_t C1_DRAIN_MAX_MS      = 45000;   // Drain safety timeout: 45s allows full 1.5-2.0L sump evacuation before advancing to Case 2
static const uint32_t SURGE_CLEAR_CONFIRM_MS = 3000;  // Float must stay clear this long before Pump-01 restarts (anti short-cycling)
// Case 2 Flush Shutdown Timer: Set to 1 min 30 s (90 s)
// for all three pumps (P1=50%, P2=100%, P3=100%) to completely evacuate the 4.2 L flush tank.
static const uint32_t C2_AUTO_MAX_RUN_MS   = 90000;

// Calibration mode guards ('calmode on' / 'calmode off')
static const uint32_t CALMODE_IDLE_TIMEOUT_MS = 120000; // No command for 2 min -> pumps off, leave calibration mode
static const uint32_t CALMODE_NOFLOW_STOP_MS  = 5000;   // Pump-2 running with ~zero flow this long -> Pump-2 off (dry sump / hose pulled)
static const float    CALMODE_NOFLOW_LPM      = 0.20f;  // Flow below this counts as 'no flow' in calibration mode
static const uint32_t C2_MAX_RUN_MS        = 600000;  // 10-minute global safety cutoff
static const uint32_t SOV_CLOSE_DELAY_MS   = 500;     // Staggered valve shutdown (anti-water-hammer)
static const uint32_t SOV_PRE_OPEN_MS      = 200;     // Lead-time: Open solenoid 200ms before pumps to prime suction line
static bool           SOV_ACTIVE_HIGH      = false;   // false = Active-LOW relay module (typical 5V opto module: relay ON when input pulled LOW); true = Active-HIGH
static const bool     SOV_HIGH_BY_FLOAT    = true;    // true = a logical HIGH is produced by FLOATING the pin (module's own 5V pull-up lifts it). A 3.3V GPIO cannot fully release a 5V-pulled input.

static const uint32_t DEBOUNCE_MS          = 30;      // Tactile button debounce
static const uint32_t LONG_PRESS_MS        = 1500;    // Short vs long press threshold
static const uint32_t FLOAT_DEBOUNCE_MS    = 250;     // Float switch debounce filter

// ======================= SPECIFICATION CALIBRATION & TELEMETRY STATE =======================
// Calibration model: Q_Lmin = max(CAL_A * freq_Hz + CAL_B, 0)
// Empirically calibrated on physical test rig for YF-S401 (6 trials, 0.775-2.040 L/min, least-squares fit via 'FIT'):
// R^2 = 0.99668, Avg Error = 1.62%, Max Error = 3.66%. Independent validation trial at 1.110 L/min: error 1.06%.
// Earlier preliminary calibration (superseded): CAL_A = 0.007692, CAL_B = 0.284842.
// 'APPLY' only updates these in RAM; edit the two constants below to keep a fit across reboots.
static double         CAL_A                = 0.009244;
static double         CAL_B                = 0.040111;

// Window timing (default 1.0 s nominal window)
static double         windowNominalSec     = 1.0;
static int64_t        tPrevUs              = 0;

// Double precision live telemetry fields (computed once per window)
static double         time_s               = 0.0;
static int64_t        cnt_prev             = 0;
static double         window_s             = 1.0;
static double         freq_Hz              = 0.0;
static double         Q_Lmin               = 0.0;
static double         Q_mLs                = 0.0;
static double         total_L              = 0.0;
static uint64_t       total_pulses         = 0;

// ESP32 PCNT (Pulse Counter) Hardware State
static pcnt_unit_handle_t    pcntUnit      = NULL;
static pcnt_channel_handle_t pcntChan      = NULL;
static int                   lastRawPcnt   = 0;
static int64_t               totalPcntAccum= 0;
static const int             PCNT_LOW_LIMIT  = -32768;
static const int             PCNT_HIGH_LIMIT = 32767;
static int64_t               lastWindowN   = 0;   // Pulses counted in the most recent telemetry window

// Calibration Run State & Fields (Between 'S' and 'E')
static bool           calRunActive         = false;
static bool           calRunEnded          = false;
static bool           calHoldingResult     = false;
static int64_t        calStartUs           = 0;
static int64_t        calEndUs             = 0;
static uint64_t       calPulsesAtStart     = 0;

static uint64_t       run_pulses           = 0;
static double         run_time_s           = 0.0;
static double         run_freq_Hz          = 0.0;
static double         Q_actual_Lmin        = 0.0;
static double         error_pct            = 0.0;
static double         V_collected          = 0.0;

// On-demand Regression Fit & Trial History
struct CalTrial {
    int trialId;
    uint64_t pulses;
    double time_s;
    double freq_Hz;
    double vol_L;
    double Q_actual;
    double error_pct;
};

static const int      MAX_TRIALS           = 20;
static CalTrial       trials[MAX_TRIALS];
static int            trialCount           = 0;

static double         fit_a                = 0.0;
static double         fit_b                = 0.0;
static double         fit_r2               = 0.0;
static double         fit_max_err          = 0.0;
static double         fit_avg_err          = 0.0;
static bool           fit_valid            = false;

// ======================= FINITE STATE MACHINE (FSM) =======================
enum SystemState {
    ST_IDLE             = 0,  // Standby: All pumps OFF, Solenoid CLOSED
    ST_CASE1_NORMAL     = 1,  // Case 1 Normal: SOV OPEN, P1 @ 50%, P2 @ 100%
    ST_CASE1_SURGE      = 2,  // Case 1 Surge: Sump HIGH -> P1 STOPPED (0%), P2 @ 100%
    ST_CASE2_FLUSHING   = 3,  // Case 2 Flush: SOV CLOSED (10L Isolated), P1, P2, P3 ALL ON
    ST_CASE2_STOPPING   = 4,  // Case 2 Staged Stop: Pumps OFF, awaiting anti-hammer SOV closure
    ST_FAULT            = 5,  // System Tripped: Dry running, pipe blockage, or timeout
    ST_CASE1_DRAIN      = 6,  // Post-surge: SOV CLOSED, P1 OFF, P2 @ 100% drains sump completely before Case 2
    ST_CALMODE          = 7   // Bench calibration mode: manual pump/valve control, button start disabled, guarded
};

static SystemState state = ST_IDLE;
static bool singleGoMode = false;        // When true, runs Case 1 then auto-transitions to Case 2
static bool surgeDemonstrated = false;   // Flag indicating sump level surge has been mitigated
static uint32_t surgeClearMs = 0;        // Timestamp when surge condition cleared
static uint32_t surgeFloatClearMs = 0;   // Timestamp when float first read clear during a surge (0 = still high)
static bool masterInterlockArmed = false; // Master Safety Interlock (true = armed/running, false = safe locked)
static bool flushFlowConfirmed = false;   // True once flush fluid is actively verified flowing through FT-01
static uint32_t c2CustomDurationMs = 0;   // Optional timed duration in ms for bench testing (0 = autonomous PS mode)
static uint8_t  c2LowFlowSamples   = 0;   // Counter for consecutive air-suction low flow samples
static uint32_t lastC2PrintMs      = 0;   // Periodic console telemetry during Case 2
static float    pumpDutyPct[3]     = {0.0f, 0.0f, 0.0f}; // Last commanded duty per pump (used by calibration-mode guards)
static uint32_t calModeLastCmdMs   = 0;   // Time of last serial command (calibration-mode idle timeout)
static uint32_t calModeNoFlowSinceMs = 0; // Pump-2 no-flow timer in calibration mode

// Legacy FSM variables kept for full backward compatibility
static volatile uint32_t pulseCount     = 0;
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

// ======================= HARDWARE INTERRUPT SERVICE ROUTINES =======================
static volatile uint32_t pulseCount14   = 0;
static volatile uint32_t pulseCount35   = 0;
static volatile uint32_t pulseCount34   = 0;

void IRAM_ATTR onFlowPulseISR14() {
    pulseCount14 = pulseCount14 + 1;
}

void IRAM_ATTR onFlowPulseISR35() {
    pulseCount35 = pulseCount35 + 1;
}

void IRAM_ATTR onFlowPulseISR34() {
    pulseCount34 = pulseCount34 + 1;
}

// ======================= ESP32 PCNT (PULSE COUNTER) DRIVER =======================
void initPCNT() {
    pcnt_unit_config_t unit_config = {};
    unit_config.low_limit = PCNT_LOW_LIMIT;
    unit_config.high_limit = PCNT_HIGH_LIMIT;

    esp_err_t err = pcnt_new_unit(&unit_config, &pcntUnit);
    if (err == ESP_OK) {
        pcnt_chan_config_t chan_config = {};
        chan_config.edge_gpio_num = PIN_FS;
        chan_config.level_gpio_num = -1;

        if (pcnt_new_channel(pcntUnit, &chan_config, &pcntChan) == ESP_OK) {
            pcnt_channel_set_edge_action(pcntChan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
            pcnt_channel_set_level_action(pcntChan, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_KEEP);
        }

        pcnt_glitch_filter_config_t filter_config = {};
        filter_config.max_glitch_ns = 1000; // 1 us glitch filter
        pcnt_unit_set_glitch_filter(pcntUnit, &filter_config);

        pcnt_unit_enable(pcntUnit);
        pcnt_unit_clear_count(pcntUnit);
        pcnt_unit_start(pcntUnit);
    }
}

int64_t getPcntCount() {
    if (pcntUnit != NULL) {
        int count = 0;
        pcnt_unit_get_count(pcntUnit, &count);
        int32_t diff = count - lastRawPcnt;
        if (diff < 0) {
            diff += (PCNT_HIGH_LIMIT + 1); // Counter clears to 0 at the high limit -> true wrap length is 32768, not 65536
        }
        lastRawPcnt = count;
        totalPcntAccum += diff;
        return totalPcntAccum;
    } else {
        noInterrupts();
        uint32_t p = pulseCount14;
        interrupts();
        return (int64_t)p;
    }
}

// ======================= PWM HARDWARE DRIVER =======================
// Compatible with both Arduino-ESP32 Core 2.x (PlatformIO default) and Core 3.x (ESP-IDF native)
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
    pumpDutyPct[pumpIdx] = dutyPct;
    uint32_t v = (uint32_t)(dutyPct * ((1 << PWM_BITS) - 1) / 100.0f);

    bool on = (v > 0);

    if (pumpIdx == 0) {
        // Pump 1 (U2): Schematic has L_EN1 on GPIO 13 (U7 Pin 15) and R_EN1 on GPIO 26 (U7 Pin 10).
        // We energize BOTH GPIO 13 and GPIO 12 HIGH to ensure enable regardless of board routing!
        digitalWrite(13, on ? HIGH : LOW);
        digitalWrite(12, on ? HIGH : LOW);
        digitalWrite(PUMP[0].rEn, on ? HIGH : LOW); // GPIO 26

        // Forward PWM: L_PWM1 (GPIO 32). Reverse PWM: R_PWM1 (GPIO 33).
        uint8_t fwdPin = PUMP_REVERSE ? PUMP[0].rPwm : PUMP[0].lPwm;
        uint8_t revPin = PUMP_REVERSE ? PUMP[0].lPwm : PUMP[0].rPwm;
        PWM_WRITE(fwdPin, 0, on ? v : 0);
        PWM_WRITE(revPin, 1, 0);
    } else if (pumpIdx == 1) {
        // Pump 2 (U3): L_EN2=GPIO 16, R_EN2=GPIO 17, L_PWM2=GPIO 18
        digitalWrite(PUMP[1].lEn, on ? HIGH : LOW);
        digitalWrite(PUMP[1].rEn, on ? HIGH : LOW);
        PWM_WRITE(PUMP[1].lPwm, 2, on ? v : 0);
    } else if (pumpIdx == 2) {
        // Pump 3 (U4): L_EN3=GPIO 21, R_EN3=GPIO 22, L_PWM3=GPIO 23
        digitalWrite(PUMP[2].lEn, on ? HIGH : LOW);
        digitalWrite(PUMP[2].rEn, on ? HIGH : LOW);
        PWM_WRITE(PUMP[2].lPwm, 4, on ? v : 0);
    }
}

void setAllPumps(float d1, float d2, float d3) {
    setPump(0, d1);
    setPump(1, d2);
    setPump(2, d3);
}

void pumpsOff() {
    setAllPumps(0.0f, 0.0f, 0.0f);
}

// Relay/solenoid line driver. energize=true -> relay ON (solenoid OPEN), false -> relay OFF (solenoid CLOSED).
// Only LOW is ever actively driven; a HIGH level is made by releasing the pin (high-Z) so a 5V-powered relay module
// sees its own pull-up (true 5V) and reliably de-energizes. Driving 3.3V into a 5V-pulled input leaves the relay stuck ON.
static void sovDrive(bool energize) {
    bool lineHigh = SOV_ACTIVE_HIGH ? energize : !energize;
    if (lineHigh && SOV_HIGH_BY_FLOAT && !SOV_ACTIVE_HIGH) {   // float only for active-LOW modules (they have the 5V pull-up)
        pinMode(PIN_SV, INPUT);          // release: output register stays 0, pin floats to module pull-up
    } else {
        pinMode(PIN_SV, OUTPUT);
        digitalWrite(PIN_SV, lineHigh ? HIGH : LOW);
    }
}

void sovOpen() {
    sovDrive(true);
    Serial.printf("[ACTUATOR] Solenoid Valve OV-01 (GPIO %d) -> ENERGIZED / OPEN (%s)\n",
                  PIN_SV, SOV_ACTIVE_HIGH ? (SOV_HIGH_BY_FLOAT ? "released/float" : "HIGH 3.3V") : "driven LOW 0V");
}

void sovClose() {
    sovDrive(false);
    Serial.printf("[ACTUATOR] Solenoid Valve OV-01 (GPIO %d) -> DE-ENERGIZED / CLOSED (%s)\n",
                  PIN_SV, SOV_ACTIVE_HIGH ? "driven LOW 0V" : (SOV_HIGH_BY_FLOAT ? "released/float" : "HIGH 3.3V"));
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
    masterInterlockArmed = false; // Disarm interlock -> locked in safe stop
    calRunActive = false;         // An emergency stop cancels any open calibration run
    calModeNoFlowSinceMs = 0;
    Serial.printf("[STATE] -> IDLE (%s) | Master Safety Interlock: LOCKED (OFF)\n", reason);
}

void enterFault(const char* reason) {
    allOff();
    state = ST_FAULT;
    singleGoMode = false;
    masterInterlockArmed = false;
    faultMsg = reason;
    Serial.printf("[FAULT] TRIP: %s | Press switch or enter 'rt'/'stop' to clear.\n", reason);
}

void startCase1Normal() {
    masterInterlockArmed = true; // Automatically arm when Case 1 is triggered
    surgeFloatClearMs = 0;

    // Fresh run: zero the totalizers so 'total transferred litres' covers this demonstration only
    total_L = 0.0;
    totalL = 0.0f;
    totalFlushL = 0.0f;
    state = ST_CASE1_NORMAL;
    stateStartMs = millis();
    lowFlowSinceMs = 0;

    // 1. Instantly operate Solenoid Valve OV-01 at the very start of Case 1
    sovOpen();
    Serial.println("[CASE 1] Solenoid Valve OV-01 actively opened at Case 1 initiation.");

    // 2. Hydraulic priming & motor inrush mitigation:
    // Allow 200ms for solenoid valve to seat fully open and flood Pump 1 suction line
    if (SOV_PRE_OPEN_MS > 0) {
        delay(SOV_PRE_OPEN_MS);
    }

    // 3. Energize recirculation pumps per specification (P1 @ 50%, P2 @ 100%)
    setAllPumps(PUMP1_C1_DUTY, PUMP2_C1_DUTY, 0.0f);
    Serial.println("[STATE] -> CASE 1 NORMAL: Recirculation active (SOV=OPEN, P1=50%, P2=100%).");
}

// Resume normal recirculation after a surge: solenoid is already open, so no priming delay and NO totalizer reset.
void resumeCase1AfterSurge() {
    state = ST_CASE1_NORMAL;
    stateStartMs = millis();
    lowFlowSinceMs = 0;
    setAllPumps(PUMP1_C1_DUTY, PUMP2_C1_DUTY, 0.0f);
    Serial.println("[STATE] -> CASE 1 NORMAL (resumed after surge): P1=50%, P2=100%.");
}

// Post-surge drain: isolate the 10L tank, stop Pump-01, keep Pump-02 @ 100% until the sump is dry (FT-01 flow collapses).
void enterCase1Drain() {
    state = ST_CASE1_DRAIN;
    stateStartMs = millis();
    lowFlowSinceMs = 0;
    sovClose();
    setAllPumps(0.0f, PUMP2_C1_DUTY, 0.0f);
    Serial.println("[DRAIN] Sump drain started: SOV CLOSED, P1 OFF, P2 @ 100%, P3 OFF. Waiting for sump to empty...");
}

void enterCase1Surge() {
    state = ST_CASE1_SURGE;
    surgeFloatClearMs = 0;
    stateStartMs = millis();

    // Sump Level Interlock (PDF Page 3): Stop Pump-01 immediately, keep Pump-02 running @ 100%
    sovOpen();
    setAllPumps(0.0f, PUMP2_C1_DUTY, 0.0f); // P1 CUT OFF, P2 DRAINING @ 100%
    Serial.println("[INTERLOCK] -> SUMP HIGH SURGE: Pump-01 STOPPED (0%), Pump-02 draining (100%).");
}

void startCase2Flushing(uint32_t customDurationSec = 0) {
    state = ST_CASE2_FLUSHING;
    stateStartMs = millis();
    lowFlowSinceMs = 0;
    c2LowFlowSamples = 0;
    lastC2PrintMs = 0;
    totalFlushL = 0.0f;
    flushFlowConfirmed = false;
    c2CustomDurationMs = customDurationSec * 1000UL;

    // Isolate 10L Tank (PDF Page 3: "The On/Off valve must be switched off in order to isolate 10L tank")
    sovClose();

    // All three pumps operate simultaneously per Case 2 specification
    setAllPumps(PUMP1_C2_DUTY, PUMP2_C2_DUTY, PUMP3_C2_DUTY);
    if (customDurationSec > 0) {
        Serial.printf("[STATE] -> CASE 2 FLUSHING (TIMED): Running all 3 pumps for %lu seconds.\n", (unsigned long)customDurationSec);
    } else {
        Serial.println("[STATE] -> CASE 2 FLUSHING: 10L Tank ISOLATED. P1, P2, P3 running simultaneously.");
        Serial.println("[C2] Priming line from 4.2L Flush Tank (TK-03). Monitoring FT-01...");
    }
}

void stopAll(const char* reason) {
    enterIdle(reason);
}

// ---- Guarded calibration mode: lets an engineer run pumps/valve by hand and use S/E/V/FIT/APPLY safely ----
void enterCalMode() {
    allOff();
    state = ST_CALMODE;
    stateStartMs = millis();
    calModeLastCmdMs = millis();
    calModeNoFlowSinceMs = 0;
    singleGoMode = false;
    masterInterlockArmed = true;
    Serial.println("\n============================================================");
    Serial.println("  [CALMODE] CALIBRATION MODE ON");
    Serial.println("  Pumps OFF, solenoid CLOSED. Use p1/p2/p3, sov, S/E/V, FIT, APPLY.");
    Serial.println("  Button start is DISABLED (button = stop). Process start commands blocked.");
    Serial.println("  Guards: float HIGH cuts Pump-1 | Pump-2 stops after 5 s of no flow |");
    Serial.println("  everything stops after 120 s with no command. Type 'calmode off' to leave.");
    Serial.println("============================================================");
}

void exitCalMode(const char* reason) {
    allOff();
    calRunActive = false;
    calRunEnded = false;
    calHoldingResult = false;
    state = ST_IDLE;
    singleGoMode = false;
    masterInterlockArmed = false;
    calModeNoFlowSinceMs = 0;
    Serial.println("\n============================================================");
    Serial.printf("  [CALMODE] CALIBRATION MODE OFF (%s)\n", reason);
    Serial.println("  Pumps OFF, solenoid CLOSED, any open run cancelled. System IDLE.");
    Serial.printf("  Trials recorded: %d | Active CAL_A = %.6f, CAL_B = %.6f\n", trialCount, CAL_A, CAL_B);
    Serial.println("  'APPLY' only changes RAM: copy CAL_A/CAL_B into main.cpp and reflash to keep them.");
    Serial.println("============================================================");
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
    if (state == ST_CALMODE) {
        // Engineer is calibrating: the button must never start the process. It acts as a plain stop.
        pumpsOff();
        sovClose();
        Serial.println("[SWITCH] Calibration mode: button start disabled. Pumps OFF, solenoid CLOSED. Type 'calmode off' to leave.");
        return;
    }
    if (state == ST_IDLE) {
        // Master Start: Arms Safety Interlock and starts automated sequence
        masterInterlockArmed = true;
        Serial.println("\n============================================================");
        Serial.println("  [SWITCH] MASTER SAFETY INTERLOCK: ARMED & RUNNING");
        Serial.println("  Initiating Case 1 Closed-Loop Recirculation...");
        Serial.println("============================================================");
        singleGoMode = true; // Automatically advances to Case 2 after surge test per challenge rules
        startCase1Normal();
    } else if (state == ST_FAULT) {
        masterInterlockArmed = false;
        enterIdle("Fault cleared by operator via switch");
    } else {
        // Running in ANY state (Case 1 Normal, Case 1 Surge, Case 2 Flush) -> SAFE SHUTDOWN OF ENTIRE PROCESS
        masterInterlockArmed = false;
        Serial.println("\n============================================================");
        Serial.println("  [SWITCH] MASTER STOP TRIPPED: Operator pressed switch.");
        Serial.println("  All pumps CUT OFF. Solenoid CLOSED. System in SAFE STOP.");
        Serial.println("============================================================");
        stopAll("Pushbutton Master Stop Command");
    }
}

void onLongPress() {
    // Hold > 1.5s: Instant hard emergency stop / disarm
    masterInterlockArmed = false;
    Serial.println("\n[SWITCH] EMERGENCY STOP: Button held > 1.5s. Immediate total shutdown.");
    stopAll("Emergency Long-Press Stop");
}

void handleTactileButton() {
    if (!USE_PHYSICAL_SWITCH) return; // Physical switch bypassed

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

// ======================= CALIBRATION ANALYSIS & FIT ROUTINES =======================
void calculateFitAndPrint(bool brief = false) {
    if (trialCount == 0) {
        Serial.println("[CAL] No calibration trials recorded yet. Run 'S' -> 'E' -> 'V <vol>' to add trials.");
        return;
    }

    Serial.println("\n================================================================================");
    Serial.println("           FT-01 SENSOR CALIBRATION REGRESSION & ERROR ANALYSIS");
    Serial.println("           Model: Q = a * f + b  (f in Hz, Q in L/min)");
    Serial.println("================================================================================");
    Serial.printf("%-6s | %-10s | %-9s | %-11s | %-9s | %-12s | %-10s | %-9s\n",
                  "Trial", "Pulses(N)", "Time(s)", "Freq(Hz)", "Vol(L)", "Q_act(L/min)", "Q_cal(L/min)", "Error(%)");
    Serial.println("--------------------------------------------------------------------------------");

    double sum_f = 0.0;
    double sum_Q = 0.0;
    double sum_f2 = 0.0;
    double sum_fQ = 0.0;

    for (int i = 0; i < trialCount; i++) {
        double f = trials[i].freq_Hz;
        double Q = trials[i].Q_actual;
        sum_f += f;
        sum_Q += Q;
        sum_f2 += f * f;
        sum_fQ += f * Q;
    }

    int n = trialCount;
    double denom = ((double)n * sum_f2) - (sum_f * sum_f);

    if (n >= 2 && fabs(denom) > 1e-9) {
        // Fit slope: a = (n*Σ(fQ) - Σf*ΣQ) / (n*Σf² - (Σf)²)
        fit_a = (((double)n * sum_fQ) - (sum_f * sum_Q)) / denom;
        // Fit offset: b = (ΣQ - a*Σf) / n
        fit_b = (sum_Q - (fit_a * sum_f)) / (double)n;
        fit_valid = true;

        double Q_mean = sum_Q / (double)n;
        double ss_res = 0.0;
        double ss_tot = 0.0;
        double max_e = 0.0;
        double sum_e = 0.0;

        for (int i = 0; i < n; i++) {
            double f = trials[i].freq_Hz;
            double Q_act = trials[i].Q_actual;
            double Q_cal = (fit_a * f) + fit_b;
            double res = Q_act - Q_cal;
            ss_res += res * res;
            ss_tot += (Q_act - Q_mean) * (Q_act - Q_mean);

            double err = (Q_act > 0.0) ? (fabs(res) / Q_act * 100.0) : 0.0;
            if (err > max_e) max_e = err;
            sum_e += err;

            Serial.printf("#%-5d | %-10llu | %-9.3f | %-11.2f | %-9.3f | %-12.3f | %-10.3f | %-9.2f\n",
                          trials[i].trialId, (unsigned long long)trials[i].pulses, trials[i].time_s,
                          f, trials[i].vol_L, Q_act, Q_cal, err);
        }

        // Coefficient of determination: R² = 1 - Σ(Q_act - Q_cal)² / Σ(Q_act - Q_mean)²
        fit_r2 = (ss_tot > 1e-12) ? (1.0 - (ss_res / ss_tot)) : 1.0;
        fit_max_err = max_e;
        fit_avg_err = sum_e / (double)n;

        Serial.println("================================================================================");
        Serial.println("                         REGRESSION FIT SUMMARY RESULTS");
        Serial.println("================================================================================");
        Serial.printf("  Fitted Slope (a)                   : %.6f L/min per Hz\n", fit_a);
        Serial.printf("  Fitted Offset (b)                  : %.6f L/min\n", fit_b);
        Serial.printf("  Determination Coeff (R^2)          : %.5f\n", fit_r2);
        Serial.printf("  Max Calibration Error              : %.2f %%\n", fit_max_err);
        Serial.printf("  Average Calibration Error          : %.2f %%\n", fit_avg_err);
        Serial.printf("  Current Active Calibration in Frame: CAL_A = %.6f, CAL_B = %.6f\n", CAL_A, CAL_B);
        Serial.println("--------------------------------------------------------------------------------");
        Serial.println("  >> Type 'APPLY' to update CAL_A and CAL_B and verify that error_pct drops! <<");
        Serial.println("================================================================================\n");
    } else {
        fit_valid = false;
        for (int i = 0; i < n; i++) {
            double q_cur = (CAL_A * trials[i].freq_Hz) + CAL_B;
            double k = (trials[i].vol_L > 0.0) ? ((double)trials[i].pulses / trials[i].vol_L) : 0.0;
            Serial.printf("#%-5d | %-10llu | %-9.3f | %-11.2f | %-9.3f | %-12.3f | %-10.3f | %-9.2f (K=%.1f)\n",
                          trials[i].trialId, (unsigned long long)trials[i].pulses, trials[i].time_s,
                          trials[i].freq_Hz, trials[i].vol_L, trials[i].Q_actual, q_cur, trials[i].error_pct, k);
        }
        Serial.println("--------------------------------------------------------------------------------");
        Serial.println("  (Note: Record at least 2 distinct flow trials to compute linear regression fit a & b)");
        Serial.println("================================================================================\n");
    }
}

void startCalibrationRun() {
    calRunActive = true;
    calRunEnded = false;
    calHoldingResult = false;
    calStartUs = esp_timer_get_time();
    calPulsesAtStart = (uint64_t)getPcntCount();   // live PCNT read, not the 1 s windowed total
    run_pulses = 0;
    run_time_s = 0.0;
    run_freq_Hz = 0.0;
    Q_actual_Lmin = 0.0;
    error_pct = 0.0;
    V_collected = 0.0;
    Serial.println("\n[CAL] >>> Calibration run STARTED ('S'). Dispensing fluid into container...");
    Serial.println("[CAL] When collection finishes, send 'E' to end the run.");
}

void endCalibrationRun() {
    if (!calRunActive) {
        Serial.println("[CAL] No active run. Send 'S' first to begin a calibration run.");
        return;
    }
    calEndUs = esp_timer_get_time();
    calRunActive = false;
    calRunEnded = true;
    calHoldingResult = true;
    uint64_t endCount = (uint64_t)getPcntCount();   // live PCNT read, aligned with calEndUs
    run_pulses = (endCount >= calPulsesAtStart) ? (endCount - calPulsesAtStart) : 0;
    run_time_s = (double)(calEndUs - calStartUs) / 1e6;
    run_freq_Hz = (run_time_s > 0.0) ? ((double)run_pulses / run_time_s) : 0.0;
    Q_actual_Lmin = 0.0;
    error_pct = 0.0;
    Serial.println("\n[CAL] >>> Calibration run ENDED ('E').");
    Serial.printf("[CAL] Pulses: %llu | Duration: %.3f s | Mean Freq: %.2f Hz\n",
                  (unsigned long long)run_pulses, run_time_s, run_freq_Hz);
    Serial.println("[CAL] Please enter measured collection volume: 'V <litres>' (e.g. 'V 1.00' or simply '1.00')");
}

void recordCollectedVolume(double volL) {
    if (volL <= 0.0) {
        Serial.println("[CAL] Error: Collected volume must be > 0.0 Litres.");
        return;
    }
    if (!calRunEnded && !calHoldingResult) {
        Serial.println("[CAL] Error: No completed run waiting for volume. Send 'S' then 'E' first.");
        return;
    }
    V_collected = volL;
    // Q_actual_Lmin = V_collected / run_time_s * 60.0
    Q_actual_Lmin = (run_time_s > 0.0) ? (V_collected / run_time_s * 60.0) : 0.0;

    // Q_Lmin_run = CAL_A * run_freq_Hz + CAL_B
    double Q_Lmin_run = (CAL_A * run_freq_Hz) + CAL_B;

    // error_pct = fabs(Q_Lmin_run - Q_actual_Lmin) / Q_actual_Lmin * 100.0
    error_pct = (Q_actual_Lmin > 0.0) ? (fabs(Q_Lmin_run - Q_actual_Lmin) / Q_actual_Lmin * 100.0) : 0.0;

    // K-factor: K = run_pulses / V (pulses/L)
    double k_factor = (V_collected > 0.0) ? ((double)run_pulses / V_collected) : 0.0;

    if (trialCount < MAX_TRIALS) {
        trials[trialCount].trialId = trialCount + 1;
        trials[trialCount].pulses = run_pulses;
        trials[trialCount].time_s = run_time_s;
        trials[trialCount].freq_Hz = run_freq_Hz;
        trials[trialCount].vol_L = V_collected;
        trials[trialCount].Q_actual = Q_actual_Lmin;
        trials[trialCount].error_pct = error_pct;
        trialCount++;
    }

    Serial.println("\n================================================================================");
    Serial.printf("                    CALIBRATION TRIAL #%d RECORDED\n", trialCount);
    Serial.println("================================================================================");
    Serial.printf("  Run Pulses (N)      : %llu\n", (unsigned long long)run_pulses);
    Serial.printf("  Run Time (t)        : %.3f s\n", run_time_s);
    Serial.printf("  Mean Frequency (f)  : %.2f Hz\n", run_freq_Hz);
    Serial.printf("  Collected Volume (V): %.3f L\n", V_collected);
    Serial.printf("  Actual Flow (Q_act) : %.3f L/min\n", Q_actual_Lmin);
    Serial.printf("  K-Factor (K)        : %.1f pulses/L\n", k_factor);
    Serial.printf("  Model Flow (Q_cal)  : %.3f L/min (with CAL_A=%.6f, CAL_B=%.6f)\n", Q_Lmin_run, CAL_A, CAL_B);
    Serial.printf("  Calibration Error   : %.2f %%\n", error_pct);
    Serial.println("--------------------------------------------------------------------------------");

    calculateFitAndPrint(true);
}

void applyCalibrationFit() {
    if (!fit_valid) {
        Serial.println("[CAL] Error: No valid regression fit to apply. Record at least 2 trials first.");
        return;
    }
    double old_a = CAL_A;
    double old_b = CAL_B;
    CAL_A = fit_a;
    CAL_B = fit_b;

    Serial.println("\n================================================================================");
    Serial.println("              CALIBRATION PARAMETERS UPDATED SUCCESSFULLY!");
    Serial.println("================================================================================");
    Serial.printf("  CAL_A : %.6f  -->  %.6f (Fitted Slope a)\n", old_a, CAL_A);
    Serial.printf("  CAL_B : %.6f  -->  %.6f (Fitted Offset b)\n", old_b, CAL_B);
    Serial.println("--------------------------------------------------------------------------------");
    Serial.println("  Recalculating errors for all trials with new calibration constants:");

    double old_max_err = 0.0;
    double old_sum_err = 0.0;
    double new_max_err = 0.0;
    double new_sum_err = 0.0;
    for (int i = 0; i < trialCount; i++) {
        double q_cal = (CAL_A * trials[i].freq_Hz) + CAL_B;
        double q_act = trials[i].Q_actual;
        double old_err = (q_act > 0.0) ? (fabs((old_a * trials[i].freq_Hz) + old_b - q_act) / q_act * 100.0) : 0.0;
        if (old_err > old_max_err) old_max_err = old_err;
        old_sum_err += old_err;
        double new_err = (q_act > 0.0) ? (fabs(q_cal - q_act) / q_act * 100.0) : 0.0;
        trials[i].error_pct = new_err;
        if (new_err > new_max_err) new_max_err = new_err;
        new_sum_err += new_err;
        Serial.printf("  Trial #%d (f=%.2f Hz, Q=%.2f L/min): Error %.2f %% --> %.2f %%\n",
                      trials[i].trialId, trials[i].freq_Hz, q_act, old_err, new_err);
    }
    double new_avg_err = (trialCount > 0) ? (new_sum_err / trialCount) : 0.0;
    Serial.println("--------------------------------------------------------------------------------");
    Serial.printf("  Max Error     : %.2f %%  -->  %.2f %%\n", old_max_err, new_max_err);
    Serial.printf("  Average Error : %.2f %%  -->  %.2f %%\n", (trialCount > 0 ? old_sum_err / trialCount : 0.0), new_avg_err);
    double old_avg_err = (trialCount > 0) ? (old_sum_err / trialCount) : 0.0;
    if (new_avg_err < old_avg_err || new_max_err < old_max_err) {
        Serial.println("  >>> Calibration error improved. <<<");
    } else {
        Serial.println("  >>> NOTE: error did not improve (least squares minimises absolute, not % error). <<<");
    }
    Serial.println("================================================================================\n");

    if (calHoldingResult && Q_actual_Lmin > 0.0) {
        double q_run = (CAL_A * run_freq_Hz) + CAL_B;
        error_pct = fabs(q_run - Q_actual_Lmin) / Q_actual_Lmin * 100.0;
    }
}

void loadDemoTrials() {
    struct DemoPt { uint64_t n; double t; double v; };
    DemoPt pts[5] = {
        {450, 60.0, 1.00},
        {912, 60.0, 2.00},
        {1380, 60.0, 3.00},
        {2070, 60.0, 4.50},
        {2772, 60.0, 6.00}
    };
    trialCount = 0;
    for (int i = 0; i < 5; i++) {
        trials[i].trialId = i + 1;
        trials[i].pulses = pts[i].n;
        trials[i].time_s = pts[i].t;
        trials[i].freq_Hz = (double)pts[i].n / pts[i].t;
        trials[i].vol_L = pts[i].v;
        trials[i].Q_actual = (pts[i].v / pts[i].t) * 60.0;
        double q_cur = (CAL_A * trials[i].freq_Hz) + CAL_B;
        trials[i].error_pct = fabs(q_cur - trials[i].Q_actual) / trials[i].Q_actual * 100.0;
    }
    trialCount = 5;
    Serial.println("\n[CAL] Loaded 5 experimental challenge calibration trials!");
    calculateFitAndPrint(false);
}

void resetCalibrationTrials() {
    trialCount = 0;
    fit_valid = false;
    calRunActive = false;
    calRunEnded = false;
    calHoldingResult = false;
    run_pulses = 0;
    run_time_s = 0.0;
    run_freq_Hz = 0.0;
    Q_actual_Lmin = 0.0;
    error_pct = 0.0;
    Serial.println("[CAL] Reset calibration trials and run fields to zero.");
}

// ======================= FLOW COMPUTATION & SERIAL STUDIO DAQ =======================
void updateFlowAndTelemetry() {
    int64_t t_now_us = esp_timer_get_time();
    if (tPrevUs == 0) {
        tPrevUs = t_now_us;
        return;
    }

    double elapsed_s = (double)(t_now_us - tPrevUs) / 1e6;
    if (elapsed_s < windowNominalSec) {
        return;
    }

    // Measured real window time (not nominal 1 s)
    window_s = elapsed_s;
    tPrevUs = t_now_us;

    // Time since boot in seconds: time_s = esp_timer_get_time() / 1e6
    time_s = (double)t_now_us / 1e6;

    // Pulses (N) = cnt_now - cnt_prev (PCNT delta in the window)
    int64_t cnt_now = getPcntCount();
    int64_t N = cnt_now - cnt_prev;
    if (N < 0) N = 0;
    cnt_prev = cnt_now;
    lastWindowN = N;

    // Pulse frequency: freq_Hz = N / window_s
    freq_Hz = (window_s > 0.0) ? ((double)N / window_s) : 0.0;

    // Flow rate: Q_Lmin = max(CAL_A * freq_Hz + CAL_B, 0)
    // Noise gate: below FLOW_CUTOFF_HZ report zero so a non-zero offset b cannot create phantom flow/volume
    Q_Lmin = (freq_Hz < FLOW_CUTOFF_HZ) ? 0.0 : fmax((CAL_A * freq_Hz) + CAL_B, 0.0);

    // Flow rate: Q_mLs = Q_Lmin * 1000.0 / 60.0
    Q_mLs = Q_Lmin * 1000.0 / 60.0;

    // Volume totalizer: total_L += (Q_Lmin / 60.0) * window_s
    total_L += (Q_Lmin / 60.0) * window_s;

    // Pulse totalizer: total_pulses += N (64-bit)
    total_pulses += (uint64_t)N;

    // Sync legacy variables for existing FSM interlocks
    flowFreqHz = (float)freq_Hz;
    flowLpm = (float)Q_Lmin;
    flowMls = (float)Q_mLs;
    totalL = (float)total_L;
    pulseCount = (uint32_t)cnt_now;
    if (state == ST_CASE2_FLUSHING) {
        totalFlushL += (float)((Q_Lmin / 60.0) * window_s);
    }
    newFlowSample = true;

    // Calibration run fields
    uint64_t cur_run_pulses = 0;
    double cur_run_time_s = 0.0;
    double cur_run_freq_Hz = 0.0;
    double cur_Q_actual_Lmin = 0.0;
    double cur_error_pct = 0.0;

    if (calRunActive) {
        cur_run_pulses = ((uint64_t)cnt_now >= calPulsesAtStart) ? ((uint64_t)cnt_now - calPulsesAtStart) : 0;
        cur_run_time_s = (double)(t_now_us - calStartUs) / 1e6;
        cur_run_freq_Hz = (cur_run_time_s > 0.0) ? ((double)cur_run_pulses / cur_run_time_s) : 0.0;
        cur_Q_actual_Lmin = 0.0;
        cur_error_pct = 0.0;
    } else if (calHoldingResult) {
        cur_run_pulses = run_pulses;
        cur_run_time_s = run_time_s;
        cur_run_freq_Hz = run_freq_Hz;
        cur_Q_actual_Lmin = Q_actual_Lmin;
        cur_error_pct = error_pct;
    }

    // SERIAL STUDIO DAQ FRAME FORMAT PER SPECIFICATION:
    // Format: /*time_s,pulses,window_s,freq_Hz,Q_Lmin,Q_mLs,total_L,total_pulses,run_pulses,run_time_s,run_freq_Hz,Q_actual_Lmin,error_pct*/
    Serial.printf("/*%.3f,%lld,%.4f,%.2f,%.3f,%.2f,%.4f,%llu,%llu,%.3f,%.2f,%.3f,%.2f*/\r\n",
                  time_s,
                  (long long)N,
                  window_s,
                  freq_Hz,
                  Q_Lmin,
                  Q_mLs,
                  total_L,
                  (unsigned long long)total_pulses,
                  (unsigned long long)cur_run_pulses,
                  cur_run_time_s,
                  cur_run_freq_Hz,
                  cur_Q_actual_Lmin,
                  cur_error_pct);
}

// ======================= CORE PROCESS CONTROL LOOPS =======================
void executeProcessControl() {
    uint32_t now = millis();

    switch (state) {
        case ST_IDLE:
            break;

        case ST_CASE1_NORMAL:
            // Sump Level Interlock: Pump 1 ONLY stops when the Sump gets FULL (Float Switch Trips HIGH)
            if (floatHigh) {
                enterCase1Surge();
                return;
            }

            // Single-Go Auto-Advance: If surge mitigation was demonstrated and stabilized for 20s, transition directly to Case 2
            if (singleGoMode && surgeDemonstrated && (now - surgeClearMs >= C1_RECOVERY_DWELL_MS)) {
                Serial.println("\n[SINGLE-GO] Sump surge mitigation demonstrated and stable recovery confirmed!");
                Serial.println("[SINGLE-GO] Transitioning directly to Case 2 flush sequence in a single go...");
                startCase2Flushing();
                return;
            }
            break;

        case ST_CASE1_SURGE:
            // Sump Level Mitigation: If sump level has subsided below float switch -> restore normal
            if (floatHigh) {
                surgeFloatClearMs = 0;
            } else {
                if (surgeFloatClearMs == 0) surgeFloatClearMs = now;
                if (now - surgeFloatClearMs >= SURGE_CLEAR_CONFIRM_MS) {
                    surgeDemonstrated = true;
                    surgeClearMs = now;
                    Serial.println("[INTERLOCK] Sump level normal (confirmed). Auto-restarting Pump-01 @ 50% duty.");
                    resumeCase1AfterSurge();
                    return;
                }
            }
            break;

        case ST_CASE1_DRAIN: {
            // Sump is empty when Pump-02 runs dry: FT-01 flow collapses and stays collapsed.
            static uint32_t lastDrainPrintMs = 0;
            if (now - lastDrainPrintMs >= 2000) {
                lastDrainPrintMs = now;
                Serial.printf("[DRAIN] Evacuating sump before Case 2... Flow: %.2f L/min | Elapsed: %lu s / %lu s\n",
                              flowLpm, (unsigned long)((now - stateStartMs) / 1000), (unsigned long)(C1_DRAIN_MAX_MS / 1000));
            }
            if (newFlowSample) {
                if (flowLpm < FLOW_MIN_VALID) {
                    if (!lowFlowSinceMs) lowFlowSinceMs = now;
                    if (now - lowFlowSinceMs >= C1_DRAIN_EMPTY_MS) {
                        Serial.println("[DRAIN] Sump empty (FT-01 flow collapsed). Advancing to Case 2 flush.");
                        startCase2Flushing();
                        return;
                    }
                } else {
                    lowFlowSinceMs = 0;
                }
            }
            if ((now - stateStartMs) >= C1_DRAIN_MAX_MS) {
                Serial.println("[DRAIN] Sump evacuation window reached. Advancing to Case 2 flush.");
                startCase2Flushing();
                return;
            }
            break;
        }

        case ST_CASE2_FLUSHING: {
            uint32_t elapsed = now - stateStartMs;

            // Global safety timeout
            if (elapsed > C2_MAX_RUN_MS) {
                enterFault("Case 2 Flush safety timeout exceeded (10 min)");
                return;
            }

            // Timed Mode (e.g. 'case2 45' for bench testing)
            if (c2CustomDurationMs > 0) {
                if (elapsed >= c2CustomDurationMs) {
                    pumpsOff();
                    stopStartMs = now;
                    state = ST_CASE2_STOPPING;
                    Serial.println("[C2] Timed flush duration complete. All pumps OFF. Closing SOV in 500ms...");
                }
                break;
            }

            // Automatic Flush Batch Completion: Set to 90 s (1 min 30 s) for 4.2 L flush tank to empty
            if (c2CustomDurationMs == 0 && elapsed >= C2_AUTO_MAX_RUN_MS) {
                pumpsOff();
                stopStartMs = now;
                state = ST_CASE2_STOPPING;
                Serial.printf("\n[C2] 4.2L FLUSH COMPLETED (%lu s elapsed, Total: %.3f L). All pumps OFF.\n",
                              (unsigned long)(elapsed / 1000), totalFlushL);
                break;
            }

            // Periodic live telemetry to console every 2 seconds for clear real-time monitoring
            if (now - lastC2PrintMs >= 2000) {
                lastC2PrintMs = now;
                Serial.printf("[C2 LIVE] Time: %lu s | Flow: %.2f L/min | Flushed: %.3f L / 4.15 L\n",
                              (unsigned long)(elapsed / 1000), flowLpm, totalFlushL);
            }

            // Autonomous Problem Statement Mode:
            // 1. Line Priming Window: Confirm fluid reaches FT-01
            if (!flushFlowConfirmed) {
                // Sump was drained before Case 2, so FT-01 flow starts at ~0: flow rising now genuinely means flush water has arrived.
                if (flowLpm >= FLOW_MIN_VALID) {
                    flushFlowConfirmed = true;
                    Serial.printf("[C2] Flush fluid detected at FT-01 (Flow: %.2f L/min). Line actively primed!\n", flowLpm);
                } else if (elapsed > C2_PRIME_MS) {
                    // Flush line priming failed after 25s
                    enterFault("Case 2 Priming Timeout: Zero flow after 25s. Ensure BV-02 valve is OPEN!");
                    return;
                }
            }

            // 2. Depletion Detection (PDF Requirement: Stop as soon as 4.2L flush finishes or air is sucked)
            // Guard: Arm depletion detection only after line priming is confirmed, 10s startup grace has passed,
            // and at least 300 mL has been delivered (prevents startup ramp-up transients from triggering air collapse).
            bool depletionArmed = flushFlowConfirmed && (elapsed >= C2_STARTUP_GRACE_MS) && (totalFlushL >= C2_MIN_FLUSH_ARM_L);

            if (depletionArmed && newFlowSample) {
                bool volumeExhausted = (totalFlushL >= FLUSH_CEILING_L);
                bool airSucked = (flowLpm < C2_FLOW_COLLAPSE_LPM);

                if (volumeExhausted) {
                    pumpsOff();
                    stopStartMs = now;
                    state = ST_CASE2_STOPPING;
                    Serial.printf("\n[C2] PDF 4.2L FLUSH FINISHED BY VOLUME: %.3f L delivered! All pumps OFF.\n", totalFlushL);
                } else if (airSucked) {
                    c2LowFlowSamples++;
                    if (!lowFlowSinceMs) lowFlowSinceMs = now;
                    if ((now - lowFlowSinceMs >= C2_EXHAUST_MS) || (c2LowFlowSamples >= 2)) {
                        pumpsOff();
                        stopStartMs = now;
                        state = ST_CASE2_STOPPING;
                        Serial.printf("\n[C2] 4.2L FLUSH TANK EMPTY: Air suction detected at inlet (Flow: %.2f L/min, Total: %.3f L). All pumps OFF.\n",
                                      flowLpm, totalFlushL);
                    }
                } else {
                    if (c2LowFlowSamples > 0) c2LowFlowSamples--;
                    lowFlowSinceMs = 0; // Healthy flow
                }
            }
            break;
        }

        case ST_CASE2_STOPPING:
            // Staggered shutdown: Close solenoid valve 500ms after pumps to eliminate pressure shocks
            if (now - stopStartMs >= SOV_CLOSE_DELAY_MS) {
                sovClose();
                enterIdle("Case 2 Flush sequence complete. System safely OFF.");
                Serial.printf("[COMPLETE] Challenge Run Finished! Total Volume: %.3f L (Flush: %.3f L)\n",
                              totalL, totalFlushL);
            }
            break;

        case ST_CALMODE: {
            // Guard 1: sump float HIGH -> cut Pump-1 (no overflow while filling by hand)
            if (floatHigh && pumpDutyPct[0] > 0.0f) {
                setPump(0, 0.0f);
                Serial.println("[CALMODE] Sump float HIGH -> Pump-1 STOPPED.");
            }
            // Guard 2: Pump-2 running with no flow (sump dry, hose pulled or line blocked) -> Pump-2 off
            if (pumpDutyPct[1] > 0.0f) {
                if (newFlowSample) {
                    if (flowLpm < CALMODE_NOFLOW_LPM) {
                        if (!calModeNoFlowSinceMs) calModeNoFlowSinceMs = now;
                        if (now - calModeNoFlowSinceMs >= CALMODE_NOFLOW_STOP_MS) {
                            setPump(1, 0.0f);
                            calModeNoFlowSinceMs = 0;
                            Serial.println("[CALMODE] Pump-2 no-flow guard: no flow for 5 s -> Pump-2 STOPPED (sump dry or line blocked?).");
                        }
                    } else {
                        calModeNoFlowSinceMs = 0;
                    }
                }
            } else {
                calModeNoFlowSinceMs = 0;
            }
            // Guard 3: nobody has typed a command for a while -> stop everything and leave calibration mode
            if (now - calModeLastCmdMs >= CALMODE_IDLE_TIMEOUT_MS) {
                exitCalMode("no command for 120 s");
                return;
            }
            break;
        }

        case ST_FAULT:
            break;
    }
}

// ======================= SERIAL CLI COMMAND INTERPRETER =======================
void handleSerialCLI() {
    static String rx;
    static uint32_t lastCharMs = 0;
    bool execute = false;

    while (Serial.available()) {
        char c = (char)Serial.read();
        lastCharMs = millis();
        if (c == '\n' || c == '\r') {
            execute = true;
            break;
        } else {
            rx += c;
        }
    }

    // Inter-character timeout: Execute even if no newline was sent (e.g. Serial Studio 'No Line Ending')
    if (!execute && rx.length() > 0 && (millis() - lastCharMs > 80)) {
        execute = true;
    }

    if (!execute) return;

    rx.trim();
    if (rx.length() == 0) return;

    String cmd = rx;
    cmd.toLowerCase();
    rx = "";
    calModeLastCmdMs = millis();

    Serial.printf("[CMD] Received: '%s'\n", cmd.c_str());

    // Master Emergency Stop command is ALWAYS permitted at any millisecond
    if (cmd == "stop" || cmd == "x" || cmd == "0" || cmd == "halt" || cmd == "kill") {
        masterInterlockArmed = false;
        stopAll("CLI Emergency Stop Command");
        return;
    }

    // ======================= CALIBRATION MODE ('calmode on' / 'calmode off') =======================
    if (cmd == "calmode on" || cmd == "calmode start") {
        if (state == ST_CALMODE) {
            Serial.println("[CALMODE] Already in calibration mode. Type 'calmode off' to leave.");
        } else if (state != ST_IDLE) {
            Serial.println("[CALMODE] Refused: process is not idle. Type 'stop' first, then 'calmode on'.");
        } else {
            enterCalMode();
        }
        return;
    }
    if (cmd == "calmode off" || cmd == "calmode exit") {
        if (state == ST_CALMODE) {
            exitCalMode("operator command");
        } else {
            Serial.println("[CALMODE] Not in calibration mode.");
        }
        return;
    }
    if (cmd.startsWith("calmode")) {
        Serial.println("[CALMODE] Usage: 'calmode on' or 'calmode off'.");
        return;
    }
    // Block process starts while an engineer is calibrating
    if (state == ST_CALMODE && (cmd == "case1" || cmd == "s1" || cmd == "c1" || cmd == "1" ||
                                cmd == "case2" || cmd == "s2" || cmd == "c2" || cmd == "2" ||
                                cmd.startsWith("case2 ") || cmd.startsWith("c2 ") ||
                                cmd == "auto" || cmd == "singlego" || cmd == "a")) {
        Serial.println("[CALMODE] Process start blocked in calibration mode. Type 'calmode off' first.");
        return;
    }

    // ======================= CALIBRATION CLI COMMANDS ('S', 'E', 'V') =======================
    // 'S' - Start calibration run
    if (cmd == "s" || cmd == "start" || cmd == "cal s" || cmd == "cal start") {
        startCalibrationRun();
        return;
    }

    // 'E' - End calibration run
    if (cmd == "e" || cmd == "end" || cmd == "cal e" || cmd == "cal end") {
        endCalibrationRun();
        return;
    }

    // 'V <litres>' - Enter collected volume
    if (cmd.startsWith("v ") || cmd.startsWith("vol ")) {
        int sp = cmd.indexOf(' ');
        double v = cmd.substring(sp + 1).toDouble();
        recordCollectedVolume(v);
        return;
    }

    // If waiting for volume after 'E', permit directly typing the number (e.g. "1.00")
    if (calHoldingResult && (V_collected == 0.0)) {
        double v = cmd.toDouble();
        if (v > 0.0) {
            recordCollectedVolume(v);
            return;
        }
    }

    // 'FIT' / 'CAL' / 'REPORT' - Print on-demand linear regression and error analysis
    if (cmd == "fit" || cmd == "cal" || cmd == "report") {
        calculateFitAndPrint(false);
        return;
    }

    // 'APPLY' / 'UPDATE' - Apply fitted slope 'a' & offset 'b' to CAL_A & CAL_B and verify error drop
    if (cmd == "apply" || cmd == "update" || cmd == "cal apply") {
        applyCalibrationFit();
        return;
    }

    // Manual calibration coefficient overrides
    if (cmd.startsWith("cal_a ")) {
        CAL_A = cmd.substring(6).toDouble();
        Serial.printf("[SET] Active CAL_A set to %.6f\n", CAL_A);
        return;
    }
    if (cmd.startsWith("cal_b ")) {
        CAL_B = cmd.substring(6).toDouble();
        Serial.printf("[SET] Active CAL_B set to %.6f\n", CAL_B);
        return;
    }

    // Nominal window setting in seconds (default 1.0 s)
    if (cmd.startsWith("window ") || cmd.startsWith("w ")) {
        int sp = cmd.indexOf(' ');
        double w = cmd.substring(sp + 1).toDouble();
        if (w >= 0.1 && w <= 60.0) {
            windowNominalSec = w;
            Serial.printf("[SET] Telemetry window duration set to %.2f seconds\n", windowNominalSec);
        } else {
            Serial.println("[SET] Invalid window. Allowed range: 0.1 - 60.0 seconds.");
        }
        return;
    }

    // Demo calibration dataset for instant mathematical verification
    if (cmd == "demo" || cmd == "test_fit") {
        loadDemoTrials();
        return;
    }

    // Reset calibration trials and run fields
    if (cmd == "c" || cmd == "clear" || cmd == "cal_reset") {
        resetCalibrationTrials();
        return;
    }

    // Remote Interlock Arm / Disarm controls
    if (cmd == "arm" || cmd == "unlock") {
        masterInterlockArmed = true;
        Serial.println("[SAFETY INTERLOCK] System REMOTELY ARMED. Process commands permitted.");
        return;
    } else if (cmd == "disarm" || cmd == "lock") {
        masterInterlockArmed = false;
        stopAll("Remote Safety Disarm Command");
        Serial.println("[SAFETY INTERLOCK] System DISARMED and LOCKED in SAFE STOP.");
        return;
    }

    // Direct Actuator Controls (Always allowed for bench diagnostics)
    if (cmd == "sov inv" || cmd == "sov pol") {
        SOV_ACTIVE_HIGH = !SOV_ACTIVE_HIGH;
        Serial.printf("[ACTUATOR] Solenoid polarity toggled: Active %s (relay re-set to OFF/closed)\n",
                      SOV_ACTIVE_HIGH ? "HIGH (relay ON when line HIGH)" : "LOW (relay ON when line pulled LOW; typical 5V module)");
        sovClose();
        return;
    } else if (cmd.startsWith("sov ")) {
        int v = cmd.substring(4).toInt();
        if (v) sovOpen(); else sovClose();
        return;
    }

    // Process Start Commands: Auto-arm on explicit user demand
    if (cmd == "case1" || cmd == "s1" || cmd == "c1" || cmd == "1") {
        masterInterlockArmed = true;
        startCase1Normal();
        return;
    } else if (cmd == "case2" || cmd == "s2" || cmd == "c2" || cmd == "2") {
        masterInterlockArmed = true;
        startCase2Flushing();
        return;
    } else if (cmd.startsWith("case2 ") || cmd.startsWith("c2 ")) {
        masterInterlockArmed = true;
        int sp = cmd.indexOf(' ');
        uint32_t s = cmd.substring(sp + 1).toInt();
        startCase2Flushing(s);
        return;
    } else if (cmd == "auto" || cmd == "singlego" || cmd == "a") {
        masterInterlockArmed = true;
        Serial.println("[CMD] Starting Single-Go Automated Challenge Run.");
        singleGoMode = true;
        startCase1Normal();
        return;
    }

    // Manual Pump Controls (Auto-arms for bench testing convenience)
    bool isPumpCmd = (cmd.startsWith("p1 ") || cmd.startsWith("p2 ") || cmd.startsWith("p3 "));
    if (isPumpCmd) {
        masterInterlockArmed = true;
    }

    if (cmd == "prev" || cmd == "p1 rev" || cmd == "p rev") {
        PUMP_REVERSE = !PUMP_REVERSE;
        uint8_t activePwm = PUMP_REVERSE ? PUMP[0].rPwm : PUMP[0].lPwm;
        Serial.printf("[MANUAL] Pump direction toggled: %s (P1 PWM on GPIO %d)\n",
                      PUMP_REVERSE ? "REVERSE" : "FORWARD", activePwm);
        return;
    } else if (cmd.startsWith("p1 ")) {
        float d = cmd.substring(3).toFloat();
        setPump(0, d);
        uint8_t activePwm = PUMP_REVERSE ? PUMP[0].rPwm : PUMP[0].lPwm;
        Serial.printf("[MANUAL] Pump 1 duty set to %.1f%% (L_EN=GPIO13&12, R_EN=GPIO%d, PWM on GPIO%d)\n",
                      d, PUMP[0].rEn, activePwm);
    } else if (cmd.startsWith("p2 ")) {
        float d = cmd.substring(3).toFloat();
        setPump(1, d);
        Serial.printf("[MANUAL] Pump 2 duty set to %.1f%% (GPIO18 PWM, L_EN=16, R_EN=17)\n", d);
    } else if (cmd.startsWith("p3 ")) {
        float d = cmd.substring(3).toFloat();
        setPump(2, d);
        Serial.printf("[MANUAL] Pump 3 duty set to %.1f%% (GPIO23 PWM, L_EN=21, R_EN=22)\n", d);
    } else if (cmd.startsWith("ppl ")) {
        float v = cmd.substring(4).toFloat();
        if (v > 10.0f) {
            ppl = v;
            Serial.printf("[SET] Calibration Pulses/L: %.1f\n", ppl);
        }
    } else if (cmd == "rt") {
        total_L = 0.0;
        totalL = 0.0f;
        totalFlushL = 0.0f;
        Serial.println("[SET] Totalizers reset to 0.000 L");
    } else if (cmd == "status") {
        const char* names[] = {"IDLE", "CASE1_NORMAL", "CASE1_SURGE", "CASE2_FLUSHING", "CASE2_STOPPING", "FAULT", "CASE1_DRAIN", "CALMODE"};
        Serial.printf("STATUS | State: %s | Interlock: %s | Sump: %s | Flow: %.3f L/min (%.2f Hz, %.2f mL/s) | Total: %.4f L\n",
                      names[state], masterInterlockArmed ? "ARMED (ACTIVE)" : "LOCKED (SAFE STOP)",
                      floatHigh ? "HIGH (ALARM)" : "NORMAL", Q_Lmin, freq_Hz, Q_mLs, total_L);
        Serial.printf("       | Pulses: Total=%llu, Window N=%lld, Run Pulses=%llu | CAL_A=%.6f, CAL_B=%.6f\n",
                      (unsigned long long)total_pulses, (long long)lastWindowN, (unsigned long long)run_pulses, CAL_A, CAL_B);
    } else if (cmd == "?") {
        Serial.println("==================== TEAM DISTRO COMMAND MENU ====================");
        Serial.println("  --- CALIBRATION PROTOCOL (Task 2) ---");
        Serial.println("  S                - Start calibration run (records pulses & time)");
        Serial.println("  E                - End calibration run (freezes run fields)");
        Serial.println("  V <litres>       - Record collected volume (e.g. 'V 1.00')");
        Serial.println("  FIT / CAL        - Display on-demand linear regression & error report");
        Serial.println("  APPLY            - Apply fitted a & b to CAL_A & CAL_B (verifies error drop)");
        Serial.println("  cal_a <val>      - Set slope CAL_A manually");
        Serial.println("  cal_b <val>      - Set offset CAL_B manually");
        Serial.println("  window <sec>     - Set nominal telemetry window (default 1.0 s)");
        Serial.println("  DEMO             - Load 5-point challenge dataset for verification");
        Serial.println("  calmode on       - Enter guarded calibration mode (button start disabled)");
        Serial.println("  calmode off      - Leave calibration mode (pumps off, run cancelled, back to idle)");
        Serial.println("  C / CLEAR        - Clear calibration trials and reset run fields to 0");
        Serial.println("  --- PROCESS CONTROL ---");
        Serial.println("  1 / case1 / s1   - Start Case 1 Closed-Loop Recirculation (P1=50%, P2=100%)");
        Serial.println("  2 / case2 / s2   - Initiate Case 2 Flush (Autonomous until 4.2L empty)");
        Serial.println("  c2 <seconds>     - Timed Case 2 Flush (e.g. 'c2 30' for bench test)");
        Serial.println("  a / auto         - Run Single-Go Automated Sequence (Case 1 -> Case 2)");
        Serial.println("  0 / stop / x     - Safe Emergency Stop (All actuators OFF)");
        Serial.println("  arm / unlock     - Remote Arm Master Safety Interlock");
        Serial.println("  disarm / lock    - Remote Disarm Master Safety Interlock (Safe Stop)");
        Serial.println("  p1 <duty>        - Manual Pump 1 test (e.g. p1 50)");
        Serial.println("  p2 <duty>        - Manual Pump 2 test (e.g. p2 100)");
        Serial.println("  p3 <duty>        - Manual Pump 3 test (e.g. p3 80)");
        Serial.println("  sov <1/0>        - Manual Solenoid Valve toggle (1=Open, 0=Close)");
        Serial.println("  rt               - Reset totalizer to 0.000 L");
        Serial.println("  status           - Snapshot print of current hardware state & pin pulses");
        Serial.println("  ?                - Show this command reference");
        Serial.println("==================================================================");
    }
}

// ======================= ARDUINO / ESP-IDF SETUP & LOOP =======================
void setup() {
    Serial.begin(115200);

    // Keep ESP_LOG output off the frame channel so Serial Studio doesn't parse log lines as frames
    esp_log_level_set("*", ESP_LOG_NONE);

    // 1. Initialise all actuators to safe OFF state first (relay released)
    sovDrive(false);

    // Initialise Pump 1 (U2) Enable lines (energize both 13 and 12 to guarantee enable)
    pinMode(12, OUTPUT);
    digitalWrite(12, LOW);
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);
    pinMode(PUMP[0].rEn, OUTPUT); // GPIO 26
    digitalWrite(PUMP[0].rEn, LOW);
    PWM_ATTACH(PUMP[0].lPwm, 0);  // GPIO 32
    PWM_ATTACH(PUMP[0].rPwm, 1);  // GPIO 33

    // Initialise Pump 2 (U3)
    pinMode(PUMP[1].lEn, OUTPUT); // GPIO 16
    digitalWrite(PUMP[1].lEn, LOW);
    pinMode(PUMP[1].rEn, OUTPUT); // GPIO 17
    digitalWrite(PUMP[1].rEn, LOW);
    PWM_ATTACH(PUMP[1].lPwm, 2);  // GPIO 18
    pinMode(PUMP[1].rPwm, OUTPUT); // GPIO 19 - unused reverse PWM, hold LOW
    digitalWrite(PUMP[1].rPwm, LOW);

    // Initialise Pump 3 (U4)
    pinMode(PUMP[2].lEn, OUTPUT); // GPIO 21
    digitalWrite(PUMP[2].lEn, LOW);
    pinMode(PUMP[2].rEn, OUTPUT); // GPIO 22
    digitalWrite(PUMP[2].rEn, LOW);
    PWM_ATTACH(PUMP[2].lPwm, 4);  // GPIO 23
    pinMode(PUMP[2].rPwm, OUTPUT); // GPIO 25 - unused reverse PWM, hold LOW
    digitalWrite(PUMP[2].rPwm, LOW);

    pumpsOff();

    // 2. Initialise physical digital inputs matching schematic & hardware bench
    pinMode(PIN_FS, INPUT_PULLUP);        // GPIO 14 (Flow Sensor FT-01 Primary on Pin 12 labeled FS)
    pinMode(PIN_FSW, INPUT);              // GPIO 35 (Sump Float Switch on Pin 6 labeled FSW, 10k R10 pullup)
    pinMode(PIN_SW, INPUT);               // GPIO 34 (ON/OFF Switch on Pin 5 labeled SW, 10k R2 pullup)

    // Hardware Pulse Counter (PCNT) configuration on GPIO 14
    initPCNT();

    // Attach interrupts on physical pins (FALLING mode gives 1 pulse per cycle matching PCNT)
    attachInterrupt(digitalPinToInterrupt(PIN_FS), onFlowPulseISR14, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_FSW), onFlowPulseISR35, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_SW), onFlowPulseISR34, FALLING);

    tPrevUs = esp_timer_get_time();
    Serial.println("\n============================================================");
    Serial.println("  LAM RESEARCH CHALLENGE 3.0 - STAGE 2 VERIFIED FIRMWARE    ");
    Serial.println("  TEAM: DISTRO | PCB: LAM STAGE 2 PCB REV 1.0 (T-Works)     ");
    Serial.println("  FLOW TRANSMITTER FT-01 DAQ & CALIBRATION ENGINE READY      ");
    Serial.println("============================================================");
    Serial.printf("[INIT] CAL_A = %.6f, CAL_B = %.6f (Empirically Calibrated) | Window = %.2fs\n", CAL_A, CAL_B, windowNominalSec);
    Serial.println("[INIT] Frame Format: /*time_s,pulses,window_s,freq_Hz,Q_Lmin,Q_mLs,total_L,total_pulses,run_pulses,run_time_s,run_freq_Hz,Q_actual_Lmin,error_pct*/");

    if (AUTO_START_ON_BOOT) {
        Serial.println("[AUTO-START] Physical switch bypassed. Starting normal rig operation in 1.5s...");
        delay(1500); // Safe delay for power rails to stabilize
        masterInterlockArmed = true;
        startCase1Normal();
    } else {
        Serial.println("[STANDBY] Master Safety Interlock: LOCKED (OFF).");
        Serial.println("[STANDBY] >> Press the physical Pushbutton on the rig to START! <<");
        Serial.println("[STANDBY] (Or send 'arm' over Serial to permit remote commands).");
    }
}

void loop() {
    handleSerialCLI();
    if (USE_PHYSICAL_SWITCH) {
        handleTactileButton();
    }
    handleFloatSwitch();
    updateFlowAndTelemetry();
    executeProcessControl();

    newFlowSample = false;

    delay(2); // Yield CPU to FreeRTOS IDLE task (prevents task_wdt triggers)
}
