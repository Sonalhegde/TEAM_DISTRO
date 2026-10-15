/*
 * ESP32 Process Controller for Lam Research Challenge 3.0 Stage 2 (LRC-26-0528)
 * Target: ESP32 NodeMCU-32S / DevKitC on LAM Stage 2 PCB Rev 1.0
 *
 * Case 1: Closed-loop recirculation at 3.0 L/min target (P1 at 50%, P2 at 100%).
 *         Sump high level stops P1 while P2 drains excess to the 10 L reservoir.
 *         P1 automatically restarts when the sump level drops below the float switch.
 * Case 2: Flush sequence with the 10 L reservoir isolated by closing solenoid OV-01.
 *         P1, P2, and P3 run concurrently from the 4.2 L flush reservoir.
 *         Automatic shutdown triggers when fluid depletes and flow collapses.
 */

#include <Arduino.h>
#include <math.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/pulse_cnt.h"
#include "config.h"

// Runtime-mutable operating parameters initialized from config.h defaults
static double   CAL_A                 = DEFAULT_CAL_SLOPE_A_LPM_PER_HZ;
static double   CAL_B                 = DEFAULT_CAL_INTERCEPT_B_LPM;
static double   windowNominalSec      = DEFAULT_WINDOW_NOMINAL_SEC;
static bool     SOV_ACTIVE_HIGH       = DEFAULT_SOV_ACTIVE_HIGH;

// Operating state and interlocks
static SystemState state              = ST_IDLE;
static bool     singleGoMode          = false;
static bool     masterInterlockArmed  = false;
static bool     surgeDemonstrated     = false;
static uint32_t surgeClearMs          = 0;
static uint32_t surgeFloatClearMs     = 0;

static uint32_t sovOpenStartMs        = 0;
static bool     c1PumpsStarted        = false;
static bool     flushFlowConfirmed    = false;
static uint32_t c2CustomDurationMs    = 0;
static uint8_t  c2LowFlowSamples      = 0;
static uint32_t lastC2PrintMs         = 0;

static float    pumpDutyPct[3]        = {0.0f, 0.0f, 0.0f};
static uint32_t stateStartMs          = 0;
static uint32_t lowFlowSinceMs        = 0;
static uint32_t stopStartMs           = 0;
static const char* faultMsg           = "";

// Float switch debounced state
static bool     floatHigh             = false;
static uint32_t floatChangeMs         = 0;
static bool     floatRawPrev          = false;

// Calibration mode state guards
static uint32_t calModeLastCmdMs      = 0;
static uint32_t calModeNoFlowSinceMs  = 0;
static bool     calRunActive          = false;
static bool     calRunEnded           = false;
static bool     calHoldingResult      = false;
static int64_t  calStartUs            = 0;
static int64_t  calEndUs              = 0;
static uint64_t calPulsesAtStart      = 0;
static uint64_t run_pulses            = 0;
static double   run_time_s            = 0.0;
static double   run_freq_Hz           = 0.0;
static double   Q_actual_Lmin         = 0.0;
static double   error_pct             = 0.0;
static double   V_collected           = 0.0;

// Telemetry integration variables
static int64_t  tPrevUs               = 0;
static double   time_s                = 0.0;
static int64_t  cnt_prev              = 0;
static double   window_s              = 1.0;
static double   freq_Hz               = 0.0;
static double   Q_Lmin                = 0.0;
static double   Q_mLs                 = 0.0;
static double   total_L               = 0.0;
static uint64_t total_pulses          = 0;
static int64_t  lastWindowN           = 0;
static float    flowLpm               = 0.0f;
static float    totalFlushL           = 0.0f;
static bool     newFlowSample         = false;

// Hardware Pulse Counter (PCNT) handle and accumulator
static pcnt_unit_handle_t    pcntUnit        = NULL;
static pcnt_channel_handle_t pcntChan        = NULL;
static int                   lastRawPcnt     = 0;
static int64_t               totalPcntAccum  = 0;
static volatile int64_t      pcntWrapAccum   = 0;

// Calibration trial storage and linear regression solver
struct CalTrial {
    int      trialId;
    uint64_t pulses;
    double   time_s;
    double   freq_Hz;
    double   vol_L;
    double   Q_actual;
    double   error_pct;
};

