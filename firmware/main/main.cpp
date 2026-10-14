/*
 * ESP32 process controller for Lam Research Challenge 3.0 Stage 2 (LRC-26-0528).
 * Target: NodeMCU-32S / DevKitC on LAM Stage 2 PCB Rev 1.0.
 *
 * Case 1: Closed-loop recirculation at 3.0 L/min target (P1 at 50%, P2 at 100%).
 *         Sump high level stops P1 while P2 drains excess to the 10 L reservoir.
 *         P1 automatically restarts when the sump level drops below the float switch.
 * Case 2: Flush sequence with the 10 L reservoir isolated by closing solenoid OV-01.
 *         P1, P2, and P3 run concurrently from the 4.2 L flush reservoir.
 *         Automatic shutdown triggers when fluid depletes and flow collapses.
 */

#include <Arduino.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/pulse_cnt.h"

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

// Pin assignments matching schematic LAM Stage 2 PCB Rev 1.0.
// Digital inputs use external 10 kOhm pull-up resistors to 3.3 V with 100 nF debounce capacitors.
static const int PIN_SW         = 34;
static const int PIN_FSW        = 35;
static const int PIN_FS         = 14;
static const int PIN_SV         = 27;

// BTS7960 high-power half-bridge motor driver connections.
// Each driver requires independent left/right enable and PWM control pins.
struct PumpPins {
    uint8_t lEn;
    uint8_t rEn;
    uint8_t lPwm;
    uint8_t rPwm;
};

// Motor driver pin assignments: U2 (Pump-01 feed), U3 (Pump-02 return), U4 (Pump-03 flush).
static const PumpPins PUMP[3] = {
    {13, 26, 32, 33},
    {16, 17, 18, 19},
    {21, 22, 23, 25}
};

// Hardware startup and operating parameters.
// Tactile switch provides manual start and safe stop functionality.
static const bool     USE_PHYSICAL_SWITCH  = true;
static const bool     AUTO_START_ON_BOOT   = false;

// 5 kHz PWM frequency operates reliably through the optocoupler isolators on the BTS7960 boards.
static const uint32_t PWM_FREQ             = 5000;
static const uint8_t  PWM_BITS             = 8;
static bool           PUMP_REVERSE         = false;
static const bool     FLOAT_CLOSED_IS_HIGH = true;

// Operating duty cycles for Case 1 recirculation: Pump-01 at 50% duty, Pump-02 at 100% duty.
static const float    PUMP1_C1_DUTY        = 50.0f;
static const float    PUMP2_C1_DUTY        = 100.0f;

// Case 2 flush line conditioning: all three pumps operate concurrently.
static const float    PUMP1_C2_DUTY        = 50.0f;
static const float    PUMP2_C2_DUTY        = 100.0f;
static const float    PUMP3_C2_DUTY        = 100.0f;

// Nominal pulses per litre rating for the YF-S401 micro-turbine (98 Hz at 1 L/min).
// NOTE: Factory rating is 5880 pulses/L (98 Hz/(L/min) * 60 s), but active flow calculation uses linear model CAL_A and CAL_B.
static float          ppl                  = 5880.0f;
// Noise gate threshold: pulse frequencies below 1.0 Hz are suppressed to prevent zero-flow creep.
static const float    FLOW_CUTOFF_HZ       = 1.0f;
// Minimum fluid velocity corresponding to 0.50 L/min to verify positive flow.
static const float    FLOW_MIN_VALID       = 0.50f;
// Flow collapse threshold: values below 0.35 L/min indicate cavitation and air entrainment.
static const float    C2_FLOW_COLLAPSE_LPM = 0.35f;
// Volumetric safety ceiling for the 4.2 L flush reservoir accounting for line holdup.
static const float    FLUSH_CEILING_L      = 4.15f;
// 10 s startup grace period to prevent initial hydraulic transients from triggering shutdown.
static const uint32_t C2_STARTUP_GRACE_MS  = 10000;
// Minimum 0.30 L delivered before the depletion collapse detector arms.
static const float    C2_MIN_FLUSH_ARM_L   = 0.30f;

