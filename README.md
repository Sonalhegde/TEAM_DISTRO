# Lam Research Challenge 3.0 – Stage 2: Practical Engineering Challenge

<div align="center">

![Team ID](https://img.shields.io/badge/Team%20ID-LRC--26--0528-blue?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Complete-success?style=for-the-badge)
[![Live 3D Digital Twin](https://img.shields.io/badge/Live%20Simulation-3D%20Digital%20Twin-brightgreen?style=for-the-badge&logo=three.js)](https://sonalhegde.github.io/TEAM_DISTRO/)
![Controller](https://img.shields.io/badge/Controller-ESP32%20NodeMCU-red?style=for-the-badge&logo=espressif)
![Firmware](https://img.shields.io/badge/Firmware-C%2B%2B%20%2F%20Arduino-teal?style=for-the-badge&logo=arduino)
![Python DAQ](https://img.shields.io/badge/DAQ-Python%203.13-yellow?style=for-the-badge&logo=python)

### **TEAM DISTRO**
**Industrial Manufacturing Process Automations: Liquid-Handling & Recirculation Control System**

👉 **[Launch Interactive 3D Digital Twin Simulation (GitHub Pages)](https://sonalhegde.github.io/TEAM_DISTRO/)** 👈

</div>

---

## 👥 Team Information

- **Team Name:** TEAM DISTRO
- **Team ID:** LRC-26-0528
- **Competition:** Lam Research Challenge 3.0 (Stage 2: Practical Engineering Challenge)
- **Repository:** [https://github.com/Sonalhegde/TEAM_DISTRO.git](https://github.com/Sonalhegde/TEAM_DISTRO.git)
- **Contributors:** Sonal Hegde ([@Sonalhegde](https://github.com/Sonalhegde)) & Team DISTRO

---

## 🎯 Project Overview

This repository contains the complete firmware, hardware documentation, telemetry pipeline, 3D printing fixtures, and digital twin simulation for an automated small-scale fluid handling platform mimicking semiconductor chemical dispense and UPW distribution systems.

### Primary Automation Cases:
1. **Case 1 (Closed-Loop Recirculation & Overflow Protection):**
   - Regulates fluid recirculation at a target flow of **3.0 L/min (50 mL/s)**.
   - Monitors sump level via float switch (`LS-01`). If external fluid is added, Pump-01 instantly cuts off while Pump-02 evacuates the excess fluid back to the main reservoir, auto-resuming once safe level is restored.
2. **Case 2 (Flushing & Safe Shutdown):**
   - Automatically isolates the primary process tank via solenoid valve (`OV-01`).
   - Runs all three pumps simultaneously with a 4.2L flush reservoir to purge the lines.
   - Automatically transitions to a safe **OFF** state once flush fluid is exhausted (zero flow detected).

---

## 🔌 Sensors & Hardware Components (In Short)

| Category | Component | Tag / Model | Function / Specifications |
|---|---|:---:|---|
| **Microcontroller** | ESP32 NodeMCU-32S | `U7` | Dual-core 240MHz MCU executing non-blocking FreeRTOS state machine & pulse interrupts |
| **Flow Sensor** | Hall-Effect Flow Sensor | `FT-01` | In-line turbine pulse flow meter (GPIO 35, interrupt-driven, calibrated $Q = 0.129f + 0.032$) |
| **Level Sensor** | Float Level Switch | `LS-01` | Reed switch sensor detecting sump liquid surcharge and overflow trips (GPIO 34) |
| **Actuators (Pumps)** | 12V DC Diaphragm Pumps | `Pump 1-3` | **Pump-01:** Feed (50% PWM)<br/>**Pump-02:** Return (100% PWM)<br/>**Pump-03:** Flush line |
| **Isolation Valve** | 12V DC Solenoid Valve | `OV-01` | Normally-Open solenoid valve isolating 10L process tank during flush (GPIO 25) |
| **Motor Drivers** | H-Bridge Driver Modules | `U2, U3, U4` | High-current MOSFET/H-Bridge PWM drivers controlling pump speeds |
| **Power Supply** | 12V DC SMPS | Mains PS | Switched-mode power supply powering pumps, valves, and logic rails |
| **Manual Valves** | 2-Way Ball Valves | `BV-01, BV-02` | `BV-01`: Manual flow throttling<br/>`BV-02`: Flush line isolation |
| **3D Mounts** | Custom FDM Mounts | — | Custom 3D printed SMPS bracket, PCB standoff feet, and tubing clamps |

> Detailed pinout and electrical connections: [docs/HARDWARE_WIRING_GUIDE.md](docs/HARDWARE_WIRING_GUIDE.md)

---

## 🗂️ Repository Structure So Far

```
TEAM_DISTRO/
├── README.md                           # Master project documentation & system overview
├── index.html                          # Main 3D Digital Twin simulation (deployed from simulation/)
├── app.js                              # Simulation UI, interactive controls & charts
├── scene.js                            # Three.js 3D rig scene, models & animations
├── sim.js                              # Dynamic hydraulic simulation & FSM controller
├── tests.js                            # Automated validation test suite (TC-01 to TC-16)
├── run-tests.js                        # Headless test runner (Node.js)
├── three.min.js                        # Bundled Three.js engine (offline-ready)
├── simulation/                         # Modular simulation source directory
├── old_simulation/                     # Archived previous simulation version
├── 3DPRINT/                            # 3D printable mechanical files & documentation
│   ├── README.md                       # 3D print guide with image gallery & slicer specs
│   ├── stl/                            # AutoCAD STL 3D models
│   │   ├── smps clamp.stl              # 12V SMPS mounting bracket
│   │   ├── PCB foot01.stl              # Elevated PCB standoff foot clamp
│   │   └── pipe clamp.stl              # 8mm fluid tubing guide clamp
│   └── images/                         # CAD blueprints, dimensions & 3D renders
│       ├── smps_clamp_dimensions.png
│       ├── smps_clamp_cad_3d.png
│       ├── pcb_clamp_dimensions.png
│       ├── pcb_clamp_cad_3d.png
│       └── pipe_clamp_cad_3d.png
├── firmware/                           # Control firmware for ESP32
│   ├── team_distro_esp32.ino           # Single-file Arduino IDE firmware sketch
│   ├── platformio.ini                  # PlatformIO build configuration
│   └── src/
│       └── main.cpp                    # Modular C++ implementation
├── telemetry/                          # Python DAQ & live monitoring tools
│   ├── data_logger.py                  # Live serial telemetry reader and CSV logger
│   ├── calibration_tool.py             # Linear regression calibration solver
│   ├── generate_plots.py               # Telemetry plot generator
│   ├── serial_studio_project.json      # Serial Studio real-time dashboard layout
│   └── requirements.txt                # Python environment requirements
├── diagrams/                           # System diagrams & vector graphics
│   ├── pid_diagram.svg                 # Complete Piping & Instrumentation Diagram (P&ID)
│   ├── electrical_schematic.svg        # Electrical interconnect & driver schematic
│   └── state_machine.svg               # Finite State Machine (FSM) control logic
├── docs/                               # Detailed technical reports & specifications
│   ├── STAGE_2_TECHNICAL_REPORT.md     # Full engineering report (calculations, Reynolds No., residence time)
│   ├── PID_DESIGN_SPECIFICATION.md     # P&ID design & operational mode specifications
│   ├── CALIBRATION_AND_ANALYSIS.md     # Sensor calibration regression & valve throttling curves
│   ├── FAULT_ANALYSIS_FMEA.md          # Failure modes and effects analysis (Conditions A–E)
│   └── HARDWARE_WIRING_GUIDE.md        # PCB, SMPS, and ESP32 pinout & commissioning guide
├── test codes/                         # Individual component validation sketches
│   ├── README.md
│   └── testcodeforpump/                # Standalone PWM pump capacity test sketch
├── unverified/                         # Native ESP-IDF C++ modular firmware implementation
│   ├── README.md
│   ├── CMakeLists.txt
│   ├── platformio.ini
│   └── main/main.cpp
└── assets/                             # System charts & calibration plot figures
    ├── flow_vs_time_sample.png
    ├── sensor_calibration_curve.png
    └── valve_characteristic_curve.png
```

---

## 📊 Live Simulation & Detailed Reports

- **🌐 Live 3D Digital Twin Simulation:** [Launch in Browser](https://sonalhegde.github.io/TEAM_DISTRO/)
- **📐 3D Print Files & Images:** [3DPRINT/README.md](3DPRINT/README.md)
- **📑 Full Mathematical Calculations & Hydraulics Report:** [docs/STAGE_2_TECHNICAL_REPORT.md](docs/STAGE_2_TECHNICAL_REPORT.md)
- **⚡ Hardware Wiring Guide:** [docs/HARDWARE_WIRING_GUIDE.md](docs/HARDWARE_WIRING_GUIDE.md)
- **🔍 Fault Matrix & FMEA Analysis:** [docs/FAULT_ANALYSIS_FMEA.md](docs/FAULT_ANALYSIS_FMEA.md)

---

<div align="center">
Developed by <b>TEAM DISTRO</b> for the <b>Lam Research Challenge 3.0</b>.
</div>
