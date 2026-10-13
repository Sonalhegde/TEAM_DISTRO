# Lam Research Challenge 3.0: Stage 2 Testing Procedure Manual
**Team Name:** TEAM DISTRO | **Team ID:** LRC-26-0528  
**Project:** Practical Engineering Challenge (Automated Liquid Handling Rig)  
**Microcontroller:** ESP32 (NodeMCU-32S / ESP32-DevKitC) | **Firmware:** Espressif ESP-IDF Native  

---

## 1. Safety & Pre-Test Physical Checklist

Before powering on or running any pumps, verify the following physical settings:

### A. Fluid Reservoirs (Critical Initial Levels)
* **10 Liter Process Tank (TK-01):** Fill with **5.5 to 6.0 Liters** of clean water.
  * *Important:* Do NOT fill to 10 Liters! Leaving 4.0 to 4.5 Liters of empty space at the top guarantees the tank will never overflow when water returns from the sump.
* **4.2 Liter Sump Tank (TK-02):** Pre-fill with **1.5 Liters** of water.
  * This primes Pump 2 and prevents dry-running at startup.
  * Verify that this starting level is below the float switch.
* **4.2 Liter Flush Tank (TK-03):** Fill with **4.2 Liters** of water (reserved for Case 2 line flushing).
* **External Beaker / Jug:** Keep **1.0 to 1.5 Liters** of water ready on the table for the Case 1 surge disturbance test.

### B. Master Valve Position Reference Table

Set the manual valve handles to these exact percentages depending on which test you are running:

| Test Phase | Ball Valve BV-01 (Return Line) | Ball Valve BV-02 (Flush Line) | Solenoid Valve OV-01 (10L Tank) |
| :--- | :--- | :--- | :--- |
| **Case 1: Part A (Surge Interlock)** | **50% OPEN** (Handle at 45 degrees) | **0% (FULLY CLOSED)** | **100% OPEN** (Controlled by ESP32) |
| **Case 1: Part B (Valve Characterization)** | Test **100%, 75%, 50%, 25%** in steps | **0% (FULLY CLOSED)** | **100% OPEN** (Controlled by ESP32) |
| **Case 2 (Line Flushing & Shutdown)** | **100% OPEN** (Handle parallel to pipe) | **100% OPEN** (Handle parallel to pipe) | **0% (CLOSED)** (Controlled by ESP32) |

*Why these positions?*
* In **Case 1 (Part A)**, keeping BV-01 at **50%** balances the outflow with Pump 1's inflow so the sump does not run dry.
* In **Case 2**, both BV-01 and BV-02 are **100% wide open** so all 3 pumps flush the lines with maximum flow and zero restriction.

### C. Electrical & Communications
* Connect 12V DC power supply to the PCB and BTS7960 motor driver boards.
* Connect the ESP32 to your PC using the USB cable.
* Open your serial terminal or Serial Studio at **115200 baud** (with newline ending enabled).

---

## 2. Pre-Test Health Check (Diagnostics)

In your serial terminal, type:
```text
status
```

**Verify the output:**
* **State:** IDLE
* **Interlock:** LOCKED (SAFE STOP)
* **Sump:** NORMAL (Float switch is hanging down)
* **Flow:** 0.00 L/min
* **Pulses:** 0

If all readings match, your sensors and safety interlocks are healthy and ready.

---

## 3. CASE 1 TESTING PROCEDURE

Case 1 tests closed-loop recirculation, manual valve throttling, and automated sump surge protection.

### Part A: Closed-Loop Recirculation & Sump Surge Interlock

#### Objective:
Prove that Pump 1 feeds the sump, Pump 2 evacuates it back to the 10L tank, and when external fluid floods the sump, Pump 1 instantly stops while Pump 2 evacuates the surcharge at 100% speed.

#### Step-by-Step Execution:
1. **Initiate Case 1:**
   * Method 1 (Physical Button): Press the button on the control panel once.
   * Method 2 (Serial Terminal): Type `arm`, press Enter, then type `case1`, press Enter.
2. **Observe Steady-State Recirculation (0 to 30 Seconds):**
   * Solenoid Valve OV-01 clicks OPEN (12V applied).
   * Pump 1 runs at **50% PWM speed**.
   * Pump 2 runs at **100% PWM speed**.
   * Water circulates: 10L Tank -> Solenoid OV-01 -> Pump 1 -> Sump -> Pump 2 -> Flow Sensor FT-01 -> Valve BV-01 -> 10L Tank.
   * Look at Serial Studio: Flow rate should stabilize between **1.50 and 2.20 Liters per minute**.
   * Sump water level remains steady (because Valve BV-01 at 50% throttles Pump 2).
