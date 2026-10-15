// Firmware Configuration Header: System Hardware Pinout, Constants & Timings
#pragma once

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// 1. PHYSICAL GPIO PINOUT (LAM STAGE 2 PCB REV 1.0)
// ============================================================================
// Schematics: T-Works Foundation, Drawn by Syed Tajammul Hassan
// Microcontroller: ESP32 (NodeMCU-32S / ESP32-DevKitC)

// User & Safety Sensor Inputs
static const int PIN_SW_GPIO           = 34; // Master SPST / Tactile Switch (Input-only, 10 kOhm ext pull-up, Active LOW)
static const int PIN_FSW_GPIO          = 35; // Sump Float Switch LS-01 (Input-only, 10 kOhm ext pull-up, Active LOW)
static const int PIN_FS_GPIO           = 14; // Flow Transmitter FT-01 pulse input (Hardware PCNT unit)
static const int PIN_SV_GPIO           = 27; // Solenoid Valve OV-01 Driver (Active LOW open / high-Z close)

// BTS7960 High-Power H-Bridge Motor Drivers (U2, U3, U4)
struct PumpPinAssignment {
    uint8_t pin_l_en;      // Half-bridge Left Enable
    uint8_t pin_r_en;      // Half-bridge Right Enable
    uint8_t pin_l_pwm;     // Half-bridge Forward PWM
    uint8_t pin_r_pwm;     // Half-bridge Reverse PWM
    uint8_t pin_l_en_alt;  // Tied secondary enable (GPIO 12 on Pump 1, 0 if unused)
};

// Index 0: Pump-01 (10 L Tank -> Sump Feed)
// Index 1: Pump-02 (Sump -> 10 L Tank Return)
// Index 2: Pump-03 (4.2 L Flush Tank -> Rig Purge)
static const PumpPinAssignment PUMP_PINS[3] = {
    {13, 26, 32, 33, 12}, // Pump 1 (U2): L_EN=13, R_EN=26, L_PWM=32, R_PWM=33, L_EN_ALT=12 (PCB ties 12 & 13)
    {16, 17, 18, 19, 0},  // Pump 2 (U3): L_EN=16, R_EN=17, L_PWM=18, R_PWM=19, no alt pin
    {21, 22, 23, 25, 0}   // Pump 3 (U4): L_EN=21, R_EN=22, L_PWM=23, R_PWM=25, no alt pin
};

// Sump float switch LS-01 polarity: reed switch closes to GND on high water level
static const bool FLOAT_ACTIVE_LEVEL_LOW = true;

// ============================================================================
// 2. PWM ACTUATOR SETTINGS & OPERATING DUTIES
// ============================================================================
static const uint32_t PWM_FREQUENCY_HZ      = 5000;   // 5 kHz optocoupler carrier frequency
static const uint8_t  PWM_RESOLUTION_BITS   = 8;      // 8-bit duty cycle resolution (0 - 255)

// Case 1 Operating Duties
static const float    PUMP1_C1_DUTY_PCT     = 50.0f;  // 50% PWM Duty Cycle (Feed Pump)
static const float    PUMP2_C1_DUTY_PCT     = 100.0f; // 100% PWM Duty Cycle (Discharge Pump)

// Case 2 Flush Operating Duties
static const float    PUMP1_C2_DUTY_PCT     = 50.0f;  // 50% PWM Duty Cycle
static const float    PUMP2_C2_DUTY_PCT     = 100.0f; // 100% PWM Duty Cycle
static const float    PUMP3_C2_DUTY_PCT     = 100.0f; // 100% PWM Duty Cycle (DI Flush Water Feed)

// Actuator Defaults
static const bool     DEFAULT_SOV_ACTIVE_HIGH = false; // Active LOW: pulling pin LOW energizes solenoid relay
static const bool     SOV_HIGH_BY_FLOAT       = true;  // Releasing pin to high-Z lets relay board pull-up de-energize coil
static const bool     USE_PHYSICAL_SWITCH     = true;  // Enable tactile button polling
static const bool     AUTO_START_ON_BOOT      = false; // Require physical button press or serial command to start

// ============================================================================
// 3. FLOW SENSOR (FT-01 / YF-S401) CALIBRATION CONSTANTS
// ============================================================================
// Linear calibration model: Q (L/min) = CAL_SLOPE_A * f (Hz) + CAL_INTERCEPT_B
// Derived from 6 physical catch-and-weigh trials (0.775 to 2.040 L/min operating envelope, R^2 = 0.99668)
static const double   DEFAULT_CAL_SLOPE_A_LPM_PER_HZ  = 0.009244; // Slope coefficient (L/min per Hz)
static const double   DEFAULT_CAL_INTERCEPT_B_LPM     = 0.040111; // Zero-flow intercept offset (L/min)
static const double   DEFAULT_WINDOW_NOMINAL_SEC      = 1.0;      // Telemetry integration window duration (seconds)

// Hydrodynamic Thresholds
static const float    FLOW_CUTOFF_HZ                  = 1.0f;     // Low-frequency threshold to reject phantom vibration
static const float    FLOW_MIN_VALID_LPM              = 0.50f;    // Minimum fluid velocity for valid priming verification
static const float    C2_FLOW_COLLAPSE_LPM            = 0.35f;    // Flow collapse threshold indicating cavitation/air suction
static const float    FLUSH_VOLUME_CEILING_L          = 4.15f;    // Physical 4.2 L flush reservoir limit minus line holdup
static const float    C2_MIN_FLUSH_ARM_L              = 0.30f;    // Minimum delivered volume before collapse trip arms