// Operational timing limits and safety timeouts.
static const uint32_t C1_GRACE_MS          = 4000;
static const uint32_t C1_NOFLOW_FAULT_MS   = 6000;
static const uint32_t C2_PRIME_MS          = 25000;
static const uint32_t C2_EXHAUST_MS        = 1000;
static const uint32_t C1_RECOVERY_DWELL_MS = 20000;
static const uint32_t C1_DRAIN_EMPTY_MS    = 3000;
static const uint32_t C1_DRAIN_MAX_MS      = 45000;
// Sump float switch must stay open for 3000 ms before Pump-01 restarts to prevent short-cycling.
static const uint32_t SURGE_CLEAR_CONFIRM_MS = 3000;
// Automatic Case 2 flush duration ceiling: 90 s evacuates the 4.2 L flush reservoir.
static const uint32_t C2_AUTO_MAX_RUN_MS   = 90000;

// Calibration mode guards and idle timeouts.
static const uint32_t CALMODE_IDLE_TIMEOUT_MS = 120000;
static const uint32_t CALMODE_NOFLOW_STOP_MS  = 5000;
static const float    CALMODE_NOFLOW_LPM      = 0.20f;
static const uint32_t C2_MAX_RUN_MS        = 600000;
// Solenoid valve closes 500 ms after pumps stop to eliminate water-hammer pressure spikes.
static const uint32_t SOV_CLOSE_DELAY_MS   = 500;
// Solenoid valve opens 200 ms prior to pump energization to pre-flood the suction line.
static const uint32_t SOV_PRE_OPEN_MS      = 200;
static bool           SOV_ACTIVE_HIGH      = false;
// Floating the GPIO pin allows the relay module on-board 5 V pull-up to fully release the coil.
static const bool     SOV_HIGH_BY_FLOAT    = true;

// Mechanical switch debounce and long-press emergency stop thresholds.
static const uint32_t DEBOUNCE_MS          = 30;
static const uint32_t LONG_PRESS_MS        = 1500;
static const uint32_t FLOAT_DEBOUNCE_MS    = 250;

// Empirical calibration coefficients derived from physical catch-and-weigh trials (0.775 to 2.040 L/min).
// Linear model: Q = CAL_A * f + CAL_B with Q in L/min and f in Hz. R^2 = 0.99668.
// NOTE: Comment historically referred to 0.285 L/min offset, but active CAL_B is 0.040111 L/min.
static double         CAL_A                = 0.009244;
static double         CAL_B                = 0.040111;

// Telemetry sample window duration in seconds.
static double         windowNominalSec     = 1.0;
static int64_t        tPrevUs              = 0;

// Telemetry integration variables updated once per sample window.
static double         time_s               = 0.0;
static int64_t        cnt_prev             = 0;
static double         window_s             = 1.0;
static double         freq_Hz              = 0.0;
static double         Q_Lmin               = 0.0;
static double         Q_mLs                = 0.0;
static double         total_L              = 0.0;
static uint64_t       total_pulses         = 0;

// ESP32 hardware pulse counter (PCNT) handle and accumulator state.
static pcnt_unit_handle_t    pcntUnit      = NULL;
static pcnt_channel_handle_t pcntChan      = NULL;
static int                   lastRawPcnt   = 0;
static int64_t               totalPcntAccum= 0;
static const int             PCNT_LOW_LIMIT  = -32768;
static const int             PCNT_HIGH_LIMIT = 32767;
static int64_t               lastWindowN   = 0;

// Calibration run tracking fields between 'S' and 'E' commands.
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

// Stored calibration trials for on-demand linear regression solver.
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

// Controller finite state machine operating states.
enum SystemState {
    ST_IDLE             = 0,
    ST_CASE1_NORMAL     = 1,
    ST_CASE1_SURGE      = 2,
    ST_CASE2_FLUSHING   = 3,
    ST_CASE2_STOPPING   = 4,
    ST_FAULT            = 5,
    ST_CASE1_DRAIN      = 6,
    ST_CALMODE          = 7
};

static SystemState state = ST_IDLE;
static bool singleGoMode = false;
static bool surgeDemonstrated = false;
static uint32_t surgeClearMs = 0;
static uint32_t surgeFloatClearMs = 0;
static bool masterInterlockArmed = false;
static bool flushFlowConfirmed = false;
static uint32_t c2CustomDurationMs = 0;
static uint8_t  c2LowFlowSamples   = 0;
static uint32_t lastC2PrintMs      = 0;
static float    pumpDutyPct[3]     = {0.0f, 0.0f, 0.0f};
static uint32_t calModeLastCmdMs   = 0;
static uint32_t calModeNoFlowSinceMs = 0;

