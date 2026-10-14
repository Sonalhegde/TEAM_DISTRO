# Lam Research Challenge 3.0: Stage 2 Practical Engineering Challenge

<div align="center">

![Team ID](https://img.shields.io/badge/Team%20ID-LRC--26--0528-blue?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Complete-success?style=for-the-badge)
[![Live 3D Simulation](https://img.shields.io/badge/Live%20Simulation-3D%20Digital%20Twin-brightgreen?style=for-the-badge&logo=three.js)](https://sonalhegde.github.io/TEAM_DISTRO/)
![Controller](https://img.shields.io/badge/Controller-ESP32%20NodeMCU-red?style=for-the-badge&logo=espressif)
![Firmware](https://img.shields.io/badge/Firmware-Native%20ESP--IDF%20C%2B%2B-teal?style=for-the-badge&logo=espressif)
![Serial Studio](https://img.shields.io/badge/Dashboard-Serial%20Studio-orange?style=for-the-badge)

### **Team DISTRO**
**Industrial Fluid Handling Automation: Liquid-Handling and Recirculation Rig**

👉 **[Launch Live 3D Digital Twin Simulation](https://sonalhegde.github.io/TEAM_DISTRO/)** 👈

</div>

---

## 1. Project Overview & Operating Cases

This repository contains the embedded firmware, 3D digital twin simulation, MATLAB analysis, and documentation for the **Lam Research Challenge 3.0 (Stage 2)** liquid handling and recirculation rig (Team ID: `LRC-26-0528`).

### Case 1 and Case 2 Behavior Summary (5 Lines)
1. **Case 1 Recirculation:** Solenoid valve OV-01 opens; Pump 1 feeds at 50% duty and Pump 2 returns at 100% duty, stabilizing closed-loop flow at 3.0 L/min.
2. **Case 1 Sump Surge:** When float switch LS-01 trips high from external liquid, Pump 1 stops immediately while Pump 2 drains the sump at 100% duty.
3. **Case 1 Auto-Recovery:** Once the sump clears for 3.0 seconds, Pump 1 automatically restarts at 50% duty and recirculation resumes safely.
4. **Case 2 Flush Sequence:** Solenoid OV-01 closes to isolate the 10 L process reservoir; Pumps 1, 2, and 3 run concurrently from the 4.2 L flush reservoir.
5. **Case 2 Safe Auto-Shutdown:** Flow sensor FT-01 monitors line depletion; all pumps cut off automatically when the 4.2 L flush finishes or flow collapses.

---

## 2. Team Members and Project Roles

- **Institution:** Nitte (Deemed to be University) - NMAM Institute of Technology
- **Team Identifier:** LRC-26-0528 | **Team Name:** Team DISTRO

| Name | Role | Department | USN | Email | GitHub | Contribution |
|---|---|---|---|---|---|---|
| **Sonal Hegde** | Team Leader | Department of ECE | NNM24AC050 | nnm24ac050@nmamit.in | [@Sonalhegde](https://github.com/Sonalhegde) | Coding and Documentation |
| **Pratham Amin** | Member | Department of Mechanical Engineering | NNM24ME084 | nnm24me084@nmamit.in | [@pratham885](https://github.com/pratham885) | Piping, 3D Printing, CAD, and Calibration |
| **Jostan Saldanha** | Member | Department of ECE | NNM25AC502 | nnm25ac502@nmamit.in | [@jostan-saldanha](https://github.com/jostan-saldanha) | Electrical and Coding |
| **Mohammed Araan** | Member | Department of CSE | NNM24CS142 | nnm24cs142@nmamit.in | [@Araan-Sheikh](https://github.com/Araan-Sheikh) | Video Editing and 3D Modelling |

---

## 3. Repository Folder Map

```text
TEAM_DISTRO/
├── README.md                 Entry point, operating cases summary, folder map, quick start
├── TEAM.md                   Team members, roles, and institutional details
├── CONTRIBUTING.md           Branching workflow, commit guidelines, and PR rules
├── CHANGELOG.md              Version history and release notes
├── LICENSE                   Project licensing terms
├── .gitignore                Excludes build artifacts, sdkconfig, logs, and caches
├── firmware/                 ESP-IDF native controller project
│   ├── CMakeLists.txt        Root firmware CMake file
│   ├── sdkconfig.defaults    Default ESP32 configuration settings
│   ├── README.md             Firmware architecture and CLI documentation
│   ├── main/
│   │   ├── CMakeLists.txt    Component CMake configuration
│   │   ├── idf_component.yml ESP-IDF component manifest (arduino-esp32)
│   │   ├── config.h          Centralized GPIO pinout, calibration constants, and timings
│   │   └── main.cpp          State machine, PCNT pulse reader, and CLI handler
│   └── tools/                Flash and build verification utilities
├── analysis/                 MATLAB calibration scripts and telemetry tools
│   ├── README.md             Analysis overview and execution instructions
│   ├── ft01_calibration_plots.m  MATLAB script for FT-01 linear regression (Q = a*f + b)
│   ├── valve_position_plot.m     MATLAB script for ball valve throttling curve
│   ├── data/                 Sample CSV datasets (calibration trials, valve characterization)
│   ├── data_logger.py        Python serial frame data logger
│   ├── calibration_tool.py   Python regression solver
│   └── requirements.txt      Python dependencies
├── dashboard/                Serial Studio visualization
│   ├── README.md             Dashboard configuration and frame structure
│   └── serial_studio_project.json Serial Studio UI dashboard project
├── docs/                     Engineering reports and setup guides
│   ├── README.md             Documentation catalog index
│   ├── SETUP_REPO.md         Git cloning, branching, and conflict resolution guide
│   ├── SETUP_ESP32.md        ESP-IDF, VS Code, drivers, and flashing guide
│   ├── SETUP_ANALYSIS.md     MATLAB and Python data processing guide
│   ├── technical-report.md   Stage 2 Technical Engineering Report
│   ├── pid-design-specification.md P&ID piping and instrumentation report
│   ├── hardware-wiring-guide.md    Schematics and PCB pin connections
│   ├── fault-analysis-fmea.md      Failure Modes and Effects Analysis
│   ├── calibration-and-analysis.md Detailed sensor calibration mathematics
│   └── diagrams/             SVG schematics, P&ID diagrams, and state machines
├── media/                    Generated plots, figures, and system photographs
│   ├── sensor_calibration_curve.png
│   ├── valve_characteristic_curve.png
│   └── flow_vs_time_sample.png
├── hardware/                 Mechanical models and 3D print assets
│   └── 3d-print/             STL files, slice parameters, and CAD renders
├── simulation/               3D digital twin package
│   ├── README.md             Simulation user guide and control logic
│   ├── web/                  HTML5 / WebGL interactive digital twin
│   ├── vendor/               Three.js 3D rendering library
│   └── archive/              Archived preliminary simulation
└── tests/                    Automated and hardware verification suites
    ├── README.md             Test index and instructions
    ├── simulation/           Node.js headless verification suite (16/16 tests)
    │   ├── run-tests.js      Test runner
    │   └── tests.js          TC-01 through TC-16 verification assertions
    └── firmware-bench/       Standalone component diagnostic firmware
```

---

## 4. Quick Start

### 1. Build and Flash Firmware (ESP-IDF)
For complete instructions, refer to [`docs/SETUP_ESP32.md`](docs/SETUP_ESP32.md).
```bash
# Using 1-click Windows scripts:
.\esp_build.bat
.\esp_flash.bat

# Or using native ESP-IDF CLI:
cd firmware
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```

### 2. Launch Serial Studio Dashboard
1. Open [Serial Studio](https://serial-studio.github.io/).
2. Load project file: [`dashboard/serial_studio_project.json`](dashboard/serial_studio_project.json).
3. Select your ESP32 serial port (`COMx`), set baud rate to `115200`, and click **Connect**.

### 3. Run Automated Verification Tests
```bash
node tests/simulation/run-tests.js
```
*(All 16 unit tests verify state transitions, surge mitigation, calibration models, and auto-shutdown).*

### 4. Run MATLAB Data Analysis
Refer to [`docs/SETUP_ANALYSIS.md`](docs/SETUP_ANALYSIS.md) for full instructions:
```matlab
cd('analysis');
ft01_calibration_plots;
valve_position_plot;
```

---

## 5. Serial CLI Reference

Send commands over the 115200 baud serial connection:
- `auto` / `a`: Runs Single-Go automated challenge sequence (Case 1 $\rightarrow$ surge drain $\rightarrow$ Case 2 flush).
- `case1` / `1`: Initiates Case 1 closed-loop recirculation (P1 at 50%, P2 at 100%, OV-01 open).
- `case2` / `2`: Initiates Case 2 line flush (P1, P2, P3 at 100%, OV-01 closed).
- `stop` / `0`: Emergency stop (cuts all pumps and closes solenoid valve).
- `status`: Displays current state, interlock mode, flow rate, total volume, and active calibration constants.
- `FIT`: Computes linear regression across recorded calibration trials.
- `APPLY`: Applies newly computed regression slope and offset to active firmware variables.
- `?`: Prints help menu and command list.

---

## 6. Documentation Index

- [Repository Setup and Git Guide](docs/SETUP_REPO.md)
- [ESP32 Firmware Build and Flash Guide](docs/SETUP_ESP32.md)
- [MATLAB Data Analysis Guide](docs/SETUP_ANALYSIS.md)
- [Stage 2 Technical Engineering Report](docs/technical-report.md)
- [P&ID Design Specification](docs/pid-design-specification.md)
- [Hardware Wiring and Schematic Guide](docs/hardware-wiring-guide.md)
- [Failure Modes and Effects Analysis (FMEA)](docs/fault-analysis-fmea.md)
