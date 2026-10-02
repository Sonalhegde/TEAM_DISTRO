# Unverified Test Controller - Lam Research Challenge 3.0 (Stage 2)

**Status:** Unverified Hardware Bring-Up / Multi-Case Controller  
**Microcontroller:** ESP32 (NodeMCU-32S / ESP32-DevKitC)  
**PCB Schematic:** LAM Stage 2 PCB Rev 1.0  

---

## Overview

This firmware implements an integrated non-blocking state machine for the dual-mode process rig without using `delay()`. It manages closed-loop PI flow control for liquid dispensing and an automated multi-pump sequence for post-process flushing.

### Operational Modes

#### Case 1: Control + Measurement (Sump Level & Flow Hold)
- **Trigger:** Sump float switch trips `HIGH` (`PIN_FSW` / GPIO35).
- **Action:**
  - Solenoid valve (`PIN_SV` / GPIO27) opens immediately.
  - Pump 1 (`C1_PUMP_MASK = 0b001`) starts instantly at 70% PWM duty.
  - PI velocity control loop adjusts PWM duty (clamped 30% - 100%) to maintain exactly **3.0 L/min** via YF-S401 Hall-effect feedback.
  - Pulse counter continuously integrates totalized discharge volume (`totalL`).
- **Shutdown:** When the float drops `LOW` (empty sump), the pump stops instantly and the solenoid valve closes.
- **Safety Interlock:** If the pump runs for >4s (`C1_GRACE_MS`) and measured flow is <0.3 L/min for >6s (`C1_NOFLOW_FAULT_MS`), a `FAULT` state trips to prevent dry-running or line over-pressurization.

#### Case 2: Flush + Shutdown
- **Trigger:** Manual long-press on tactile switch or serial command `s2`.
- **Action:**
  - Solenoid valve opens.
  - All three pumps (P1, P2, P3 via `C2_PUMP_MASK = 0b111`) run concurrently at 80% duty.
- **Shutdown:** When the flush reservoir empties and flow drops below 0.3 L/min for 3 seconds (`C2_EXHAUST_MS`), all pumps de-energize. 500 ms later, the solenoid valve closes and the rig transitions to `IDLE`.
- **Timeout:** Global safety timeout of 10 minutes (`C2_MAX_RUN_MS`).

---

## Hardware Pin Connections

| Function | ESP32 GPIO | Connected Component | Notes |
|---|---|---|---|
| **SW** | GPIO34 | ON/OFF Tactile Switch | Input-only, 10k external pull-up on PCB, Active LOW |
| **FSW** | GPIO35 | Sump Float Switch | Input-only, 10k external pull-up on PCB |
| **FS** | GPIO14 | YF-S401 Flow Sensor Pulse | Interrupt attached (`RISING`), Internal pull-up |
| **SV** | GPIO27 | Solenoid Valve Driver | Low-side MOSFET gate driver PWM/IN |
| **LED** | GPIO2 | Status LED | Onboard Blue LED (indicates state via blink patterns) |
| **Pump 1 L_EN / R_EN** | GPIO13 / GPIO26 | BTS7960 Driver #1 | Half-bridge enable lines |
| **Pump 1 L_PWM / R_PWM** | GPIO32 / GPIO33 | BTS7960 Driver #1 | PWM drive lines (20 kHz, 8-bit) |
| **Pump 2 L_EN / R_EN** | GPIO16 / GPIO17 | BTS7960 Driver #2 | Half-bridge enable lines |
| **Pump 2 L_PWM / R_PWM** | GPIO18 / GPIO19 | BTS7960 Driver #2 | PWM drive lines (20 kHz, 8-bit) |
| **Pump 3 L_EN / R_EN** | GPIO21 / GPIO22 | BTS7960 Driver #3 | Half-bridge enable lines |
| **Pump 3 L_PWM / R_PWM** | GPIO23 / GPIO25 | BTS7960 Driver #3 | PWM drive lines (20 kHz, 8-bit) |

---

## Tactile Switch Operations (GPIO34)

- **IDLE State:**
  - **Short Press (< 1.5s):** Arm Case 1 (Auto level & flow control).
  - **Long Press (>= 1.5s):** Start Case 2 (Full rig flush).
- **RUNNING State:**
  - **Any Press:** Emergency stop — shuts down all pumps and closes solenoid valve.
- **FAULT State:**
  - **Any Press:** Reset fault condition and return to `IDLE`.

---

## Serial Console Commands (115200 Baud)

| Command | Action |
|---|---|
| `s1` | Start Case 1 (Control + Measurement) |
| `s2` | Start Case 2 (Flush + Shutdown) |
| `x` | Emergency stop (All OFF) |
| `sp <L/min>` | Set target flow rate (range: 1.0 to 6.0 L/min, e.g. `sp 3.5`) |
| `ppl <pulses>` | Update flow sensor pulses per litre calibration factor |
| `cal0` | Zero pulse counter for calibration run |
| `cal <litres>` | Compute new `ppl` calibration factor after dispensing known volume |
| `rt` | Reset totalizer volume to 0.000 L |
| `?` | Print command help menu |

---

## Status LED Indicators

- **IDLE:** Slow heartbeat blink (80 ms pulse every 1 s).
- **CASE 1 STANDBY:** Solid ON (armed, awaiting high sump level).
- **CASE 1 PUMPING:** Rapid 2 Hz toggle.
- **CASE 2 FLUSH:** 1 Hz toggle.
- **FAULT:** Rapid strobe (50 ms on, 50 ms off).
