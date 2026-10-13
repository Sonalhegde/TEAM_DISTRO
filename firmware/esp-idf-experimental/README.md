# ESP32 Process Controller & Serial Studio DAQ (Espressif Format)

**Project:** Lam Research Challenge 3.0 (Stage 2: Practical Engineering Challenge)  
**Team:** TEAM DISTRO (LRC-26-0528)  
**Microcontroller:** ESP32 (NodeMCU-32S / ESP32-DevKitC)  
**PCB Schematic:** LAM Stage 2 PCB Rev 1.0  
**Format:** Native Espressif C++ / ESP-IDF & PlatformIO  

---

## 1. Overview & Project Layout

This controller has been converted from the legacy `.ino` sketch to standard Espressif format (`CMakeLists.txt`, `main/main.cpp`, and `platformio.ini`). It executes an asynchronous, non-blocking finite state machine (zero `delay()`) providing closed-loop PI flow control, dual-mode operation (Case 1 & Case 2), safety fault interlocks, and live telemetry streaming for **Serial Studio Data Acquisition (DAQ)**.

### Directory Structure
```
unverified/
├── CMakeLists.txt         # Root ESP-IDF CMakeLists
├── main/
│   ├── CMakeLists.txt     # Component CMakeLists
│   └── main.cpp           # Native C++ application & DAQ engine
├── platformio.ini         # PlatformIO build configuration
└── README.md              # Hardware & operations guide
```

---

## 2. Operational Modes

### Case 1: Closed-Loop Recirculation & Sump Level Regulation
- **Trigger:** Sump float switch trips `HIGH` (`PIN_FSW` / GPIO35) or manual command `s1`.
- **Action:**
  - Solenoid valve (`PIN_SV` / GPIO27) opens immediately.
  - Feed Pump 1 starts instantly at 70% PWM duty.
  - PI velocity control loop adjusts PWM duty (clamped 30% - 100%) to maintain exactly **3.0 L/min** via YF-S401 feedback.
  - Pulse counter continuously integrates totalized discharge volume (`totalL`).
- **Shutdown:** When the float drops `LOW` (empty sump), the pump stops instantly and the solenoid valve closes.
- **Safety Interlock:** If the pump runs for >4s (`C1_GRACE_MS`) and measured flow is <0.3 L/min for >6s (`C1_NOFLOW_FAULT_MS`), a `FAULT` state trips to prevent dry-running or line over-pressurization.

### Case 2: Multi-Pump Rig Flush & Automated Safe Shutdown
- **Trigger:** Manual long-press on tactile switch (>= 1.5s) or serial command `s2`.
- **Action:**
  - Solenoid valve opens.
  - All three pumps (P1, P2, P3) run concurrently at 80% duty.
- **Shutdown:** When the flush reservoir empties and flow drops below 0.3 L/min for 3 seconds (`C2_EXHAUST_MS`), all pumps de-energize. 500 ms later, the solenoid valve closes to eliminate hydraulic water hammer, and the rig transitions to `IDLE`.
- **Timeout:** Global safety timeout of 10 minutes (`C2_MAX_RUN_MS`).

---

## 3. Hardware Pin Connections (PCB Rev 1.0)

| Function | ESP32 GPIO | Connected Component | Description |
|---|---|---|---|
| **SW** | GPIO34 | ON/OFF Tactile Switch | Input-only, 10k external pull-up on PCB, Active LOW |
| **FSW** | GPIO35 | Sump Float Switch | Input-only, 10k external pull-up on PCB |
| **FS** | GPIO14 | YF-S401 Flow Sensor | Interrupt attached (`RISING`), Internal pull-up |
| **SV** | GPIO27 | Solenoid Valve Driver | Low-side MOSFET gate driver PWM/IN |
| **LED** | GPIO2 | Status LED | Onboard Blue LED (indicates state via blink patterns) |
| **Pump 1 L_EN / R_EN** | GPIO13 / GPIO26 | BTS7960 Driver #1 | Half-bridge enable lines |
| **Pump 1 L_PWM / R_PWM** | GPIO32 / GPIO33 | BTS7960 Driver #1 | PWM drive lines (20 kHz, 8-bit) |
| **Pump 2 L_EN / R_EN** | GPIO16 / GPIO17 | BTS7960 Driver #2 | Half-bridge enable lines |
| **Pump 2 L_PWM / R_PWM** | GPIO18 / GPIO19 | BTS7960 Driver #2 | PWM drive lines (20 kHz, 8-bit) |
| **Pump 3 L_EN / R_EN** | GPIO21 / GPIO22 | BTS7960 Driver #3 | Half-bridge enable lines |
| **Pump 3 L_PWM / R_PWM** | GPIO23 / GPIO25 | BTS7960 Driver #3 | PWM drive lines (20 kHz, 8-bit) |