static const int MAX_TRIALS = 20;
static CalTrial  trials[MAX_TRIALS];
static int       trialCount = 0;
static double    fit_a      = 0.0;
static double    fit_b      = 0.0;
static double    fit_r2     = 0.0;
static double    fit_max_err= 0.0;
static double    fit_avg_err= 0.0;
static bool      fit_valid  = false;

// Forward declarations
void enterIdle(const char* reason);
void enterFault(const char* reason);
void stopAll(const char* reason);
void setPump(uint8_t pumpIdx, float dutyPct);
void setAllPumps(float d1, float d2, float d3);
void pumpsOff();
void sovOpen();
void sovClose();

// ESP-IDF v5.2.x link compatibility stub for arduino-esp32 3.0.0 CPU frequency scaling
extern "C" void __attribute__((weak)) esp_timer_impl_update_apb_freq(uint32_t apb_ticks_per_us) {
    (void)apb_ticks_per_us;
}

// PCNT watchpoint callback fired when counter reaches positive upper limit (32767)
static bool IRAM_ATTR onPcntWatchpoint(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) {
    pcntWrapAccum += (int64_t)PCNT_UPPER_LIMIT_PULSES;
    return false;
}

// Configures the ESP32 pulse counter (PCNT) peripheral on GPIO 14 with a 1000 ns glitch filter
void initPCNT() {
    pcnt_unit_config_t unit_config = {};
    unit_config.low_limit = PCNT_LOWER_LIMIT_PULSES;
    unit_config.high_limit = PCNT_UPPER_LIMIT_PULSES;
    unit_config.flags.accum_count = 1;

    esp_err_t err = pcnt_new_unit(&unit_config, &pcntUnit);
    if (err != ESP_OK) {
        Serial.printf("[PCNT] ERROR: Failed to create PCNT unit (err %d)\n", err);
        enterFault("Hardware PCNT peripheral unit creation failed");
        return;
    }

    pcnt_chan_config_t chan_config = {};
    chan_config.edge_gpio_num = PIN_FS_GPIO;
    chan_config.level_gpio_num = -1;

    err = pcnt_new_channel(pcntUnit, &chan_config, &pcntChan);
    if (err != ESP_OK) {
        Serial.printf("[PCNT] ERROR: Failed to create PCNT channel (err %d)\n", err);
        enterFault("Hardware PCNT channel init failed");
        return;
    }

    // Counts on rising edge: open-collector hall-effect transistor releases pin to external pull-up
    pcnt_channel_set_edge_action(pcntChan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
    pcnt_channel_set_level_action(pcntChan, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_KEEP);

    pcnt_glitch_filter_config_t filter_config = {};
    filter_config.max_glitch_ns = 1000;
    pcnt_unit_set_glitch_filter(pcntUnit, &filter_config);

    pcnt_event_callbacks_t cbs = {};
    cbs.on_reach = onPcntWatchpoint;
    pcnt_unit_register_event_callbacks(pcntUnit, &cbs, NULL);
    pcnt_unit_add_watch_point(pcntUnit, PCNT_UPPER_LIMIT_PULSES);

    pcnt_unit_enable(pcntUnit);
    pcnt_unit_clear_count(pcntUnit);
    pcnt_unit_start(pcntUnit);
    Serial.println("[PCNT] Pulse counter unit online on GPIO 14 (rising edge, 1us filter).");
}

// Reads hardware pulse count and handles counter overflow wrapping
int64_t getPcntCount() {
    if (pcntUnit == NULL) {
        return 0;
    }
    int count = 0;
    pcnt_unit_get_count(pcntUnit, &count);
    int32_t diff = count - lastRawPcnt;
    if (diff < 0) {
        diff += (PCNT_UPPER_LIMIT_PULSES + 1);
    }
    lastRawPcnt = count;
    totalPcntAccum += diff;
    return totalPcntAccum;
}

// Symmetric initialization for BTS7960 high-power half-bridge motor drivers
void initPumpDriver(uint8_t pumpIdx) {
    if (pumpIdx >= 3) return;
    const PumpPinAssignment& p = PUMP_PINS[pumpIdx];

    pinMode(p.pin_l_en, OUTPUT);
    digitalWrite(p.pin_l_en, LOW);

    if (p.pin_l_en_alt != 0) {
        pinMode(p.pin_l_en_alt, OUTPUT);
        digitalWrite(p.pin_l_en_alt, LOW);
    }

    pinMode(p.pin_r_en, OUTPUT);
    digitalWrite(p.pin_r_en, LOW);

    // Forward PWM on left half-bridge input (LEDC peripheral)
    ledcAttach(p.pin_l_pwm, PWM_FREQUENCY_HZ, PWM_RESOLUTION_BITS);
    ledcWrite(p.pin_l_pwm, 0);

    // Reverse input tied LOW: hydraulic centrifugal pumps operate in forward direction only
    pinMode(p.pin_r_pwm, OUTPUT);
    digitalWrite(p.pin_r_pwm, LOW);
}

// Sets PWM duty cycle (0.0 to 100.0%) for a designated pump driver
void setPump(uint8_t pumpIdx, float dutyPct) {
    if (pumpIdx >= 3) return;
    dutyPct = constrain(dutyPct, 0.0f, 100.0f);
    pumpDutyPct[pumpIdx] = dutyPct;
    uint32_t v = (uint32_t)(dutyPct * ((1 << PWM_RESOLUTION_BITS) - 1) / 100.0f);
    bool on = (v > 0);

    const PumpPinAssignment& p = PUMP_PINS[pumpIdx];
    digitalWrite(p.pin_l_en, on ? HIGH : LOW);
    if (p.pin_l_en_alt != 0) {
        digitalWrite(p.pin_l_en_alt, on ? HIGH : LOW);
    }
    digitalWrite(p.pin_r_en, on ? HIGH : LOW);

    ledcWrite(p.pin_l_pwm, on ? v : 0);
    digitalWrite(p.pin_r_pwm, LOW);
}

void setAllPumps(float d1, float d2, float d3) {
    setPump(0, d1);
    setPump(1, d2);
    setPump(2, d3);
}

void pumpsOff() {
    setAllPumps(0.0f, 0.0f, 0.0f);
}

// Drives the solenoid relay driver pin
static void sovDrive(bool energize) {
    bool lineHigh = SOV_ACTIVE_HIGH ? energize : !energize;
    if (lineHigh && SOV_HIGH_BY_FLOAT && !SOV_ACTIVE_HIGH) {
        pinMode(PIN_SV_GPIO, INPUT);
    } else {
        pinMode(PIN_SV_GPIO, OUTPUT);
        digitalWrite(PIN_SV_GPIO, lineHigh ? HIGH : LOW);
    }
}

void sovOpen() {
    sovDrive(true);
    Serial.printf("[ACTUATOR] Solenoid Valve OV-01 (GPIO %d) -> ENERGIZED / OPEN\n", PIN_SV_GPIO);
}

void sovClose() {
    sovDrive(false);
    Serial.printf("[ACTUATOR] Solenoid Valve OV-01 (GPIO %d) -> DE-ENERGIZED / CLOSED\n", PIN_SV_GPIO);
}

void allOff() {
    pumpsOff();
    sovClose();
}

void enterIdle(const char* reason) {
    allOff();
    state = ST_IDLE;
    singleGoMode = false;
    surgeDemonstrated = false;
    masterInterlockArmed = false;
    calRunActive = false;
    calModeNoFlowSinceMs = 0;
    c1PumpsStarted = false;
    Serial.printf("[STATE] -> IDLE (%s) | Master Safety Interlock: LOCKED (OFF)\n", reason);
}

void enterFault(const char* reason) {
    allOff();
    state = ST_FAULT;
    singleGoMode = false;
    masterInterlockArmed = false;
    faultMsg = reason;
    c1PumpsStarted = false;
    Serial.printf("[FAULT] TRIP: %s | Press switch or enter 'rt'/'stop' to clear.\n", reason);
}

// Starts Case 1 recirculation: energizes OV-01, pre-opens non-blockingly, then starts P1 @ 50% and P2 @ 100%
void startCase1Normal() {
    masterInterlockArmed = true;
    surgeFloatClearMs = 0;
    total_L = 0.0;
    totalFlushL = 0.0f;
    state = ST_CASE1_NORMAL;
    stateStartMs = millis();
    lowFlowSinceMs = 0;

    sovOpen();
    sovOpenStartMs = millis();
    c1PumpsStarted = false;
    Serial.println("[CASE 1] Solenoid Valve OV-01 opening. Suction line priming (200ms non-blocking lead)...");
}

void resumeCase1AfterSurge() {
    state = ST_CASE1_NORMAL;
    stateStartMs = millis();
    lowFlowSinceMs = 0;
    c1PumpsStarted = true;
    setAllPumps(PUMP1_C1_DUTY_PCT, PUMP2_C1_DUTY_PCT, 0.0f);
    Serial.println("[STATE] -> CASE 1 NORMAL (resumed after surge): P1=50%, P2=100%.");
}

void enterCase1Drain() {
    state = ST_CASE1_DRAIN;
    stateStartMs = millis();
    lowFlowSinceMs = 0;
    c1PumpsStarted = false;
    sovClose();
    setAllPumps(0.0f, PUMP2_C1_DUTY_PCT, 0.0f);
    Serial.println("[DRAIN] Sump drain started: SOV CLOSED, P1 OFF, P2 @ 100%, P3 OFF. Evacuating sump...");
}

void enterCase1Surge() {
    state = ST_CASE1_SURGE;
    surgeFloatClearMs = 0;
    stateStartMs = millis();
    c1PumpsStarted = false;
    sovOpen();
    setAllPumps(0.0f, PUMP2_C1_DUTY_PCT, 0.0f);
    Serial.println("[INTERLOCK] -> SUMP HIGH SURGE: Pump-01 STOPPED (0%), Pump-02 draining (100%).");
}

// Starts Case 2 flush: isolates 10 L tank and runs all 3 pumps simultaneously
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
    setAllPumps(PUMP1_C2_DUTY_PCT, PUMP2_C2_DUTY_PCT, PUMP3_C2_DUTY_PCT);
    if (customDurationSec > 0) {
        Serial.printf("[STATE] -> CASE 2 FLUSHING (TIMED): Running all 3 pumps for %lu seconds.\n", (unsigned long)customDurationSec);
    } else {
        Serial.println("[STATE] -> CASE 2 FLUSHING: 10L Tank ISOLATED. P1, P2, P3 running simultaneously.");
        Serial.println("[C2] Priming line from 4.2L Flush Tank. Monitoring FT-01...");
    }
}

