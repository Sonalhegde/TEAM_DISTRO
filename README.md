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

## 4. Quick Start

### 1. Run the 3D Digital Twin Simulation
- **Live Online:** **[https://sonalhegde.github.io/TEAM_DISTRO/](https://sonalhegde.github.io/TEAM_DISTRO/)**
- **Offline:** Open [`simulation/web/index.html`](simulation/web/index.html) in any modern web browser.

### 2. Run Automated Simulation Tests
Run the headless Node.js verification test suite:
```bash
node tests/simulation/run-tests.js
```
All 16 test cases verify automatically.

### 3. Build & Flash Production Controller Firmware
The controller firmware is located in [`controller_firmware/`](controller_firmware/).

**1-Click Automated Batch Scripts (Windows):**
```cmd
.\esp_build.bat
.\esp_flash.bat
```

**Using ESP-IDF Native CLI:**
```bash
cd controller_firmware
idf.py set-target esp32
idf.py build
idf.py -p COM4 flash monitor
```

### 4. Hardware Verification & Testing Procedure
Follow the step-by-step testing manual:
- **Testing Manual:** [`TESTING_PROCEDURE.md`](TESTING_PROCEDURE.md)
- **Comprehensive Guide:** [`docs/COMPLETE_TESTING_PROCEDURE.md`](docs/COMPLETE_TESTING_PROCEDURE.md)

### 5. Run Telemetry & Serial Studio DAQ
- **Serial Studio:** Open [`telemetry/serial_studio_project.json`](telemetry/serial_studio_project.json) in Serial Studio at `115200` baud.
- **Python Logger:**
  ```bash
  pip install -r telemetry/requirements.txt
  python telemetry/data_logger.py --port COM3
  ```

---

## 5. Repository Structure

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
│       ├── stl/                  # Individual STL files
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

## 6. License

Refer to [`LICENSE`](LICENSE) for licensing terms.