// Hardware Pulse Counter (PCNT) Unit Limits
static const int      PCNT_LOWER_LIMIT_PULSES         = -32768;   // Counter wrap minimum boundary
static const int      PCNT_UPPER_LIMIT_PULSES         = 32767;    // Counter wrap maximum boundary (wrap length = 32768)

// ============================================================================
// 4. TIMING INTERVALS, DEBOUNCE FILTERS & SAFETY TIME-OUTS
// ============================================================================
// Debounce and Button Durations
static const uint32_t SWITCH_DEBOUNCE_MS              = 30;       // Pushbutton mechanical debounce
static const uint32_t SWITCH_LONG_PRESS_MS            = 1500;     // Emergency stop hold duration
static const uint32_t FLOAT_DEBOUNCE_MS               = 250;      // Sump surface ripple filter

// Process Safety Timeouts
static const uint32_t C1_STARTUP_GRACE_MS             = 4000;     // Initial dry-run suppression window
static const uint32_t C1_NOFLOW_FAULT_TRIP_MS         = 6000;     // Maximum duration without flow before dry-run trip
static const uint32_t C1_RECOVERY_DWELL_MS            = 20000;    // Recirculation stabilization dwell after surge clears
static const uint32_t C1_DRAIN_CONFIRM_EMPTY_MS       = 3000;     // Zero-flow persistence indicating empty sump
static const uint32_t C1_DRAIN_SAFETY_TIMEOUT_MS      = 45000;    // Maximum time permitted for sump evacuation
static const uint32_t SURGE_CLEAR_CONFIRMATION_MS     = 3000;     // Required float clear stability before pump restart

// Case 2 Flush Delays and Ceilings
static const uint32_t C2_PRIME_WINDOW_MS              = 25000;    // Maximum time allowed for flush fluid to reach FT-01
static const uint32_t C2_STARTUP_GRACE_MS             = 10000;    // Flush pump ramp-up grace period
static const uint32_t C2_EXHAUST_PERSISTENCE_MS       = 1000;     // Cavitation persistence to confirm empty flush tank
static const uint32_t C2_AUTO_MAX_RUN_MS              = 90000;    // Case 2 automatic duration ceiling (90 seconds)
static const uint32_t C2_GLOBAL_SAFETY_TIMEOUT_MS     = 600000;   // Hard cutoff safety timeout (10 minutes)
static const uint32_t SOV_STAGGERED_CLOSE_DELAY_MS    = 500;      // Anti-water-hammer delay (solenoid closes after pumps)
static const uint32_t SOV_PRE_OPEN_LEAD_MS            = 200;      // Valve opens before pump start to prime suction line

// Calibration Mode Guards
static const uint32_t CALMODE_IDLE_TIMEOUT_MS         = 120000;   // Maximum idle command timeout in calibration mode
static const uint32_t CALMODE_PUMP2_NOFLOW_STOP_MS    = 5000;     // Pump-2 dry-run cutoff during calibration
static const float    CALMODE_NOFLOW_THRESHOLD_LPM    = 0.20f;    // Zero-flow detection threshold in calibration mode

// ============================================================================
// 5. FINITE STATE MACHINE (FSM) STATES
// ============================================================================
enum SystemState {
    ST_IDLE             = 0, // Standby state, all actuators de-energized
    ST_CASE1_NORMAL     = 1, // Closed-loop recirculation (P1 50%, P2 100%, OV-01 OPEN)
    ST_CASE1_SURGE      = 2, // Sump high-level surge interlock (P1 0%, P2 100%, OV-01 OPEN)
    ST_CASE2_FLUSHING   = 3, // Multi-pump line flush (P1 50%, P2 100%, P3 100%, OV-01 CLOSED)
    ST_CASE2_STOPPING   = 4, // Flush line post-run staggered shutdown (pumps OFF, delay before OV-01 close)
    ST_FAULT            = 5, // Safety trip state (dry-run, priming timeout, or sensor fault)
    ST_CASE1_DRAIN      = 6, // Sump evacuation prior to line flush (P1 0%, P2 100%, OV-01 CLOSED)
    ST_CALMODE          = 7  // Bench calibration DAQ mode (interactive CLI, guards armed)
};

// ============================================================================
// 6. TELEMETRY FRAME SPECIFICATION
// ============================================================================
// The controller emits a CSV telemetry frame wrapped in '/*' and '*/' once per sample window:
// /*time_s,pulses,window_s,freq_Hz,Q_Lmin,Q_mLs,total_L,total_pulses,run_pulses,run_time_s,run_freq_Hz,Q_actual_Lmin,error_pct*/
//
// Field 1:  time_s         - System uptime timestamp (s)
// Field 2:  pulses         - Pulse count increment across the current sample window
// Field 3:  window_s       - Actual elapsed duration of sample window (s)
// Field 4:  freq_Hz        - Instantaneous pulse frequency: pulses / window_s (Hz)
// Field 5:  Q_Lmin         - Calibrated volumetric flow rate: CAL_A * freq_Hz + CAL_B (L/min)
// Field 6:  Q_mLs          - Calibrated volumetric flow rate in mL/s: Q_Lmin * (1000 / 60)
// Field 7:  total_L        - Cumulative volume integrated over active run (L)
// Field 8:  total_pulses   - Cumulative total hardware pulses counted since boot
// Field 9:  run_pulses     - Pulses accumulated during active calibration run ('S' to 'E')
// Field 10: run_time_s     - Elapsed time of active calibration run (s)
// Field 11: run_freq_Hz    - Mean pulse frequency of active calibration run (Hz)
// Field 12: Q_actual_Lmin  - Actual reference flow rate derived from gravimetric volume (L/min)
// Field 13: error_pct      - Relative calibration fit error percentage (%)
