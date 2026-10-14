# Firmware Configuration Header: System Hardware Pinout, Constants & Timings
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
static const int PIN_FS_GPIO           = 14; // Flow Transmitter FT-01 pulse input (Interrupt / Hardware PCNT)
static const int PIN_SV_GPIO           = 27; // Solenoid Valve OV-01 Driver (Active LOW open / high-Z close)
static const int PIN_LED_GPIO          = 2;  // Onboard Blue Status LED

// BTS7960 High-Power H-Bridge Motor Drivers (U2, U3, U4)
struct PumpPinAssignment {
    uint8_t pin_l_en;  // Half-bridge Left Enable
    uint8_t pin_r_en;  // Half-bridge Right Enable
    uint8_t pin_l_pwm; // Half-bridge Forward PWM
    uint8_t pin_r_pwm; // Half-bridge Reverse PWM
};

// Index 0: Pump-01 (10 L Tank -> Sump Feed)
// Index 1: Pump-02 (Sump -> 10 L Tank Return)
// Index 2: Pump-03 (4.2 L Flush Tank -> Rig Purge)
static const PumpPinAssignment PUMP_PINS[3] = {
    {13, 26, 32, 33}, // Pump 1 (U2): L_EN=13 (and 12 tied), R_EN=26, L_PWM=32, R_PWM=33
    {16, 17, 18, 19}, // Pump 2 (U3): L_EN=16, R_EN=17, L_PWM=18, R_PWM=19
    {21, 22, 23, 25}  // Pump 3 (U4): L_EN=21, R_EN=22, L_PWM=23, R_PWM=25
};

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

// ============================================================================
// 3. FLOW SENSOR (FT-01 / YF-S401) CALIBRATION CONSTANTS
// ============================================================================
// Linear model: Q (L/min) = CAL_SLOPE_A * f (Hz) + CAL_INTERCEPT_B
// Calibrated from 6 physical rig trials across 0.775 to 2.040 L/min operating envelope
static const double   DEFAULT_CAL_SLOPE_A_LPM_PER_HZ  = 0.009244; // Slope coefficient (L/min per Hz)
static const double   DEFAULT_CAL_INTERCEPT_B_LPM     = 0.040111; // Zero-flow offset (L/min)
static const float    NOMINAL_PULSES_PER_LITRE        = 5880.0f;  // Factory nominal rating (98 Hz * 60 s)

// Hydrodynamic Thresholds
static const float    FLOW_NOISE_GATE_HZ              = 1.0f;     // Low-frequency threshold to reject phantom flow
static const float    FLOW_MIN_VALID_LPM              = 0.50f;    // Minimum fluid velocity for valid flow
static const float    C2_FLOW_COLLAPSE_LPM            = 0.35f;    // Flow collapse threshold indicating line cavitation
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