---

## 4. Serial Studio DAQ Telemetry Integration

The firmware streams telemetry frames every 500 ms matching the project definition in [`telemetry/serial_studio_project.json`](file:///d:/TEAM_DISTRO/telemetry/serial_studio_project.json):

```
/*<Time_ms>,<State_ID>,<Freq_Hz>,<Flow_Lpm>,<Flow_Mls>,<Total_L>,<Sump_Alarm>*/\r\n
```

### Dataset Index Mapping:
1. **Time Since Boot:** Milliseconds elapsed since ESP32 startup (`ms`).
2. **System State ID:** Integer code (`0`=IDLE, `1`=C1_STANDBY, `2`=C1_PUMPING, `3`=C2_FLUSH, `4`=C2_STOPPING, `5`=FAULT).
3. **Pulse Frequency:** Hall sensor pulse rate (`Hz`).
4. **Flow Rate (L/min):** Instantaneous flow rate (`0.0 - 6.0 L/min`).
5. **Flow Rate (mL/s):** Instantaneous flow rate (`0.0 - 100.0 mL/s`).
6. **Total Transferred Volume:** Cumulative volume (`0.0 - 10.0 Liters`).
7. **Sump High Level Alarm:** Digital status (`0` = OK, `1` = Level breach / high surcharge).

### 4.1 Step-by-Step Instructions: Importing JSON into Serial Studio

Follow these steps to connect your ESP32 to Serial Studio and visualize real-time hydraulics:

1. **Install Serial Studio:**
   - Download the latest release from [Serial Studio GitHub Releases](https://github.com/Serial-Studio/Serial-Studio/releases) (or install via installer / portable `.exe`).
2. **Launch Serial Studio:**
   - Run Serial Studio on your workstation.
3. **Import Project Configuration JSON:**
   - In Serial Studio, click the **Project** / **Setup** menu (or click **Open Project** / press `Ctrl+O`).
   - Navigate to the repository and select:
     ```
     d:\TEAM_DISTRO\telemetry\serial_studio_project.json
     ```
     *(Relative path from `unverified/`: `../telemetry/serial_studio_project.json`)*
   - Serial Studio will automatically load the project title **"Lam Stage 2 Hydraulic Process Controller"** along with configured widgets:
     - **Flow Rate (L/min)**: Circular dial gauge (0 - 6 L/min).
     - **Flow Rate (mL/s)**: Time-series live line chart.
     - **Total Transferred Volume**: Linear bar progress gauge (0 - 10 L).
     - **Pulse Frequency**: Real-time frequency graph (Hz).
     - **Sump High Level Alarm**: Digital alarm status LED.
     - **State ID**: Controller finite-state indicator.
4. **Configure Serial Port Connection:**
   - Click on the **Connection Settings** / **Serial** tab on the left sidebar:
     - **Port:** Select your ESP32 COM port (e.g. `COM3`, `COM4`, or `/dev/ttyUSB0`).
     - **Baud Rate:** `115200`.
     - **Data Bits:** `8`.
     - **Parity:** `None`.
     - **Stop Bits:** `1`.
     - **Flow Control:** `None`.
5. **Start Data Acquisition:**
   - Click the **Connect** (Play / Connect) button in the top toolbar.
   - Switch to the **Dashboard** view to monitor live analog dials, plots, and status LEDs.
   - Use the **Console** view to inspect raw telemetry frames (`/*...*/`) or send CLI commands (`s1`, `s2`, `x`, `sp 3.5`, etc.).

---

## 5. Serial CLI Commands (115200 Baud)

| Command | Action |
|---|---|
| `s1` | Arm Case 1 (Closed-Loop Level Regulation) |
| `s2` | Start Case 2 (Multi-Pump Line Purge) |
| `x` | Emergency Stop (All actuators OFF immediately) |
| `sp <L/min>` | Set target flow rate setpoint (e.g. `sp 3.5`) |
| `ppl <pulses>` | Update flow sensor pulses per litre calibration factor |
| `cal0` | Zero pulse counter for volumetric calibration |
| `cal <litres>` | Compute new `ppl` calibration factor after dispensing known volume |
| `rt` | Reset discharged volume totalizer to 0.000 L |
| `status` | Print human-readable hardware telemetry status |
| `?` | Print command help menu |

---

## 6. How to Build & Flash

### Option A: Using PlatformIO CLI
```powershell
pio run -d d:\TEAM_DISTRO\unverified --target upload
pio device monitor -d d:\TEAM_DISTRO\unverified -b 115200
```

### Option B: Using ESP-IDF CMake
```bash
cd unverified
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```
