# Lam Research Challenge 3.0: Stage 2 Practical Engineering Challenge

<div align="center">

![Team ID](https://img.shields.io/badge/Team%20ID-LRC--26--0528-blue?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Complete-success?style=for-the-badge)
[![Live 3D Simulation](https://img.shields.io/badge/Live%20Simulation-3D%20Digital%20Twin-brightgreen?style=for-the-badge&logo=three.js)](https://sonalhegde.github.io/TEAM_DISTRO/)
![Controller](https://img.shields.io/badge/Controller-ESP32%20NodeMCU-red?style=for-the-badge&logo=espressif)
![Firmware](https://img.shields.io/badge/Firmware-Native%20ESP--IDF%20C%2B%2B-teal?style=for-the-badge&logo=espressif)
![Python DAQ](https://img.shields.io/badge/DAQ-Python%203.13-yellow?style=for-the-badge&logo=python)

### **Team DISTRO**
**Industrial Manufacturing Process Automations: Liquid-Handling and Recirculation Rig**

👉 **[Launch Live 3D Digital Twin Simulation (Click Here)](https://sonalhegde.github.io/TEAM_DISTRO/)** 👈

</div>

---

## 1. Problem Statement

Design, build, commission, and validate an automated small-scale fluid handling platform capable of:
1. **Case 1 (Closed-Loop Recirculation and Level Protection):**
   - Recirculates fluid continuously at a target flow rate of 3.0 L/min (50 mL/s).
   - Monitors sump level with a float switch (`LS-01`).
   - If external fluid enters the sump, the controller stops Pump-01 immediately. Pump-02 continues pumping at 100% duty cycle to return the excess liquid to the main 10 L tank.
   - When the float switch clears, Pump-01 automatically restarts at 50% duty cycle.
2. **Case 2 (Line Flushing and Safe Shutdown):**
   - Energizes the solenoid valve (`OV-01`) to close and isolate the primary 10 L tank.
   - Operates all three pumps (Pump-01, Pump-02, and Pump-03) simultaneously with a 4.2 L flush reservoir to flush the lines.
   - Halts all pumps safely when the flush volume finishes and flow drops to zero.

---

## 2. Team Members and Project Contributions

- **Institution:** Nitte (Deemed to be University) - NMAM Institute of Technology
- **Team Identifier:** LRC-26-0528
- **Team Name:** Team DISTRO

| Name | Role | Department | USN | Email | GitHub | Contribution |
|---|---|---|---|---|---|---|
| **Sonal Hegde** | Team Leader | Department of ECE | NNM24AC050 | nnm24ac050@nmamit.in | [@Sonalhegde](https://github.com/Sonalhegde) | Coding and Documentation |
| **Pratham Amin** | Member | Department of Mechanical Engineering | NNM24ME084 | nnm24me084@nmamit.in | [@pratham885](https://github.com/pratham885) | Piping, 3D Printing, CAD, and Calibration |
| **Jostan Saldanha** | Member | Department of ECE | NNM25AC502 | nnm25ac502@nmamit.in | [@jostan-saldanha](https://github.com/jostan-saldanha) | Electrical and Coding |
| **Mohammed Araan** | Member | Department of CSE | NNM24CS142 | nnm24cs142@nmamit.in | [@Araan-Sheikh](https://github.com/Araan-Sheikh) | Video Editing and 3D Modelling |

---

## 3. Hardware and Sensor Components (LAM Stage 2 PCB Rev 1.0)

| Tag | Component | Function | ESP32 GPIO | Electrical Characteristic |
|---|---|---|:---:|---|
| `U7` | ESP32 NodeMCU-32S | 32-bit dual-core microcontroller running native FreeRTOS state engine | Core Controller | 3V3 / 5V Logic rails |
| `SW` | Master Run Switch | Manual SPST toggle switch / push button | GPIO 34 | Input-only, 10 kΩ pull-up, Active LOW |
| `FSW` / `LS-01` | Sump Float Switch | Detects high liquid level in sump tank to trigger interlock | GPIO 35 | Input-only, 10 kΩ pull-up |
| `FS` / `FT-01` | Flow Sensor (YF-S401) | Micro-turbine pulse frequency sensor ($K \approx 6,155\text{ pulses/L}$) | GPIO 14 | Interrupt attached, internal pull-up |
| `SV` / `OV-01` | Solenoid Valve Driver | Isolates 10 L process reservoir during Case 2 flushing | GPIO 27 | Active HIGH = Open, LOW = Closed |
| `U2` (Pump 1) | BTS7960 Motor Driver #1 | Feeds liquid from 10 L tank into sump (50% PWM nominal) | L_EN: 13, R_EN: 26<br>L_PWM: 32, R_PWM: 33 | 20 kHz PWM, 8-bit |
| `U3` (Pump 2) | BTS7960 Motor Driver #2 | Returns liquid from sump to 10 L process reservoir | L_EN: 16, R_EN: 17<br>L_PWM: 18, R_PWM: 19 | 20 kHz PWM, 8-bit |
| `U4` (Pump 3) | BTS7960 Motor Driver #3 | Delivers DI flush liquid during Case 2 line purge | L_EN: 21, R_EN: 22<br>L_PWM: 23, R_PWM: 25 | 20 kHz PWM, 8-bit |
| `LED` | Status LED | Onboard Blue LED indicator for states and alarms | GPIO 2 | State blink patterns |
| `BV-01, BV-02` | 2-Way Manual Ball Valves | BV-01 throttles return line; BV-02 isolates flush reservoir | Manual Valves | Plumbing network |

Detailed pin assignments are in [`docs/hardware-wiring-guide.md`](docs/hardware-wiring-guide.md).

---

## 4. Quick Start: Build, Flash & Run Firmware (Espressif ESP-IDF)

The production controller firmware is written in native C++ using the Espressif ESP-IDF framework under [`controller_firmware/main/main.cpp`](controller_firmware/main/main.cpp).

### 1. Build & Flash Controller Firmware

#### Option A: 1-Click Automated Batch Scripts (Windows)
From the repository root:
```cmd
.\esp_build.bat
.\esp_flash.bat
```
- `esp_build.bat`: Sets Espressif toolchain paths and builds via `idf.py build`.
- `esp_flash.bat`: Uses [`controller_firmware/tools/esp_quick.py`](controller_firmware/tools/esp_quick.py) to automatically scan COM ports, kill stale monitor locks, flash binaries, and launch serial output.

#### Option B: Native ESP-IDF CLI
```bash
cd controller_firmware
idf.py set-target esp32
idf.py build
idf.py -p COM4 flash monitor
```

---

## 5. Serial CLI Command Reference & Process Control

Open the serial connection at **115200 baud** (newline line endings) using any serial terminal or Serial Studio.

### Process Execution Commands
| Command | Shorthand | Action / Description |
|---|---|---|
| `auto` | `a`, `singlego` | **Single-Go Automated Challenge Run:** Executes Case 1 recirculation, handles sump surge if triggered, evacuates sump, and automatically advances to Case 2 flush. |
| `case1` | `s1`, `c1`, `1` | **Start Case 1 Recirculation:** Solenoid OV-01 opens, Pump 1 runs at 50% duty, Pump 2 runs at 100% duty. |
| `case2` | `s2`, `c2`, `2` | **Start Case 2 Flush:** Solenoid OV-01 closes (isolating 10 L tank), Pumps 1, 2, and 3 run concurrently until 4.2 L flush reservoir finishes or zero flow / air suction is detected. |
| `case2 <sec>` | `c2 30` | **Timed Case 2 Flush:** Runs flush sequence for a designated number of seconds (e.g. `case2 45` for bench tests). |
| `stop` | `0`, `x` | **Emergency Stop:** De-energizes all 3 pumps and closes solenoid valve immediately. |
| `arm` | `unlock` | **Arm Master Safety Interlock:** Remotely enables execution of process commands. |
| `disarm` | `lock` | **Disarm Master Safety Interlock:** Puts rig into safe stop; blocks process start. |
| `status` | — | **System Snapshot:** Displays state, interlock mode, sump level, pulse counts, instantaneous flow rate, and cumulative totalizer. |
| `rt` | — | **Reset Totalizer:** Sets cumulative transferred volume back to 0.000 L. |
| `?` | — | **Help Menu:** Prints complete command reference to serial terminal. |

### Calibration & Bench Diagnostics
| Command | Parameters | Description |
|---|---|---|
| `calmode on` | — | Enters guarded calibration mode (push-button start disabled; automatic safety guards active). |
| `calmode off` | — | Exits calibration mode and restores system to IDLE. |
| `S` | — | **Start Calibration Run:** Starts pulse counting and timer for volumetric calibration point. |
| `E` | — | **End Calibration Run:** Freezes timer and pulse count for calibration run. |
| `V <vol>` | e.g. `V 1.00` | Records measured volume (litres) collected in graduated beaker during run. |
| `FIT` / `CAL` | — | Computes least-squares linear regression ($Q = a \cdot f + b$) and prints trial error table. |
| `APPLY` | — | Applies newly fitted slope $a$ and offset $b$ directly to active RAM calibration coefficients. |
| `cal_a <val>` | e.g. `cal_a 0.009244` | Manually sets calibration slope $a$ (L/min per Hz). |
| `cal_b <val>` | e.g. `cal_b 0.040111` | Manually sets calibration intercept offset $b$ (L/min). |
| `DEMO` | — | Loads 5-point challenge verification dataset and calculates regression fit report. |
| `C` / `CLEAR` | — | Clears all calibration run records and stored trials. |
| `p1 <duty>` | `0.0 - 100.0` | Manually tests Pump 1 PWM duty (e.g. `p1 50`). |
| `p2 <duty>` | `0.0 - 100.0` | Manually tests Pump 2 PWM duty (e.g. `p2 100`). |
| `p3 <duty>` | `0.0 - 100.0` | Manually tests Pump 3 PWM duty (e.g. `p3 80`). |
| `sov <1/0>` | `1` or `0` | Manually drives solenoid valve relay (`1` = Open, `0` = Closed). |
| `sov inv` | — | Toggles solenoid relay driver output logic polarity. |
| `prev` | — | Toggles Pump 1 forward / reverse H-bridge direction. |

---

## 6. Serial Studio DAQ Telemetry Setup

The ESP-IDF controller firmware outputs live telemetry frames every nominal 1.0 second formatted for real-time visualization in **Serial Studio**:

```
/*<time_s>,<pulses>,<window_s>,<freq_Hz>,<Q_Lmin>,<Q_mLs>,<total_L>,<total_pulses>,<run_pulses>,<run_time_s>,<run_freq_Hz>,<Q_actual_Lmin>,<error_pct>*/\r\n
```

### Setup Instructions
1. **Download Serial Studio:** Install the application from [Serial Studio Releases](https://github.com/Serial-Studio/Serial-Studio/releases).
2. **Open Project Configuration:**
   - In Serial Studio, click **Project** -> **Open Project** (`Ctrl+O`).
   - Select [`telemetry/serial_studio_project.json`](telemetry/serial_studio_project.json).
3. **Configure Connection:**
   - Set **Transport Type** to **Serial Port**.
   - Select your ESP32 COM port (e.g. `COM3` or `COM4`).
   - Set **Baud Rate** to `115200`, **Data Bits** `8`, **Parity** `None`, **Stop Bits** `1`.
4. **Connect & Visualize:**
   - Click **Connect** in the top toolbar.
   - Switch to **Dashboard** to view live dials, flow rate time-series plots, volume bars, and sump alarms.
   - Switch to **Console** to send CLI commands (`auto`, `case1`, `case2`, `stop`, `FIT`, `APPLY`).

---

## 7. Python Telemetry Logger & Verification Tests

### 1. Run 3D Digital Twin Simulation
- **Live Online:** **[https://sonalhegde.github.io/TEAM_DISTRO/](https://sonalhegde.github.io/TEAM_DISTRO/)**
- **Offline:** Open [`simulation/web/index.html`](simulation/web/index.html) in any web browser.

### 2. Run Automated Simulation Tests
```bash
node tests/simulation/run-tests.js
```
All 16 test cases verify automatically.

### 3. Run Python Data Logger
```bash
pip install -r telemetry/requirements.txt
python telemetry/data_logger.py --port COM3
```

### 4. Hardware Verification & Testing Manual
- **Testing Procedure:** [`TESTING_PROCEDURE.md`](TESTING_PROCEDURE.md)
- **Detailed Rig Manual:** [`docs/COMPLETE_TESTING_PROCEDURE.md`](docs/COMPLETE_TESTING_PROCEDURE.md)

---

## 8. Repository Structure

```
TEAM_DISTRO/
├── .github/
│   └── workflows/
│       └── pages.yml             # GitHub Pages automated deployment workflow
├── README.md                     # Project overview, problem statement, team, links
├── TEAM.md                       # Team members, roles, departments, contributions
├── TESTING_PROCEDURE.md          # Hardware testing procedure quick link
├── LICENSE                       # Software license terms
├── .gitignore
├── esp_build.bat                 # 1-Click build script for controller firmware
├── esp_flash.bat                 # 1-Click flash and monitor script for controller firmware
├── TEAM_DISTRO.code-workspace    # VS Code unified workspace
├── controller_firmware/          # Production Native ESP-IDF C++ controller firmware
│   ├── CMakeLists.txt            # Root ESP-IDF CMakeLists
│   ├── sdkconfig.defaults        # ESP-IDF configuration defaults
│   ├── dependencies.lock         # ESP-IDF component dependencies lock
│   ├── README.md                 # Firmware architecture, CLI commands, and pinout guide
│   ├── CHANGES.md                # Changes and verification log
│   ├── main/
│   │   ├── CMakeLists.txt        # Component CMakeLists
│   │   ├── idf_component.yml     # ESP-IDF component manager manifest
│   │   └── main.cpp              # Native C++ application & DAQ engine
│   └── tools/
│       ├── esp_quick.py          # Quick build/flash helper tool
│       └── pre_flash.bat         # Pre-flash environment verification
├── docs/                         # System design reports and guides
│   ├── README.md                 # Documentation index
│   ├── technical-report.md       # Engineering report with derivations
│   ├── pid-design-specification.md
│   ├── calibration-and-analysis.md
│   ├── fault-analysis-fmea.md
│   ├── hardware-wiring-guide.md
│   ├── COMPLETE_TESTING_PROCEDURE.md # Full testing procedure manual
│   ├── diagrams/                 # P&ID and electrical schematics
│   └── figures/                  # Plots and calibration figures
├── hardware/
│   └── 3d-print/                 # 3D printable CAD models and specifications
│       ├── README.md             # Component index and slicer settings
│       ├── stl/              # Individual STL files
│       └── images/               # CAD blueprints and 3D visual renders
├── simulation/                   # 3D digital twin package
│   ├── README.md                 # Simulation architecture and user guide
│   ├── web/                      # HTML5/WebGL interactive simulation
│   ├── vendor/                   # Three.js 3D rendering engine
│   └── archive/                  # Earlier simulation build
├── tests/                        # Verification test suites
│   ├── README.md                 # Test catalog and run instructions
│   ├── simulation/               # Headless Node.js test suite (TC-01 to TC-16)
│   └── firmware-bench/           # Physical rig component test sketches & diagnostics
└── telemetry/                    # Python data logging and DAQ tools
    ├── README.md                 # Telemetry usage and setup instructions
    ├── data_logger.py            # Real-time serial data logger
    ├── calibration_tool.py       # Regression solver
    ├── generate_plots.py         # Plot generator
    ├── requirements.txt          # Python dependencies
    └── serial_studio_project.json # Serial Studio dashboard profile
```

---

## 9. License

Refer to [`LICENSE`](LICENSE) for licensing terms.