3. **Inject the Sump Surge Disturbance (The Flood Test):**
   * Take your beaker containing 1.0 to 1.5 Liters of water.
   * Pour the water steadily into the 4.2L Sump Tank.
   * Watch the water level rise and lift the float switch (LS-01) up.
4. **Observe Instant Safety Interlock Action:**
   * The terminal immediately displays:
     ```text
     [INTERLOCK] -> SUMP HIGH SURGE: Pump-01 STOPPED (0%), Pump-02 draining (100%).
     ```
   * **Pump 1 INSTANTLY STOPS (0% speed)** to prevent the sump from overflowing.
   * **Pump 2 CONTINUES RUNNING AT 100% speed** to aggressively pump the excess water back into the 10L tank.
   * The 10L tank absorbs the excess liquid safely because of the empty headroom.
5. **Observe Automatic Recovery:**
   * As Pump 2 drains the surcharge, the sump water level drops.
   * The float switch drops back down to the normal position.
   * The controller prints:
     ```text
     [INTERLOCK] Sump level normal. Auto-restarting Pump-01 @ 50% duty.
     ```
   * **Pump 1 automatically spins back up to 50% speed**.
   * Normal steady-state recirculation resumes with zero human intervention.

---

### Part B: Valve Throttling Characterization (Task 2.1)

#### Objective:
Measure and record the hydraulic flow rate response at four distinct positions of Ball Valve BV-01 (100%, 75%, 50%, and 25% open) to demonstrate the valve characteristic curve.

#### Step-by-Step Execution:
*Keep Case 1 running (Pump 1 at 50%, Pump 2 at 100%). You will manually turn the handle of Ball Valve BV-01 by hand.*

1. **Position 1: 100% Open (Full Bore):**
   * Rotate handle until it is completely parallel with the pipe.
   * Allow flow to stabilize for 10 seconds.
   * Read flow rate on Serial Studio: **Approx. 3.20 Liters per minute** (minimum hydraulic restriction).
2. **Position 2: 75% Open (Slight Throttle):**
   * Turn handle approximately 22 degrees from the open line.
   * Allow flow to stabilize for 10 seconds.
   * Read flow rate on Serial Studio: **Approx. 2.65 Liters per minute**.
3. **Position 3: 50% Open (Half Throttle):**
   * Turn handle to 45 degrees.
   * Allow flow to stabilize for 10 seconds.
   * Read flow rate on Serial Studio: **Approx. 1.85 Liters per minute**.
4. **Position 4: 25% Open (Heavy Throttle):**
   * Turn handle to approximately 67 degrees (nearly closed).
   * Allow flow to stabilize for 10 seconds.
   * Read flow rate on Serial Studio: **Approx. 0.65 Liters per minute** (sharp vena contracta, high backpressure).
5. **Reset Valve:**
   * Return handle to **50% open** for balanced continuous running.

---

## 4. CASE 2 TESTING PROCEDURE

Case 2 tests multi-pump line purging and automated volume exhaustion shutdown.

### Objective:
Isolate the primary 10L tank, flush the entire piping network using the 4.2L flush supply with all three pumps operating simultaneously, and automatically shut down the rig once the flush tank runs dry.

### Pre-Test Configuration:
1. Ensure the 4.2L Flush Tank (TK-03) has **4.2 Liters** of water.
2. Manually **OPEN Ball Valve BV-02** on the flush line so Pump 3 has liquid.
3. Keep the 10L tank at safe capacity.

### Step-by-Step Execution:
1. **Initiate Case 2:**
   * Method 1 (Direct Serial Command): Type `arm`, press Enter, then type `case2`, press Enter.
   * Method 2 (Single-Go Mode): If running the full sequence, the controller auto-advances to Case 2 after Case 1 surge is cleared.