void stopAll(const char* reason) {
    enterIdle(reason);
}

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
    Serial.printf("  Trials recorded: %d | Active CAL_A = %.6f, CAL_B = %.6f\n", trialCount, CAL_A, CAL_B);
    Serial.println("============================================================");
}

// Sump float switch debouncing (contact closes to GND on high liquid level)
void handleFloatSwitch() {
    bool raw = digitalRead(PIN_FSW_GPIO);
    bool high = FLOAT_ACTIVE_LEVEL_LOW ? (raw == LOW) : (raw == HIGH);
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
        pumpsOff();
        sovClose();
        Serial.println("[SWITCH] Calibration mode: button start disabled. Type 'calmode off' to leave.");
        return;
    }
    if (state == ST_IDLE) {
        masterInterlockArmed = true;
        Serial.println("\n[SWITCH] MASTER INTERLOCK: ARMED & RUNNING. Starting Case 1 Recirculation...");
        singleGoMode = true;
        startCase1Normal();
    } else if (state == ST_FAULT) {
        masterInterlockArmed = false;
        enterIdle("Fault cleared by operator via switch");
    } else {
        masterInterlockArmed = false;
        Serial.println("\n[SWITCH] MASTER STOP: All actuators cut off. Safe stop active.");
        stopAll("Pushbutton Master Stop");
    }
}