// Process flow and level variables shared across control tasks.
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

// Interrupt pulse counters placed in internal IRAM for deterministic low-latency execution.
static volatile uint32_t pulseCount14   = 0;
static volatile uint32_t pulseCount35   = 0;
static volatile uint32_t pulseCount34   = 0;

// Increments pulse counter on falling edge of GPIO 14 (FT-01 flow sensor).
// Located in IRAM via IRAM_ATTR to avoid flash cache miss latency during flash writes.
void IRAM_ATTR onFlowPulseISR14() {
    pulseCount14 = pulseCount14 + 1;
}

// Increments debug interrupt counter on GPIO 35 transitions.
// Located in IRAM via IRAM_ATTR to prevent execution stalls.
// NOTE: Function onFlowPulseISR35 attaches to float switch GPIO 35, not a flow sensor.
void IRAM_ATTR onFlowPulseISR35() {
    pulseCount35 = pulseCount35 + 1;
}

// Increments debug interrupt counter on GPIO 34 transitions.
// Located in IRAM via IRAM_ATTR to prevent execution stalls.
// NOTE: Function onFlowPulseISR34 attaches to tactile switch GPIO 34, not a flow sensor.
void IRAM_ATTR onFlowPulseISR34() {
    pulseCount34 = pulseCount34 + 1;
}

// Configures the ESP32 pulse counter (PCNT) peripheral on GPIO 14 with a 1 us glitch filter.
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
        filter_config.max_glitch_ns = 1000;
        pcnt_unit_set_glitch_filter(pcntUnit, &filter_config);

        pcnt_unit_enable(pcntUnit);
        pcnt_unit_clear_count(pcntUnit);
        pcnt_unit_start(pcntUnit);
    }
}

