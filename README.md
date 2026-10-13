# Lam Research Challenge 3.0: Stage 2 Practical Engineering Challenge

<div align="center">

![Team ID](https://img.shields.io/badge/Team%20ID-LRC--26--0528-blue?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Complete-success?style=for-the-badge)
[![Live 3D Simulation](https://img.shields.io/badge/Live%20Simulation-3D%20Digital%20Twin-brightgreen?style=for-the-badge&logo=three.js)](https://sonalhegde.github.io/TEAM_DISTRO/)
![Controller](https://img.shields.io/badge/Controller-ESP32%20NodeMCU-red?style=for-the-badge&logo=espressif)
![Firmware](https://img.shields.io/badge/Firmware-C%2B%2B%20%2F%20Arduino-teal?style=for-the-badge&logo=arduino)
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
| **Jostan Saldanha** | Member | Department of ECE | NNM25AC502 | nnm25ac502@nmamit.in | [@jostan-saldanha](https://github.com/jostan-saldanha) | Electrical and Coding |
| **Mohammed Araan** | Member | Department of CSE | NNM24CS142 | nnm24cs142@nmamit.in | [@Araan-Sheikh](https://github.com/Araan-Sheikh) | Video Editing and 3D Modelling |
| **Pratham Amin** | Member | Department of Mechanical Engineering | NNM24ME084 | nnm24me084@nmamit.in | [@pratham885](https://github.com/pratham885) | Piping, 3D Printing, CAD, and Calibration |

---

## 3. Hardware and Sensor Components

| Tag | Component | Function | Interface / GPIO |
|---|---|---|:---:|
| `U7` | ESP32 NodeMCU-32S | 32-bit dual-core microcontroller running FreeRTOS state engine | Core Controller |
| `FT-01` | Hall-Effect Flow Sensor | Measures pulse frequency to determine volumetric flow rate | GPIO 35 (Interrupt) |
| `LS-01` | Float Level Switch | Detects high liquid level in the sump tank to trigger interlock | GPIO 34 (Digital In) |
| `OV-01` | 12 V Solenoid Valve | Isolates the 10 L process reservoir during flushing | GPIO 25 (Digital Out) |
| `P-01` | 12 V DC Diaphragm Pump | Feeds fluid from 10 L tank to the sump (50% PWM) | GPIO 18 (PWM Ch 0) |
| `P-02` | 12 V DC Diaphragm Pump | Returns fluid from sump to 10 L tank (100% PWM) | GPIO 19 (PWM Ch 1) |
| `P-03` | 12 V DC Diaphragm Pump | Delivers DI water from flush tank during Case 2 | GPIO 21 (PWM Ch 2) |
| `U2, U3, U4` | H-Bridge Motor Drivers | Amplifies logic PWM signals to drive 12 V pump motors | Driver Modules |
| `BV-01, BV-02` | 2-Way Manual Ball Valves | BV-01 controls flush inlet; BV-02 controls throttling | Manual Valves |
| `SMPS` | 12 V DC Power Supply | Supplies DC power to pumps, solenoid, and PCB logic rails | Mains Supply |

Detailed pin assignments are in [`docs/hardware-wiring-guide.md`](docs/hardware-wiring-guide.md).

---

## 4. Quick Start

### Run the 3D Digital Twin Simulation
- **Live Online:** **[https://sonalhegde.github.io/TEAM_DISTRO/](https://sonalhegde.github.io/TEAM_DISTRO/)**
- **Offline:** Open [`simulation/web/index.html`](simulation/web/index.html) in any modern web browser.

### Run Automated Simulation Tests
Run the headless Node.js verification test suite:
```bash
node tests/simulation/run-tests.js
```
All 16 test cases verify automatically.

### Upload Controller Firmware
- **Arduino IDE:** Open [`firmware/arduino/team_distro_esp32.ino`](firmware/arduino/team_distro_esp32.ino) and upload to your ESP32 board.
- **PlatformIO:** Open [`firmware/platformio/`](firmware/platformio/) and run `pio run -t upload`.

### Run Telemetry Logger
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
│       └── pages.yml         # GitHub Pages automated deployment workflow
├── README.md                 # Project overview, problem statement, team, links
├── TEAM.md                   # Team members, roles, departments, contributions
├── LICENSE                   # Software license terms
├── .gitignore
├── docs/                     # System design reports and guides
│   ├── README.md             # Documentation index
│   ├── technical-report.md   # Engineering report with derivations
│   ├── pid-design-specification.md
│   ├── calibration-and-analysis.md
│   ├── fault-analysis-fmea.md
│   ├── hardware-wiring-guide.md
│   ├── diagrams/             # P&ID and electrical schematics
│   └── figures/              # Plots and calibration figures
├── firmware/                 # Microcontroller source code
│   ├── README.md             # Firmware architecture guide
│   ├── arduino/              # Single-file Arduino IDE firmware sketch
│   ├── platformio/           # Modular C++ PlatformIO project
│   └── esp-idf-experimental/ # Experimental native ESP-IDF C++ implementation
├── hardware/
│   └── 3d-print/             # 3D printable CAD models and specifications
│       ├── README.md         # Component index and slicer settings
│       ├── stl/              # Individual STL files
│       └── images/           # CAD blueprints and 3D visual renders
├── simulation/               # 3D digital twin package
│   ├── README.md             # Simulation architecture and user guide
│   ├── web/                  # HTML5/WebGL interactive simulation
│   ├── vendor/               # Three.js 3D rendering engine
│   └── archive/              # Earlier simulation build
├── tests/                    # Verification test suites
│   ├── README.md             # Test catalog and run instructions
│   ├── simulation/           # Headless Node.js test suite (TC-01 to TC-16)
│   └── firmware-bench/       # Physical rig component test sketches
└── telemetry/                # Python data logging and DAQ tools
    ├── README.md             # Telemetry usage and setup instructions
    ├── data_logger.py        # Real-time serial data logger
    ├── calibration_tool.py   # Regression solver
    ├── generate_plots.py     # Plot generator
    ├── requirements.txt      # Python dependencies
    └── serial_studio_project.json # Serial Studio dashboard profile
```

---

## 6. License

Refer to [`LICENSE`](LICENSE) for licensing terms.