void onLongPress() {
    masterInterlockArmed = false;
    Serial.println("\n[SWITCH] EMERGENCY STOP: Button held > 1.5s. Immediate shutdown.");
    stopAll("Emergency Long-Press Stop");
}

void handleTactileButton() {
    if (!USE_PHYSICAL_SWITCH) return;

    static bool stable = false;
    static bool lastRaw = false;
    static bool longFired = false;
    static uint32_t chg = 0;
    static uint32_t pressStart = 0;

    uint32_t now = millis();
    bool raw = (digitalRead(PIN_SW_GPIO) == LOW);

    if (raw != lastRaw) {
        lastRaw = raw;
        chg = now;
    }

    if ((now - chg >= SWITCH_DEBOUNCE_MS) && (raw != stable)) {
        stable = raw;
        if (stable) {
            pressStart = now;
            longFired = false;
        } else if (!longFired) {
            onShortPress();
        }
    }

    if (stable && !longFired && (now - pressStart >= SWITCH_LONG_PRESS_MS)) {
        longFired = true;
        onLongPress();
    }
}

// Least-squares regression analysis: Q = a * f + b
void calculateFitAndPrint(bool brief = false) {
    if (trialCount == 0) {
        Serial.println("[CAL] No calibration trials recorded. Send 'S' -> 'E' -> 'V <vol>' to record.");
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
        Serial.printf("  Fitted Slope (a)                   : %.6f L/min per Hz\n", fit_a);
        Serial.printf("  Fitted Offset (b)                  : %.6f L/min\n", fit_b);
        Serial.printf("  Determination Coeff (R^2)          : %.5f\n", fit_r2);
        Serial.printf("  Max Calibration Error              : %.2f %%\n", fit_max_err);
        Serial.printf("  Average Calibration Error          : %.2f %%\n", fit_avg_err);
        Serial.printf("  Current Active Calibration in Frame: CAL_A = %.6f, CAL_B = %.6f\n", CAL_A, CAL_B);
        Serial.println("  >> Type 'APPLY' to activate fitted constants in active telemetry. <<");
        Serial.println("================================================================================\n");
    } else {
        fit_valid = false;
        for (int i = 0; i < n; i++) {
            double q_cur = (CAL_A * trials[i].freq_Hz) + CAL_B;
            Serial.printf("#%-5d | %-10llu | %-9.3f | %-11.2f | %-9.3f | %-12.3f | %-10.3f | %-9.2f\n",
                          trials[i].trialId, (unsigned long long)trials[i].pulses, trials[i].time_s,
                          trials[i].freq_Hz, trials[i].vol_L, trials[i].Q_actual, q_cur, trials[i].error_pct);
        }
        Serial.println("================================================================================\n");
    }
}

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
    Serial.println("\n[CAL] >>> Calibration run STARTED ('S'). Dispensing fluid into beaker...");
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
    uint64_t endCount = (uint64_t)getPcntCount();
    run_pulses = (endCount >= calPulsesAtStart) ? (endCount - calPulsesAtStart) : 0;
    run_time_s = (double)(calEndUs - calStartUs) / 1e6;
    run_freq_Hz = (run_time_s > 0.0) ? ((double)run_pulses / run_time_s) : 0.0;
    Q_actual_Lmin = 0.0;
    error_pct = 0.0;
    Serial.println("\n[CAL] >>> Calibration run ENDED ('E').");
    Serial.printf("[CAL] Pulses: %llu | Duration: %.3f s | Mean Freq: %.2f Hz\n",
                  (unsigned long long)run_pulses, run_time_s, run_freq_Hz);
    Serial.println("[CAL] Please enter measured collection volume: 'V <litres>' (e.g. 'V 1.00')");
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
    Q_actual_Lmin = (run_time_s > 0.0) ? (V_collected / run_time_s * 60.0) : 0.0;

    double Q_Lmin_run = (CAL_A * run_freq_Hz) + CAL_B;
    error_pct = (Q_actual_Lmin > 0.0) ? (fabs(Q_Lmin_run - Q_actual_Lmin) / Q_actual_Lmin * 100.0) : 0.0;

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
    Serial.printf("  Pulses: %llu | Time: %.3f s | Freq: %.2f Hz | Vol: %.3f L | Q_act: %.3f L/min | Err: %.2f %%\n",
                  (unsigned long long)run_pulses, run_time_s, run_freq_Hz, V_collected, Q_actual_Lmin, error_pct);
    Serial.println("================================================================================");
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
    Serial.printf("  CAL_A : %.6f  -->  %.6f (Fitted Slope)\n", old_a, CAL_A);
    Serial.printf("  CAL_B : %.6f  -->  %.6f (Fitted Intercept)\n", old_b, CAL_B);
    Serial.printf("  R^2: %.5f | Max Err: %.2f %% | Avg Err: %.2f %%\n", fit_r2, fit_max_err, fit_avg_err);
    Serial.println("================================================================================\n");

    if (calHoldingResult && Q_actual_Lmin > 0.0) {
        double q_run = (CAL_A * run_freq_Hz) + CAL_B;
        error_pct = fabs(q_run - Q_actual_Lmin) / Q_actual_Lmin * 100.0;
    }
}