2. **Observe Multi-Pump Flushing (Phase 1 & Phase 2):**
   * **Solenoid Valve OV-01 CLOSES** (LED turns off), safely isolating the 10L process tank.
   * **All three pumps turn ON simultaneously:**
     * Pump 1 runs at **50% speed**.
     * Pump 2 runs at **100% speed**.
     * Pump 3 runs at **80% speed** (feeding fresh water from Tank 3 into the Equal Tee).
   * *Phase 1 (Priming Window, 0 to 25s):* Water travels through Pump 3, Tee, Pump 1, Sump, and Pump 2 to prime the dry lines.
   * *Phase 2 (Active Purging):* As soon as water reaches FT-01, flow rate is detected (greater than 0.30 L/min).
   * The controller prints:
     ```text
     [C2] Flush fluid detected at FT-01 (Flow: 2.10 L/min). Active line conditioning!
     ```
   * Cumulative flush volume begins totalizing toward 4.20 Liters.
3. **Observe Automated Safe Shutdown (Phase 3):**
   * As all three pumps continue running, the 4.2L Flush Tank empties (takes approximately 90 to 120 seconds).
   * When Tank 3 runs dry, Pump 3 sucks air, and the flow through FT-01 collapses to zero.
   * After zero flow persists for 4 continuous seconds (or when cumulative volume reaches 4.20 Liters):
     ```text
     [C2] Flush supply exhausted (zero flow detected, Total Flushed: 4.180 L). All pumps OFF.
     [STATE] -> IDLE (Case 2 Flush sequence complete. System safely OFF.)
     ```
   * All three pumps turn OFF immediately.
   * After 500 milliseconds (anti-water-hammer delay), Solenoid OV-01 is verified closed.
   * The system automatically disarms and enters **SAFE OFF / IDLE**.

### Special Dry Bench Testing Mode for Case 2:
If you are testing the rig without water or want a timed bench demonstration:
* Type `arm`, press Enter, then type:
  ```text
  case2 30
  ```
* All three pumps will run simultaneously for **exactly 30 seconds**, then safely shut down automatically without tripping dry-run faults.

---

## 5. EMERGENCY STOP & MASTER SAFETY INTERLOCK TEST

The physical pushbutton on the control panel acts as an industrial Master Safety Interlock:

### A. Local Pushbutton E-Stop Test:
1. While any case is running (water flowing, pumps spinning), **press the physical pushbutton once**.
2. **Result:**
   * All three pumps immediately cut to 0% PWM and enable pins pull LOW.
   * Solenoid Valve OV-01 closes.
   * The system outputs:
     ```text
     [SWITCH] MASTER STOP TRIPPED: Operator pressed switch.
     All pumps CUT OFF. Solenoid CLOSED. System in SAFE STOP.
     ```
   * The rig enters **LOCKED (SAFE STOP)** mode.

### B. Remote Serial E-Stop:
* Sending `stop`, `s`, `x`, or `0` in the serial terminal at any millisecond immediately cuts all power and forces the rig into Safe Standby.

---

## 6. QUICK COMMAND REFERENCE TABLE

| Command | Allowed in IDLE? | Description |
| :--- | :---: | :--- |
| `status` | YES | Prints real-time sensor states, pin pulse counts, float alarm, and flow totalizer. |
| `arm` | YES | Pre-arms the safety interlock to allow remote case starts from PC. |
| `disarm` | YES | Trips interlock into Safe Locked mode. |
| `case1` or `1` | Needs Arm/Button | Starts Case 1 Closed-Loop Recirculation (P1=50%, P2=100%, OV-01 Open). |
| `case2` or `2` | Needs Arm/Button | Starts Case 2 Line Flushing (OV-01 Closed, P1=50%, P2=100%, P3=80% until 4.2L empty). |
| `case2 <sec>` | Needs Arm/Button | Runs Case 2 for a fixed timed duration (e.g., `case2 30` for 30s dry test). |
| `auto` or `a` | Needs Arm/Button | Runs Single-Go automated sequence (Case 1 -> Surge demonstration -> Case 2 -> Stop). |
| `stop` or `0` | **ALWAYS** | Master Emergency Stop (instantly cuts all pumps and closes valve). |
| `rt` | YES | Resets volume totalizers back to 0.000 Liters. |
| `cal0` | YES | Zeroes pulse baseline for K-factor calibration. |
| `cal <liters>` | YES | Automatically calculates new K-factor after dispensing a measured volume. |
| `ppl <val>` | YES | Manually updates flow sensor calibration factor (pulses per Liter). |
| `?` | YES | Displays the live serial command help menu. |