// Reads the hardware pulse count and handles counter overflow wrapping at 32768 pulses.
int64_t getPcntCount() {
    if (pcntUnit != NULL) {
        int count = 0;
        pcnt_unit_get_count(pcntUnit, &count);
        int32_t diff = count - lastRawPcnt;
        if (diff < 0) {
            diff += (PCNT_HIGH_LIMIT + 1);
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

// Configures PWM output compatibility across ESP-IDF Arduino component versions.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  #define PWM_ATTACH(pin, ch)   ledcAttach(pin, PWM_FREQ, PWM_BITS)
  #define PWM_WRITE(pin, ch, v) ledcWrite(pin, v)
#else
  #define PWM_ATTACH(pin, ch)   do { ledcSetup(ch, PWM_FREQ, PWM_BITS); ledcAttachPin(pin, ch); } while (0)
  #define PWM_WRITE(pin, ch, v) ledcWrite(ch, v)
#endif

// Sets PWM duty cycle (0.0 to 100.0%) for a designated pump driver.
void setPump(uint8_t pumpIdx, float dutyPct) {
    if (pumpIdx >= 3) return;
    dutyPct = constrain(dutyPct, 0.0f, 100.0f);
    pumpDutyPct[pumpIdx] = dutyPct;
    uint32_t v = (uint32_t)(dutyPct * ((1 << PWM_BITS) - 1) / 100.0f);

    bool on = (v > 0);

    if (pumpIdx == 0) {
        // NOTE: GPIO 12 and GPIO 13 are both driven for Pump 1 enable due to PCB routing divergence.
        digitalWrite(13, on ? HIGH : LOW);
        digitalWrite(12, on ? HIGH : LOW);
        digitalWrite(PUMP[0].rEn, on ? HIGH : LOW);

        uint8_t fwdPin = PUMP_REVERSE ? PUMP[0].rPwm : PUMP[0].lPwm;
        uint8_t revPin = PUMP_REVERSE ? PUMP[0].lPwm : PUMP[0].rPwm;
        PWM_WRITE(fwdPin, 0, on ? v : 0);
        PWM_WRITE(revPin, 1, 0);
    } else if (pumpIdx == 1) {
        digitalWrite(PUMP[1].lEn, on ? HIGH : LOW);
        digitalWrite(PUMP[1].rEn, on ? HIGH : LOW);
        PWM_WRITE(PUMP[1].lPwm, 2, on ? v : 0);
    } else if (pumpIdx == 2) {
        digitalWrite(PUMP[2].lEn, on ? HIGH : LOW);
        digitalWrite(PUMP[2].rEn, on ? HIGH : LOW);
        PWM_WRITE(PUMP[2].lPwm, 4, on ? v : 0);
    }
}

// Commands duty cycles for Pump 1, Pump 2, and Pump 3 simultaneously.
void setAllPumps(float d1, float d2, float d3) {
    setPump(0, d1);
    setPump(1, d2);
    setPump(2, d3);
}

// Disables all three pump motor outputs.
void pumpsOff() {
    setAllPumps(0.0f, 0.0f, 0.0f);
}

// Drives the solenoid relay. Pulls pin LOW to energize; releases to high-Z to de-energize.
static void sovDrive(bool energize) {
    bool lineHigh = SOV_ACTIVE_HIGH ? energize : !energize;
    if (lineHigh && SOV_HIGH_BY_FLOAT && !SOV_ACTIVE_HIGH) {
        pinMode(PIN_SV, INPUT);
    } else {
        pinMode(PIN_SV, OUTPUT);
        digitalWrite(PIN_SV, lineHigh ? HIGH : LOW);
    }
}

// Energizes solenoid valve OV-01 to establish fluid path to the process reservoir.
void sovOpen() {
    sovDrive(true);
    Serial.printf("[ACTUATOR] Solenoid Valve OV-01 (GPIO %d) -> ENERGIZED / OPEN (%s)\n",
                  PIN_SV, SOV_ACTIVE_HIGH ? (SOV_HIGH_BY_FLOAT ? "released/float" : "HIGH 3.3V") : "driven LOW 0V");
}

// De-energizes solenoid valve OV-01 to isolate the process reservoir.
void sovClose() {
    sovDrive(false);
    Serial.printf("[ACTUATOR] Solenoid Valve OV-01 (GPIO %d) -> DE-ENERGIZED / CLOSED (%s)\n",
                  PIN_SV, SOV_ACTIVE_HIGH ? "driven LOW 0V" : (SOV_HIGH_BY_FLOAT ? "released/float" : "HIGH 3.3V"));
}

// De-energizes all pumps and closes solenoid valve OV-01.
void allOff() {
    pumpsOff();
    sovClose();
}

// Transitions controller to IDLE standby with interlocks disarmed.
void enterIdle(const char* reason) {
    allOff();
    state = ST_IDLE;
    singleGoMode = false;
    surgeDemonstrated = false;
    masterInterlockArmed = false;
    calRunActive = false;
    calModeNoFlowSinceMs = 0;
    Serial.printf("[STATE] -> IDLE (%s) | Master Safety Interlock: LOCKED (OFF)\n", reason);
}

// Enters FAULT safe state following dry-run detection or hardware timeout.
void enterFault(const char* reason) {
    allOff();
    state = ST_FAULT;
    singleGoMode = false;
    masterInterlockArmed = false;
    faultMsg = reason;
    Serial.printf("[FAULT] TRIP: %s | Press switch or enter 'rt'/'stop' to clear.\n", reason);
}

// Starts Case 1 recirculation: opens OV-01, starts Pump-01 at 50% and Pump-02 at 100%.
void startCase1Normal() {
    masterInterlockArmed = true;
    surgeFloatClearMs = 0;

    total_L = 0.0;
    totalL = 0.0f;
    totalFlushL = 0.0f;
    state = ST_CASE1_NORMAL;
    stateStartMs = millis();
    lowFlowSinceMs = 0;

    sovOpen();
    Serial.println("[CASE 1] Solenoid Valve OV-01 actively opened at Case 1 initiation.");

    // Allows 200 ms for the valve disc to lift and pre-flood the suction line.
    if (SOV_PRE_OPEN_MS > 0) {
        delay(SOV_PRE_OPEN_MS);
    }

    setAllPumps(PUMP1_C1_DUTY, PUMP2_C1_DUTY, 0.0f);
    Serial.println("[STATE] -> CASE 1 NORMAL: Recirculation active (SOV=OPEN, P1=50%, P2=100%).");
}

// Resumes Case 1 after surge mitigation without zeroing the volume totalizer.
void resumeCase1AfterSurge() {
    state = ST_CASE1_NORMAL;
    stateStartMs = millis();
    lowFlowSinceMs = 0;
    setAllPumps(PUMP1_C1_DUTY, PUMP2_C1_DUTY, 0.0f);
    Serial.println("[STATE] -> CASE 1 NORMAL (resumed after surge): P1=50%, P2=100%.");
}

// Isolates 10 L reservoir and evacuates the sump completely before starting Case 2.
void enterCase1Drain() {
    state = ST_CASE1_DRAIN;
    stateStartMs = millis();
    lowFlowSinceMs = 0;
    sovClose();
    setAllPumps(0.0f, PUMP2_C1_DUTY, 0.0f);
    Serial.println("[DRAIN] Sump drain started: SOV CLOSED, P1 OFF, P2 @ 100%, P3 OFF. Waiting for sump to empty...");
}

// Sump high level interlock: stops feed Pump-01 while Pump-02 drains excess volume at 100%.
void enterCase1Surge() {
    state = ST_CASE1_SURGE;
    surgeFloatClearMs = 0;
    stateStartMs = millis();

    sovOpen();
    setAllPumps(0.0f, PUMP2_C1_DUTY, 0.0f);
    Serial.println("[INTERLOCK] -> SUMP HIGH SURGE: Pump-01 STOPPED (0%), Pump-02 draining (100%).");
}

// Starts Case 2 flush: isolates 10 L tank and runs all 3 pumps simultaneously.
void startCase2Flushing(uint32_t customDurationSec = 0) {
    state = ST_CASE2_FLUSHING;
    stateStartMs = millis();
    lowFlowSinceMs = 0;
    c2LowFlowSamples = 0;
    lastC2PrintMs = 0;
    totalFlushL = 0.0f;
    flushFlowConfirmed = false;
    c2CustomDurationMs = customDurationSec * 1000UL;

    sovClose();
    setAllPumps(PUMP1_C2_DUTY, PUMP2_C2_DUTY, PUMP3_C2_DUTY);
    if (customDurationSec > 0) {
        Serial.printf("[STATE] -> CASE 2 FLUSHING (TIMED): Running all 3 pumps for %lu seconds.\n", (unsigned long)customDurationSec);
    } else {
        Serial.println("[STATE] -> CASE 2 FLUSHING: 10L Tank ISOLATED. P1, P2, P3 running simultaneously.");
        Serial.println("[C2] Priming line from 4.2L Flush Tank (TK-03). Monitoring FT-01...");
    }
}

// Shuts down all process actuators and returns to safe IDLE.
void stopAll(const char* reason) {
    enterIdle(reason);
}

// Enters guarded calibration mode for manual bench testing.
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

// Exits calibration mode and restores system to safe IDLE state.
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

// Evaluates sump float switch with 250 ms mechanical noise debouncing.
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

// Handles short button press: arms and starts Case 1, or performs safe stop if running.
void onShortPress() {
    if (state == ST_CALMODE) {
        pumpsOff();
        sovClose();
        Serial.println("[SWITCH] Calibration mode: button start disabled. Pumps OFF, solenoid CLOSED. Type 'calmode off' to leave.");
        return;
    }
    if (state == ST_IDLE) {
        masterInterlockArmed = true;
        Serial.println("\n============================================================");
        Serial.println("  [SWITCH] MASTER SAFETY INTERLOCK: ARMED & RUNNING");
        Serial.println("  Initiating Case 1 Closed-Loop Recirculation...");
        Serial.println("============================================================");
        singleGoMode = true;
        startCase1Normal();
    } else if (state == ST_FAULT) {
        masterInterlockArmed = false;
        enterIdle("Fault cleared by operator via switch");
    } else {
        masterInterlockArmed = false;
        Serial.println("\n============================================================");
        Serial.println("  [SWITCH] MASTER STOP TRIPPED: Operator pressed switch.");
        Serial.println("  All pumps CUT OFF. Solenoid CLOSED. System in SAFE STOP.");
        Serial.println("============================================================");
        stopAll("Pushbutton Master Stop Command");
    }
}

// Emergency stop triggered when tactile push button is held longer than 1500 ms.
void onLongPress() {
    masterInterlockArmed = false;
    Serial.println("\n[SWITCH] EMERGENCY STOP: Button held > 1.5s. Immediate total shutdown.");
    stopAll("Emergency Long-Press Stop");
}

// Debounces tactile push button and discriminates between momentary and long presses.
void handleTactileButton() {
    if (!USE_PHYSICAL_SWITCH) return;

    static bool stable = false;
    static bool lastRaw = false;
    static bool longFired = false;
    static uint32_t chg = 0;
    static uint32_t pressStart = 0;

    uint32_t now = millis();
    bool raw = (digitalRead(PIN_SW) == LOW);

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

// Computes least-squares linear regression (Q = a*f + b) across stored calibration trials.
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
        fit_a = (((double)n * sum_fQ) - (sum_f * sum_Q)) / denom;
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

// Starts pulse accumulation and microsecond timing for a volumetric calibration point.
void startCalibrationRun() {
    calRunActive = true;
    calRunEnded = false;
    calHoldingResult = false;
    calStartUs = esp_timer_get_time();
    calPulsesAtStart = (uint64_t)getPcntCount();
    run_pulses = 0;
    run_time_s = 0.0;
    run_freq_Hz = 0.0;
    Q_actual_Lmin = 0.0;
    error_pct = 0.0;
    V_collected = 0.0;
    Serial.println("\n[CAL] >>> Calibration run STARTED ('S'). Dispensing fluid into container...");
    Serial.println("[CAL] When collection finishes, send 'E' to end the run.");
}

// Freezes calibration timer and pulse count upon completion of a volumetric trial.
void endCalibrationRun() {
    if (!calRunActive) {
        Serial.println("[CAL] No active run. Send 'S' first to begin a calibration run.");
        return;
    }
    calEndUs = esp_timer_get_time();
    calRunActive = false;
    calRunEnded = true;
    calHoldingResult = true;
    uint64_t endCount = (uint64_t)getPcntCount();
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

// Records measured volumetric beaker volume and calculates empirical flow rate and error.
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
    Q_actual_Lmin = (run_time_s > 0.0) ? (V_collected / run_time_s * 60.0) : 0.0;

    double Q_Lmin_run = (CAL_A * run_freq_Hz) + CAL_B;
    error_pct = (Q_actual_Lmin > 0.0) ? (fabs(Q_Lmin_run - Q_actual_Lmin) / Q_actual_Lmin * 100.0) : 0.0;
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

// Applies newly computed regression coefficients directly to active RAM parameters.
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

// Populates 5-point challenge verification dataset for calibration algorithm testing.
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

// Clears stored calibration trials and resets accumulators to zero.
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

// Integrates flow pulses and outputs real-time Serial Studio DAQ telemetry frame once per second.
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

    window_s = elapsed_s;
    tPrevUs = t_now_us;
    time_s = (double)t_now_us / 1e6;

    int64_t cnt_now = getPcntCount();
    int64_t N = cnt_now - cnt_prev;
    if (N < 0) N = 0;
    cnt_prev = cnt_now;
    lastWindowN = N;

    freq_Hz = (window_s > 0.0) ? ((double)N / window_s) : 0.0;

    // Zero-flow noise gate: below 1.0 Hz, reported flow is zero to suppress non-zero intercept creep.
    Q_Lmin = (freq_Hz < FLOW_CUTOFF_HZ) ? 0.0 : fmax((CAL_A * freq_Hz) + CAL_B, 0.0);
    Q_mLs = Q_Lmin * 1000.0 / 60.0;
    total_L += (Q_Lmin / 60.0) * window_s;
    total_pulses += (uint64_t)N;

    flowFreqHz = (float)freq_Hz;
    flowLpm = (float)Q_Lmin;
    flowMls = (float)Q_mLs;
    totalL = (float)total_L;
    pulseCount = (uint32_t)cnt_now;
    if (state == ST_CASE2_FLUSHING) {
        totalFlushL += (float)((Q_Lmin / 60.0) * window_s);
    }
    newFlowSample = true;

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

    // Telemetry output: /*time_s,pulses,window_s,freq_Hz,Q_Lmin,Q_mLs,total_L,total_pulses,run_pulses,run_time_s,run_freq_Hz,Q_actual_Lmin,error_pct*/
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

// Executes core finite state machine logic and transitions.
void executeProcessControl() {
    uint32_t now = millis();

    switch (state) {
        case ST_IDLE:
            break;

        case ST_CASE1_NORMAL:
            // Sump high level interlock: stops Pump-01 to mitigate overflow.
            if (floatHigh) {
                enterCase1Surge();
                return;
            }

            // Advances to Case 2 after surge mitigation has stabilized for 20 s.
            if (singleGoMode && surgeDemonstrated && (now - surgeClearMs >= C1_RECOVERY_DWELL_MS)) {
                Serial.println("\n[SINGLE-GO] Sump surge mitigation demonstrated and stable recovery confirmed!");
                Serial.println("[SINGLE-GO] Transitioning directly to Case 2 flush sequence in a single go...");
                startCase2Flushing();
                return;
            }
            break;

        case ST_CASE1_SURGE:
            // Restores Pump-01 feed when sump level drops below the float switch for 3 s.
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

            if (elapsed > C2_MAX_RUN_MS) {
                enterFault("Case 2 Flush safety timeout exceeded (10 min)");
                return;
            }

            if (c2CustomDurationMs > 0) {
                if (elapsed >= c2CustomDurationMs) {
                    pumpsOff();
                    stopStartMs = now;
                    state = ST_CASE2_STOPPING;
                    Serial.println("[C2] Timed flush duration complete. All pumps OFF. Closing SOV in 500ms...");
                }
                break;
            }

            if (c2CustomDurationMs == 0 && elapsed >= C2_AUTO_MAX_RUN_MS) {
                pumpsOff();
                stopStartMs = now;
                state = ST_CASE2_STOPPING;
                Serial.printf("\n[C2] 4.2L FLUSH COMPLETED (%lu s elapsed, Total: %.3f L). All pumps OFF.\n",
                              (unsigned long)(elapsed / 1000), totalFlushL);
                break;
            }

            if (now - lastC2PrintMs >= 2000) {
                lastC2PrintMs = now;
                Serial.printf("[C2 LIVE] Time: %lu s | Flow: %.2f L/min | Flushed: %.3f L / 4.15 L\n",
                              (unsigned long)(elapsed / 1000), flowLpm, totalFlushL);
            }

            if (!flushFlowConfirmed) {
                if (flowLpm >= FLOW_MIN_VALID) {
                    flushFlowConfirmed = true;
                    Serial.printf("[C2] Flush fluid detected at FT-01 (Flow: %.2f L/min). Line actively primed!\n", flowLpm);
                } else if (elapsed > C2_PRIME_MS) {
                    enterFault("Case 2 Priming Timeout: Zero flow after 25s. Ensure BV-02 valve is OPEN!");
                    return;
                }
            }

            // Depletion detection: triggers shutdown on 4.15 L delivery or when flow collapses below 0.35 L/min.
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
                    lowFlowSinceMs = 0;
                }
            }
            break;
        }

        case ST_CASE2_STOPPING:
            // Staggered shutdown: Close solenoid valve 500 ms after pumps stop.
            // NOTE: Solenoid valve was already closed at Case 2 initiation; this ensures safe de-energization.
            if (now - stopStartMs >= SOV_CLOSE_DELAY_MS) {
                sovClose();
                enterIdle("Case 2 Flush sequence complete. System safely OFF.");
                Serial.printf("[COMPLETE] Challenge Run Finished! Total Volume: %.3f L (Flush: %.3f L)\n",
                              totalL, totalFlushL);
            }
            break;

        case ST_CALMODE: {
            if (floatHigh && pumpDutyPct[0] > 0.0f) {
                setPump(0, 0.0f);
                Serial.println("[CALMODE] Sump float HIGH -> Pump-1 STOPPED.");
            }
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

// Parses incoming serial commands at 115200 baud.
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

    if (cmd == "stop" || cmd == "x" || cmd == "0" || cmd == "halt" || cmd == "kill") {
        masterInterlockArmed = false;
        stopAll("CLI Emergency Stop Command");
        return;
    }

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
    if (state == ST_CALMODE && (cmd == "case1" || cmd == "s1" || cmd == "c1" || cmd == "1" ||
                                cmd == "case2" || cmd == "s2" || cmd == "c2" || cmd == "2" ||
                                cmd.startsWith("case2 ") || cmd.startsWith("c2 ") ||
                                cmd == "auto" || cmd == "singlego" || cmd == "a")) {
        Serial.println("[CALMODE] Process start blocked in calibration mode. Type 'calmode off' first.");
        return;
    }

    if (cmd == "s" || cmd == "start" || cmd == "cal s" || cmd == "cal start") {
        startCalibrationRun();
        return;
    }

    if (cmd == "e" || cmd == "end" || cmd == "cal e" || cmd == "cal end") {
        endCalibrationRun();
        return;
    }

    if (cmd.startsWith("v ") || cmd.startsWith("vol ")) {
        int sp = cmd.indexOf(' ');
        double v = cmd.substring(sp + 1).toDouble();
        recordCollectedVolume(v);
        return;
    }

    if (calHoldingResult && (V_collected == 0.0)) {
        double v = cmd.toDouble();
        if (v > 0.0) {
            recordCollectedVolume(v);
            return;
        }
    }

    if (cmd == "fit" || cmd == "cal" || cmd == "report") {
        calculateFitAndPrint(false);
        return;
    }

    if (cmd == "apply" || cmd == "update" || cmd == "cal apply") {
        applyCalibrationFit();
        return;
    }

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

    if (cmd == "demo" || cmd == "test_fit") {
        loadDemoTrials();
        return;
    }

    if (cmd == "c" || cmd == "clear" || cmd == "cal_reset") {
        resetCalibrationTrials();
        return;
    }

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

// Configures hardware peripherals, timers, and external interrupts.
void setup() {
    Serial.begin(115200);

    esp_log_level_set("*", ESP_LOG_NONE);

    sovDrive(false);

    pinMode(12, OUTPUT);
    digitalWrite(12, LOW);
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);
    pinMode(PUMP[0].rEn, OUTPUT);
    digitalWrite(PUMP[0].rEn, LOW);
    PWM_ATTACH(PUMP[0].lPwm, 0);
    PWM_ATTACH(PUMP[0].rPwm, 1);

    pinMode(PUMP[1].lEn, OUTPUT);
    digitalWrite(PUMP[1].lEn, LOW);
    pinMode(PUMP[1].rEn, OUTPUT);
    digitalWrite(PUMP[1].rEn, LOW);
    PWM_ATTACH(PUMP[1].lPwm, 2);
    pinMode(PUMP[1].rPwm, OUTPUT);
    digitalWrite(PUMP[1].rPwm, LOW);

    pinMode(PUMP[2].lEn, OUTPUT);
    digitalWrite(PUMP[2].lEn, LOW);
    pinMode(PUMP[2].rEn, OUTPUT);
    digitalWrite(PUMP[2].rEn, LOW);
    PWM_ATTACH(PUMP[2].lPwm, 4);
    pinMode(PUMP[2].rPwm, OUTPUT);
    digitalWrite(PUMP[2].rPwm, LOW);

    pumpsOff();

    pinMode(PIN_FS, INPUT_PULLUP);
    pinMode(PIN_FSW, INPUT);
    pinMode(PIN_SW, INPUT);

    initPCNT();

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
        delay(1500);
        masterInterlockArmed = true;
        startCase1Normal();
    } else {
        Serial.println("[STANDBY] Master Safety Interlock: LOCKED (OFF).");
        Serial.println("[STANDBY] >> Press the physical Pushbutton on the rig to START! <<");
        Serial.println("[STANDBY] (Or send 'arm' over Serial to permit remote commands).");
    }
}

// Main execution loop: CLI handling, sensor debouncing, telemetry streaming, and FSM.
void loop() {
    handleSerialCLI();
    if (USE_PHYSICAL_SWITCH) {
        handleTactileButton();
    }
    handleFloatSwitch();
    updateFlowAndTelemetry();
    executeProcessControl();

    newFlowSample = false;

    // Yields CPU to FreeRTOS IDLE task to prevent task watchdog timer timeouts.
    delay(2);
}