// Populates the 6 physical experimental rig trials (0.775 to 2.040 L/min operating envelope)
void loadDemoTrials() {
    struct DemoPt { uint64_t n; double t; double v; };
    DemoPt pts[6] = {
        {6678, 30.584, 1.040},
        {5525, 30.378, 0.870},
        {4316, 30.777, 0.700},
        {2493, 30.210, 0.390},
        {4881, 30.571, 0.790},
        {6013, 30.409, 0.940}
    };
    trialCount = 0;
    for (int i = 0; i < 6; i++) {
        trials[i].trialId = i + 1;
        trials[i].pulses = pts[i].n;
        trials[i].time_s = pts[i].t;
        trials[i].freq_Hz = (double)pts[i].n / pts[i].t;
        trials[i].vol_L = pts[i].v;
        trials[i].Q_actual = (pts[i].v / pts[i].t) * 60.0;
        double q_cur = (CAL_A * trials[i].freq_Hz) + CAL_B;
        trials[i].error_pct = fabs(q_cur - trials[i].Q_actual) / trials[i].Q_actual * 100.0;
    }
    trialCount = 6;
    Serial.println("\n[CAL] Loaded 6 physical rig calibration trials (0.775 - 2.040 L/min).");
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
    Serial.println("[CAL] Reset calibration trials and run accumulators to zero.");
}

// Integrates flow pulses and outputs 13-field CSV frame
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

    // Zero-flow noise cutoff: below FLOW_CUTOFF_HZ, reported flow is forced to 0
    Q_Lmin = (freq_Hz < FLOW_CUTOFF_HZ) ? 0.0 : fmax((CAL_A * freq_Hz) + CAL_B, 0.0);
    Q_mLs = Q_Lmin * 1000.0 / 60.0;
    total_L += (Q_Lmin / 60.0) * window_s;
    total_pulses += (uint64_t)N;

    flowLpm = (float)Q_Lmin;
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

// Executes core finite state machine logic and transitions
void executeProcessControl() {
    uint32_t now = millis();

    switch (state) {
        case ST_IDLE:
            break;

        case ST_CASE1_NORMAL:
            // Non-blocking pre-open sequence for solenoid valve OV-01
            if (!c1PumpsStarted) {
                if (now - sovOpenStartMs >= SOV_PRE_OPEN_LEAD_MS) {
                    setAllPumps(PUMP1_C1_DUTY_PCT, PUMP2_C1_DUTY_PCT, 0.0f);
                    c1PumpsStarted = true;
                    stateStartMs = now;
                    Serial.println("[STATE] -> CASE 1 NORMAL: Recirculation active (SOV=OPEN, P1=50%, P2=100%).");
                }
                return;
            }

            // Sump high level interlock: stops Pump-01 to mitigate overflow
            if (floatHigh) {
                enterCase1Surge();
                return;
            }

            // Advances to Case 2 after surge mitigation has stabilized for 20 s
            if (singleGoMode && surgeDemonstrated && (now - surgeClearMs >= C1_RECOVERY_DWELL_MS)) {
                Serial.println("\n[SINGLE-GO] Surge mitigation confirmed and recirculation stabilized.");
                Serial.println("[SINGLE-GO] Transitioning directly to Case 2 flush sequence in a single go...");
                startCase2Flushing();
                return;
            }
            break;

        case ST_CASE1_SURGE:
            // Restores Pump-01 feed when sump level drops below float switch for 3 s
            if (floatHigh) {
                surgeFloatClearMs = 0;
            } else {
                if (surgeFloatClearMs == 0) surgeFloatClearMs = now;
                if (now - surgeFloatClearMs >= SURGE_CLEAR_CONFIRMATION_MS) {
                    surgeDemonstrated = true;
                    surgeClearMs = now;
                    Serial.println("[INTERLOCK] Sump level normal. Restarting Pump-01 @ 50% duty.");
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
                              flowLpm, (unsigned long)((now - stateStartMs) / 1000), (unsigned long)(C1_DRAIN_SAFETY_TIMEOUT_MS / 1000));
            }
            if (newFlowSample) {
                if (flowLpm < FLOW_MIN_VALID_LPM) {
                    if (!lowFlowSinceMs) lowFlowSinceMs = now;
                    if (now - lowFlowSinceMs >= C1_DRAIN_CONFIRM_EMPTY_MS) {
                        Serial.println("[DRAIN] Sump empty (FT-01 flow collapsed). Advancing to Case 2 flush.");
                        startCase2Flushing();
                        return;
                    }
                } else {
                    lowFlowSinceMs = 0;
                }
            }
            if ((now - stateStartMs) >= C1_DRAIN_SAFETY_TIMEOUT_MS) {
                Serial.println("[DRAIN] Sump evacuation window reached. Advancing to Case 2 flush.");
                startCase2Flushing();
                return;
            }
            break;
        }

        case ST_CASE2_FLUSHING: {
            uint32_t elapsed = now - stateStartMs;

            if (elapsed > C2_GLOBAL_SAFETY_TIMEOUT_MS) {
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
                if (flowLpm >= FLOW_MIN_VALID_LPM) {
                    flushFlowConfirmed = true;
                    Serial.printf("[C2] Flush fluid detected at FT-01 (Flow: %.2f L/min). Line primed.\n", flowLpm);
                } else if (elapsed > C2_PRIME_WINDOW_MS) {
                    enterFault("Case 2 Priming Timeout: Zero flow after 25s. Ensure BV-01 is OPEN!");
                    return;
                }
            }

            // Depletion detection: triggers shutdown on 4.15 L delivery or when flow collapses below 0.35 L/min
            bool depletionArmed = flushFlowConfirmed && (elapsed >= C2_STARTUP_GRACE_MS) && (totalFlushL >= C2_MIN_FLUSH_ARM_L);

            if (depletionArmed && newFlowSample) {
                bool volumeExhausted = (totalFlushL >= FLUSH_VOLUME_CEILING_L);
                bool airSucked = (flowLpm < C2_FLOW_COLLAPSE_LPM);

                if (volumeExhausted) {
                    pumpsOff();
                    stopStartMs = now;
                    state = ST_CASE2_STOPPING;
                    Serial.printf("\n[C2] 4.2L FLUSH FINISHED BY VOLUME: %.3f L delivered! All pumps OFF.\n", totalFlushL);
                } else if (airSucked) {
                    c2LowFlowSamples++;
                    if (!lowFlowSinceMs) lowFlowSinceMs = now;
                    if ((now - lowFlowSinceMs >= C2_EXHAUST_PERSISTENCE_MS) || (c2LowFlowSamples >= 2)) {
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
            // Staggered shutdown: close solenoid valve 500 ms after pumps stop
            if (now - stopStartMs >= SOV_STAGGERED_CLOSE_DELAY_MS) {
                sovClose();
                enterIdle("Case 2 Flush sequence complete. System safely OFF.");
                Serial.printf("[COMPLETE] Challenge Run Finished! Total Volume: %.3f L (Flush: %.3f L)\n",
                              total_L, totalFlushL);
            }
            break;

        case ST_CALMODE: {
            if (floatHigh && pumpDutyPct[0] > 0.0f) {
                setPump(0, 0.0f);
                Serial.println("[CALMODE] Sump float HIGH -> Pump-1 STOPPED.");
            }
            if (pumpDutyPct[1] > 0.0f) {
                if (newFlowSample) {
                    if (flowLpm < CALMODE_NOFLOW_THRESHOLD_LPM) {
                        if (!calModeNoFlowSinceMs) calModeNoFlowSinceMs = now;
                        if (now - calModeNoFlowSinceMs >= CALMODE_PUMP2_NOFLOW_STOP_MS) {
                            setPump(1, 0.0f);
                            calModeNoFlowSinceMs = 0;
                            Serial.println("[CALMODE] Pump-2 no-flow guard: no flow for 5 s -> Pump-2 STOPPED.");
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

// Parses incoming serial commands at 115200 baud
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
        Serial.printf("[ACTUATOR] Solenoid polarity toggled: Active %s\n",
                      SOV_ACTIVE_HIGH ? "HIGH (relay ON when line HIGH)" : "LOW (relay ON when line pulled LOW)");
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

    if (cmd.startsWith("p1 ")) {
        float d = cmd.substring(3).toFloat();
        setPump(0, d);
        Serial.printf("[MANUAL] Pump 1 duty set to %.1f%%\n", d);
    } else if (cmd.startsWith("p2 ")) {
        float d = cmd.substring(3).toFloat();
        setPump(1, d);
        Serial.printf("[MANUAL] Pump 2 duty set to %.1f%%\n", d);
    } else if (cmd.startsWith("p3 ")) {
        float d = cmd.substring(3).toFloat();
        setPump(2, d);
        Serial.printf("[MANUAL] Pump 3 duty set to %.1f%%\n", d);
    } else if (cmd == "rt") {
        total_L = 0.0;
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
        Serial.println("  --- CALIBRATION PROTOCOL ---");
        Serial.println("  S                - Start calibration run (records pulses & time)");
        Serial.println("  E                - End calibration run (freezes run fields)");
        Serial.println("  V <litres>       - Record collected volume (e.g. 'V 1.00')");
        Serial.println("  FIT / CAL        - Display on-demand linear regression & error report");
        Serial.println("  APPLY            - Apply fitted a & b to CAL_A & CAL_B in active frame");
        Serial.println("  cal_a <val>      - Set slope CAL_A manually");
        Serial.println("  cal_b <val>      - Set offset CAL_B manually");
        Serial.println("  window <sec>     - Set nominal telemetry window (default 1.0 s)");
        Serial.println("  DEMO             - Load 6 physical rig calibration trials");
        Serial.println("  calmode on       - Enter guarded calibration mode");
        Serial.println("  calmode off      - Leave calibration mode");
        Serial.println("  C / CLEAR        - Clear calibration trials and reset run fields");
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
        Serial.println("  p3 <duty>        - Manual Pump 3 test (e.g. p3 100)");
        Serial.println("  sov <1/0>        - Manual Solenoid Valve toggle (1=Open, 0=Close)");
        Serial.println("  rt               - Reset totalizer to 0.000 L");
        Serial.println("  status           - Snapshot print of current hardware state");
        Serial.println("  ?                - Show this command reference");
        Serial.println("==================================================================");
    }
}

// Configures hardware peripherals, timers, and actuators
void setup() {
    Serial.begin(115200);

    esp_log_level_set("*", ESP_LOG_NONE);

    // Initial state: solenoid closed, all pump driver channels initialized LOW
    sovDrive(false);
    for (uint8_t i = 0; i < 3; i++) {
        initPumpDriver(i);
    }
    pumpsOff();

    // Inputs: external pull-ups on PCB
    pinMode(PIN_FS_GPIO, INPUT_PULLUP);
    pinMode(PIN_FSW_GPIO, INPUT);
    pinMode(PIN_SW_GPIO, INPUT);

    // Pulse counter peripheral on FT-01 (GPIO 14)
    initPCNT();

    tPrevUs = esp_timer_get_time();
    Serial.println("\n============================================================");
    Serial.println("  LAM RESEARCH CHALLENGE 3.0 - STAGE 2 PRODUCTION FIRMWARE   ");
    Serial.println("  TEAM: DISTRO | PCB: LAM STAGE 2 PCB REV 1.0 (T-Works)     ");
    Serial.println("  FLOW TRANSMITTER FT-01 DAQ & CALIBRATION ENGINE READY      ");
    Serial.println("============================================================");
    Serial.printf("[INIT] CAL_A = %.6f, CAL_B = %.6f | Window = %.2fs\n", CAL_A, CAL_B, windowNominalSec);
    Serial.println("[INIT] Frame Format: /*time_s,pulses,window_s,freq_Hz,Q_Lmin,Q_mLs,total_L,total_pulses,run_pulses,run_time_s,run_freq_Hz,Q_actual_Lmin,error_pct*/");

    if (AUTO_START_ON_BOOT) {
        Serial.println("[AUTO-START] Physical switch bypassed. Starting in 1.5s...");
        delay(1500);
        masterInterlockArmed = true;
        startCase1Normal();
    } else {
        Serial.println("[STANDBY] Master Safety Interlock: LOCKED (OFF).");
        Serial.println("[STANDBY] Press physical Pushbutton on the rig or send 'arm' over Serial to START.");
    }
}

// Main execution loop: CLI handling, debouncing, telemetry streaming, and process FSM
void loop() {
    handleSerialCLI();
    if (USE_PHYSICAL_SWITCH) {
        handleTactileButton();
    }
    handleFloatSwitch();
    updateFlowAndTelemetry();
    executeProcessControl();

    newFlowSample = false;

    // Yield CPU to FreeRTOS IDLE task to prevent task watchdog timer timeout
    delay(2);
}
