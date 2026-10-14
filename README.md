# Lam Research Challenge 3.0: Stage 2 Practical Engineering Challenge

<div align="center">

![Team ID](https://img.shields.io/badge/Team%20ID-LRC--26--0528-blue?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Complete-success?style=for-the-badge)
[![Live 3D Simulation](https://img.shields.io/badge/Live%20Simulation-3D%20Digital%20Twin-brightgreen?style=for-the-badge&logo=three.js)](https://sonalhegde.github.io/TEAM_DISTRO/)
![Controller](https://img.shields.io/badge/Controller-ESP32%20NodeMCU-red?style=for-the-badge&logo=espressif)
![Firmware](https://img.shields.io/badge/Firmware-Native%20ESP--IDF%20C%2B%2B-teal?style=for-the-badge&logo=espressif)
![Serial Studio](https://img.shields.io/badge/Dashboard-Serial%20Studio-orange?style=for-the-badge)

### **Team DISTRO**
**Automated Liquid-Handling, Recirculation, and Telemetry Platform**

👉 **[Launch Live 3D Digital Twin Simulation](https://sonalhegde.github.io/TEAM_DISTRO/)** 👈

</div>

This repository provides the complete embedded controller firmware, MATLAB calibration analysis scripts, 3D printable CAD models, and real-time DAQ dashboard for the Lam Research Challenge 3.0 Stage 2 automated liquid-handling rig (Team ID: LRC-26-0528).
The system uses an ESP32 microcontroller with high-power BTS7960 H-bridge drivers to regulate fluid recirculation, protect against sump overflow surges, and execute automated line flushes with live serial telemetry.
All engineering documentation, build instructions, Git workflows, and calibration derivations are unified in this document.

---

## 1. Team Members & Project Roles

- **Institution:** Nitte (Deemed to be University) - NMAM Institute of Technology
- **Team Identifier:** LRC-26-0528 | **Team Name:** Team DISTRO

| Name | Role | Department | USN | Email | GitHub | Contribution |
|---|---|---|---|---|---|---|
| **Sonal Hegde** | Team Leader | Department of ECE | NNM24AC050 | [nnm24ac050@nmamit.in](mailto:nnm24ac050@nmamit.in) | [@Sonalhegde](https://github.com/Sonalhegde) | Coding and Documentation |
| **Pratham Amin** | Member | Department of Mechanical Engineering | NNM24ME084 | [nnm24me084@nmamit.in](mailto:nnm24me084@nmamit.in) | [@pratham885](https://github.com/pratham885) | Piping, 3D Printing, CAD, Layout Designing, and Calibration |
| **Jostan Saldanha** | Member | Department of ECE | NNM25AC502 | [nnm25ac502@nmamit.in](mailto:nnm25ac502@nmamit.in) | [@jostan-saldanha](https://github.com/jostan-saldanha) | Electronics and Firmware |
| **Mohammed Araan** | Member | Department of CSE | NNM24CS142 | [nnm24cs142@nmamit.in](mailto:nnm24cs142@nmamit.in) | [@Araan-Sheikh](https://github.com/Araan-Sheikh) | Video Editing and 3D Modelling |

---

## 2. System Overview

### Case 1: Closed-Loop Recirculation with Sump Level Protection
- **Normal Recirculation:** Solenoid valve OV-01 opens; Pump-1 operates at 50% duty cycle to feed fluid from the 10 L main reservoir into the sump, and Pump-2 runs at 100% duty cycle to pump fluid through the FT-01 turbine flow meter and return it to the 10 L tank, maintaining a steady 3.0 L/min flow.
- **Sump Surge Interlock:** When external fluid causes the sump level to rise, float switch LS-01 trips (contact closes). The controller immediately halts Pump-1 (0% duty) while keeping Pump-2 running at 100% duty cycle to evacuate excess liquid back to the 10 L tank, preventing any spill.
- **Automatic Recovery:** Once the sump level drops below the float switch threshold and remains clear for 3.0 seconds, Pump-1 automatically restarts at 50% duty cycle, and normal recirculation resumes without manual intervention.

### Case 2: Multi-Pump Line Flush and Autonomous Safe Shutdown
- **10 L Reservoir Isolation:** Solenoid valve OV-01 closes, isolating the 10 L process reservoir from the flow circuit.
- **Concurrent Multi-Pump Flushing:** Pump-1 (50%), Pump-2 (100%), and Pump-3 (100%) operate simultaneously to draw deionized flush water from the 4.2 L flush reservoir through the entire piping network.
- **Autonomous Depletion Shutdown:** The FT-01 sensor continuously measures delivered volume and line velocity; when the 4.2 L flush reservoir finishes and fluid flow collapses below 0.35 L/min (air entrainment), the controller automatically cuts off all three pumps and safely transitions to idle.

---

## 3. Hardware Components

### Mechanical hardware
| Component | Description | Main use |
|---|---|---|
| Ball Valve (BV-01) | Manually operated valve with a spherical shut-off mechanism | Manually isolates or permits flow in the pipeline; useful for maintenance, testing and flow isolation |
| 10-Litre Fluid Tank (Fluid Tank-01) | Transparent storage tank, approx. 10 L | Main process-fluid reservoir; the transparent body allows visual monitoring of level and condition |
| 5-Litre Sump | Small transparent reservoir labelled SUMP, approx. 4.5 L | Collection/storage vessel for fluid entering or returning to the sump section |
| 4.2-Litre Flush Fluid Tank / Flush Line | Transparent vessel labelled FLUSH FLUID, approx. 4.5 L | Supplies flush fluid for cleaning the main pipe circuit |
| ON/OFF Valve (OV-01) | Electrically operated solenoid-type valve | Automatic control of flow; lets the controller switch a flow path on or off |
| Pumps (Pump-1, Pump-2, Pump-3) | Motor-driven fluid pumps | Circulate and transfer fluid between sections; each pump is independently controlled |
| 8 mm Process Pipe | Flexible blue tubing, approx. 8 mm | Main flow path connecting tanks, valves, pumps, tees and other components |
| 10 mm x 8 mm Reducers | Fittings that transition between a 10 mm connection and 8 mm pipe | Adapt the pump connections to the 8 mm pipe (mainly used for the pumps) |
| 8 mm Connectors | Push-fit/barbed fittings for 8 mm tubing | Join pipes to components and make detachable connections |
| 8 mm Equal Tee | T-shaped connector with three equal 8 mm ports | Splits one 8 mm line into two branches or combines two branches into one |

### Electronic hardware
| Component | Identification / function | Use |
|---|---|---|
| PCB (Printed Circuit Board) | Custom control board | Electrical interconnection between the 12 V supply, ESP32, sensors, valves, indicators and pump-driver interfaces |
| ESP32 / NodeMCU ESP32S module (U7) | Main microcontroller | Central controller; provides GPIO/PWM outputs and reads sensors and switches |
| 12 V DC Input Connector | Marked 12V INPUT | Connection point for the external 12 V DC supply |
| 12 V Output Connectors | P1, P2, P3 and additional 12 V output terminals | 12 V power for external loads such as the pump-driver/motor circuits |
| Motor Driver Interfaces (U2, U3, U4) | Three multi-pin driver connectors on the PCB | Dedicated driver/control connections for Pump-1, Pump-2 and Pump-3 |
| Solenoid Valve Connector | Labelled SOLENOID_VALVE - S/V/G | Controls/connects the solenoid (ON/OFF) valve |
| Flow Sensor Connector | Labelled FLOW_SENSOR - S/V/G | Powers the flow sensor and receives its signal. The flow sensor must be calibrated |
| Float Switch Connector | Labelled FLOAT_SWITCH - FSW/GND | Interface for the float/level switch that detects a water-level condition |
| ON/OFF Switch Connector | Labelled ON/OFF_SWITCH - SW/GND | Interface for the external manual ON/OFF switch |
| 12 V DC SMPS | 12 V DC SMPS | Power supply for the board |

> **Note on Reservoir Capacities:** The sump vessel is physically labelled 5 L, the flush tank is labelled 4.2 L, and the challenge hardware bill of materials lists both as approx. 4.5 L. For process calculations, the measured usable fluid volumes are 2.0 L nominal sump capacity and 4.15 L usable flush volume (accounting for 50 mL suction line holdup).

---

## 4. 3D-Printed Parts and Mounts

Custom 3D-printed mechanical components secure the electrical and hydraulic assemblies to the aluminum rig frame. STL files are located in [`cad/`](cad/).

| Component | Part & STL File | Description & Purpose | View / Diagram |
|---|---|---|:---:|
| **SMPS Power Supply Bracket** | `smps-clamp.stl`<br>([`cad/smps-clamp.stl`](cad/smps-clamp.stl)) | Mounts and secures the 12 V DC industrial SMPS power brick to the aluminum structural rail. | ![SMPS Clamp 3D Render](media/smps_clamp_cad_3d.png)<br>*3D CAD Isometric Model* |
| **SMPS Dimensions & Blueprint** | `smps-clamp.stl` | Dimensional drawings showing mounting hole spacing and structural ribbing. | ![SMPS Clamp Dimensions](media/smps_clamp_dimensions.png)<br>*Engineering Dimensions* |
| **Control PCB Mounting Foot** | `pcb-foot.stl`<br>([`cad/pcb-foot.stl`](cad/pcb-foot.stl)) | Elevated standoff foot that clips to the PCB edge, electrically isolating the board from the rig frame. | ![PCB Foot 3D Render](media/pcb_clamp_cad_3d.png)<br>*PCB Standoff Foot Render* |
| **PCB Foot Dimensioned Drawing** | `pcb-foot.stl` | Top and side elevation blueprint showing PCB slot tolerance and M3 bolt hole. | ![PCB Foot Dimensions](media/pcb_clamp_dimensions.png)<br>*PCB Foot Blueprint* |
| **8 mm Fluid Pipe Clamp** | `pipe-clamp.stl`<br>([`cad/pipe-clamp.stl`](cad/pipe-clamp.stl)) | Retention clamp that routes and dampens vibration in the 8 mm flexible polyurethane fluid tubing. | ![Pipe Clamp 3D Render](media/pipe_clamp_cad_3d.png)<br>*Tubing Retainer Clamp* |
| **Pipe Clamp Isometric View** | `pipe-clamp.stl` | Profile view showing snap-fit conduit channel and fastening flange. | ![Pipe Clamp Isometric](media/pipe_clamp_iso.png)<br>*Pipe Clamp Profile* |

---

## 5. Repository Layout

```text
TEAM_DISTRO/
├── README.md        Unified master documentation, quick start, hardware tables, and setup guide
├── LICENSE          Software licensing terms (MIT / Academic Open Access)
├── .gitignore       Excludes build directories, sdkconfig, temporary logs, and IDE caches
├── firmware/        Native ESP-IDF v5.x C++ firmware project with batch build/flash utilities
│   ├── CMakeLists.txt     Root ESP-IDF CMakeLists build script
│   ├── sdkconfig.defaults Default configuration parameters (FreeRTOS 1000 Hz, CPU 240 MHz)
│   ├── esp_build.bat      1-Click batch compilation script for Windows
│   ├── esp_flash.bat      1-Click batch flashing and serial monitor script for Windows
│   ├── pre_flash.bat      Serial port scan and COM lock cleanup tool
│   ├── main/
│   │   ├── CMakeLists.txt Component CMake configuration
│   │   ├── idf_component.yml ESP-IDF component manager manifest (arduino-esp32)
│   │   ├── config.h       Centralized GPIO pinout, calibration constants, and timings with units
│   │   └── main.cpp       Controller state machine, PCNT pulse counter, and serial CLI
│   └── tools/             Python build and comment verification utilities
├── analysis/        MATLAB calibration analysis scripts and Python command-line telemetry tools
│   ├── ft01_calibration_plots.m  MATLAB script: FT-01 linear regression & error plots
│   ├── valve_position_plot.m     MATLAB script: Ball valve BV-01 throttling curve
│   ├── run_analysis.bat          1-Click Windows script to execute MATLAB scripts and generate plots
│   ├── data_logger.py            Python real-time serial DAQ logger
│   ├── calibration_tool.py       Python least-squares regression tool
│   ├── generate_plots.py         Python Matplotlib figure generator
│   ├── requirements.txt          Python dependency specification
│   └── data/                     Empirical calibration trial CSV datasets
├── dashboard/       Serial Studio live telemetry visualization package
│   └── serial_studio_project.json Serial Studio UI dashboard project mapping frames to widgets
├── media/           P&ID schematics, telemetry plots, calibration curves, and 3D-print renders
│   ├── ft01_calibration_curve.png   Calibration curve plot (Q vs frequency)
│   ├── ft01_calibration_error.png   Calibration error vs flow rate bar chart
│   ├── valve_position_vs_flow.png   Ball valve throttling characteristic curve
│   ├── serial_studio_dashboard.png  Real-time Serial Studio telemetry dashboard screenshot
│   ├── pid_diagram.svg              Piping and Instrumentation Diagram (P&ID)
│   ├── electrical_schematic.svg     LAM Stage 2 PCB electrical wiring schematic
│   └── state_machine.svg            Finite State Machine state transition diagram
├── cad/             STL fabrication files for all 3D-printed mechanical components
│   ├── smps-clamp.stl  12 V SMPS power supply chassis mounting bracket
│   ├── pcb-foot.stl    Control PCB elevated standoff mounting foot
│   └── pipe-clamp.stl  8 mm fluid tubing vibration damper and routing clamp
├── simulation/      Headless and browser-based 3D digital twin package
│   ├── web/         HTML5 / Three.js WebGL digital twin simulation
│   ├── vendor/      Three.js rendering engine
│   └── archive/     Legacy simulation files
└── tests/           Automated unit tests and standalone component bench test firmware
    ├── simulation/  Node.js automated verification test suite (TC-01 through TC-16)
    └── firmware-bench/ Standalone diagnostic sketches for individual pumps and sensors
```

---

## 6. Getting the Code (Git Guide)

### 1. First-Time Git Configuration
Before committing, set your username and email:
```bash
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"
```
*Verify it worked:* Run `git config --list` and confirm your name and email appear.

### 2. Cloning the Repository
Download the local working copy:
```bash
git clone https://github.com/Sonalhegde/TEAM_DISTRO.git
cd TEAM_DISTRO
```
*Verify it worked:* Run `git status` — it must report `On branch main` with a clean tree.

### 3. Pulling Updates
Fetch and merge incoming commits:
```bash
git pull origin main
```
*Verify it worked:* Run `git log -n 1 --oneline` and check that the commit matches GitHub remote.

### 4. Branching and Switching
Always work in a feature branch:
```bash
git checkout -b feature/calibration-update
```
*Verify it worked:* Run `git branch` — the active branch is marked with `*`.

### 5. Staging, Committing, and Pushing
```bash
git add firmware/main/config.h
git commit -m "refactor: update flow calibration constants"
git push -u origin feature/calibration-update
```
*Verify it worked:* Open GitHub in your browser to verify the branch was published.

### 6. Resolving Pull Conflicts
If `git pull` reports merge conflicts with your local edits:
1. Stash local uncommitted changes: `git stash`
2. Pull latest remote changes: `git pull origin main`
3. Re-apply your changes: `git stash pop`
4. If merge conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`) appear in a file, open it in your editor, pick the correct lines, save, and mark resolved:
   ```bash
   git add <filename>
   git commit -m "merge: resolve incoming conflict"
   ```
*Verify it worked:* Run `git status` — it must report `nothing to commit, working tree clean`.

---

## 7. Firmware Setup & Build Guide

The controller firmware is located in [`firmware/`](firmware/) and targets the ESP32 NodeMCU-32S board.

### Option A: ESP-IDF in Visual Studio Code (Recommended)
1. Install the official **ESP-IDF Extension** (by Espressif) from the VS Code Extensions marketplace (`Ctrl+Shift+X`).
2. Press `F1` or `Ctrl+Shift+P`, type `ESP-IDF: Configure ESP-IDF Extension`, and select **Express Setup**.
3. Choose ESP-IDF version **v5.2.x** (or **v5.1.x**). Wait for the toolchains and Python virtual environment to install.
4. Open the `TEAM_DISTRO` folder in VS Code.
5. In the Command Palette, select `ESP-IDF: Set Espressif Device Target` -> select `esp32`.
6. Select `ESP-IDF: Select Port to Use (COM / ttyUSB)` -> choose your ESP32 COM port.
7. Click the **Build** (cylinder) icon in the bottom status bar, or press `Ctrl+E B`.
8. Click the **Flash** (lightning) icon, then the **Monitor** (computer screen) icon.

#### CLI Equivalents (ESP-IDF Terminal)
```bash
cd firmware
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```
*(Press `Ctrl + ]` to exit the serial monitor).*  
*Verify it worked:* The serial monitor prints the boot banner:
```text
============================================================
  LAM RESEARCH CHALLENGE 3.0 - STAGE 2 VERIFIED FIRMWARE    
  FLOW TRANSMITTER FT-01 DAQ & CALIBRATION ENGINE READY      
============================================================
[INIT] CAL_A = 0.009244, CAL_B = 0.040111 | Window = 1.00s
```

### Option B: PlatformIO in VS Code
PlatformIO is supported by adding a `platformio.ini` in `firmware/`:
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = espidf
monitor_speed = 115200
upload_speed = 921600
```
Open the PlatformIO sidebar, click **Build**, then click **Upload and Monitor**.  
*Verify it worked:* PlatformIO uploads to `0x10000` and displays the startup banner in the terminal.

### Option C: Arduino IDE Porting Analysis
- **Status: Compiles on Arduino-ESP32 Core 3.0+; Incompatible with Core 2.x.**
- **Technical Reason:** The firmware directly calls the ESP-IDF 5.x Pulse Counter driver (`driver/pulse_cnt.h` with `pcnt_new_unit`, `pcnt_new_channel`, and `pcnt_glitch_filter_config_t`). Arduino-ESP32 Core 2.x was built on ESP-IDF 4.4, which used the deprecated `driver/pcnt.h` driver.
- **How to port to Arduino IDE:** Install Arduino IDE 2.x, open Boards Manager, install **esp32 by Espressif Systems version 3.0.0 or higher**, rename `firmware/main/main.cpp` to `firmware.ino`, select **ESP32 Dev Module**, and click Upload.

### Option D: USB-Serial Drivers, Ports, and BOOT Button Flashing
- **Drivers:** If the ESP32 is not recognized when plugged in:
  - Silicon Labs CP2102/CP2104: Download [CP210x VCP Driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers).
  - WCH CH340G/CH341: Download [CH341SER Driver](http://www.wch-ic.com/downloads/CH341SER_EXE.html).
- **Finding the Port:**
  - Windows: Open **Device Manager** -> expand **Ports (COM & LPT)** (e.g. `COM3`).
  - Linux: Run `ls /dev/ttyUSB* /dev/ttyACM*`. Add user to group: `sudo usermod -aG dialout $USER`.
  - macOS: Run `ls /dev/cu.usbserial*`.
- **BOOT-Button Flashing:** If esptool displays `A fatal error occurred: Failed to connect to ESP32: Timed out waiting for packet header`:
  1. Hold down the **BOOT** button (GPIO 0) on the ESP32 board.
  2. Start the flash command.
  3. When the terminal prints `Connecting...`, release the BOOT button.
  4. Press the **EN** / **RST** button once after flashing completes to boot the firmware.

### Option E: Using the Batch Files
The `.bat` scripts live inside `firmware/` and `analysis/` next to the code they execute:

| Batch Script | Location | Purpose & How to Run |
|---|---|---|
| `esp_build.bat` | `firmware/` | Compiles the ESP-IDF project using the configured toolchain. Run from PowerShell: `.\firmware\esp_build.bat` or double-click. |
| `esp_flash.bat` | `firmware/` | Automatically scans COM ports, kills any stale monitor processes, flashes the compiled binary, and starts the serial monitor. |
| `pre_flash.bat` | `firmware/` | Runs environment diagnostics and scans active COM ports before flashing. |
| `run_analysis.bat` | `analysis/` | Executes both MATLAB scripts (`ft01_calibration_plots.m` and `valve_position_plot.m`) in batch mode to regenerate all figures. |

*Verify it worked:* Run `.\firmware\esp_build.bat` — it prints `BUILD SUCCESSFUL!` and outputs the `.bin` artifact.

### Option F: Flashing and Verifying Calibration Constants
1. Centralized constants in [`firmware/main/config.h`](firmware/main/config.h) and [`firmware/main/main.cpp`](firmware/main/main.cpp):
   - `CAL_A = 0.009244` (Slope in L/min per Hz)
   - `CAL_B = 0.040111` (Zero-flow offset in L/min)
2. Flash the board: `.\firmware\esp_flash.bat`
3. In the serial console at 115200 baud, type `status` and press Enter.  
*Verify it worked:* The console returns `CAL_A=0.009244, CAL_B=0.040111`. Type `demo` to verify regression calculation across the 5 reference calibration points.

---

## 8. Serial Studio Real-Time Telemetry Dashboard

The firmware broadcasts real-time telemetry frames once per second at **115200 baud** (8-N-1):

```text
/*time_s,pulses,window_s,freq_Hz,Q_Lmin,Q_mLs,total_L,total_pulses,run_pulses,run_time_s,run_freq_Hz,Q_actual_Lmin,error_pct*/\r\n
```

### Dashboard Setup Instructions
1. Download and install [Serial Studio](https://serial-studio.github.io/).
2. Open Serial Studio, click **Project Setup** (gear/document icon), and select **Load Project**.
3. Choose [`dashboard/serial_studio_project.json`](dashboard/serial_studio_project.json).
4. In the connection pane:
   - Select **Serial Port** mode.
   - Choose your ESP32 COM port (e.g. `COM3`).
   - Set **Baud Rate** to `115200`, Data Bits `8`, Parity `None`, Stop Bits `1`.
5. Click **Connect**.

![Serial Studio Dashboard](media/serial_studio_dashboard.png)
*Live Serial Studio Dashboard: Pulse frequency dial, flow rate dials, flow vs time strip charts, and cumulative volume totalizer.*

*Verify it worked:* The dials and strip charts update live every 1.0 s as fluid moves through the FT-01 sensor.

---

## 9. Data Analysis Scripts (MATLAB)

The repository provides automated MATLAB scripts in [`analysis/`](analysis/) for sensor calibration regression and valve throttling characterization.

### Requirements
- **MATLAB Version:** R2020a, R2021a/b, R2022a/b, R2023a/b, R2024a/b or newer. (Also compatible with GNU Octave 6.0+).
- **Toolboxes:** Standard MATLAB base installation (uses built-in `polyfit`, `polyval`, `readtable`, `exportgraphics`).

### Script 1: `ft01_calibration_plots.m`
- **Purpose:** Fits linear calibration equation $Q = a \cdot f + b$ to 6 physical gravimetric trials and tests prediction error against an independent validation point.
- **Inputs:** Pulse count $N$, duration $t$, and volume $V$ collected in graduated cylinders.
- **Outputs:**
  - `media/ft01_calibration_curve.png` (Calibration curve of flow rate vs pulse frequency)
  - `media/ft01_calibration_error.png` (Calibration error bar chart across flow rates)
- **How to Run:**
  ```matlab
  cd('analysis');
  ft01_calibration_plots;
  ```
  *(Or execute `.\analysis\run_analysis.bat` from Windows terminal).*

![FT-01 Calibration Curve](media/ft01_calibration_curve.png)
*Figure 1: FT-01 turbine pulse frequency vs measured volumetric flow rate showing linear fit ($R^2 = 0.99668$).*

![FT-01 Calibration Error](media/ft01_calibration_error.png)
*Figure 2: Percentage calibration error across operating flow envelope with independent validation point.*

### Script 2: `valve_position_plot.m`
- **Purpose:** Analyzes the throttling characteristic curve of manual ball valve BV-01 across 25%, 50%, 75%, and 100% handle positions.
- **Inputs:** Valve position percentage and steady-state pulse frequencies recorded via Serial Studio.
- **Outputs:** `media/valve_position_vs_flow.png`
- **How to Run:**
  ```matlab
  cd('analysis');
  valve_position_plot;
  ```

![Valve Throttling Curve](media/valve_position_vs_flow.png)
*Figure 3: Measured volumetric flow rate (L/min) as a function of ball valve opening percentage.*

---

## 10. Calibration & Results Summary

Physical catch-and-weigh calibration was conducted on the rig using water at $25^\circ\text{C}$ across six flow rates spanning 0.775 to 2.040 L/min, with an independent validation trial at 1.110 L/min.

$$\dot{Q}\ (\text{L/min}) = 0.009244 \times f\ (\text{Hz}) + 0.040111$$

| Metric | Measured Value | Acceptance Criterion | Status |
|---|:---:|:---:|:---:|
| **Fitted Slope ($a$)** | `0.009244` L/min per Hz | Positive linear slope | Pass |
| **Fitted Intercept ($b$)** | `0.040111` L/min | Near-zero offset | Pass |
| **Coefficient of Determination ($R^2$)** | `0.99668` | $R^2 \ge 0.995$ | **Pass** |
| **Maximum Calibration Error** | `3.66%` | $\le 5.0\%$ | **Pass** |
| **Average Calibration Error** | `1.62%` | $\le 2.0\%$ | **Pass** |
| **Independent Validation Error** | `1.06%` | $\le 3.0\%$ | **Pass** |

---

## 11. Known Limitations

1. **Rotor Inertia and Low-Velocity Cutoff:** The FT-01 micro-turbine rotor exhibits boundary friction below 1.0 Hz (~0.05 L/min). Flow rates below 0.50 L/min are treated as zero flow by the software noise gate to prevent zero-flow integration creep.
2. **Manual Ball Valve Repeatability:** Ball valves BV-01 and BV-02 rely on manual handle alignment. Slight positioning variance ($\pm 5^\circ$) can alter line resistance by $\approx 0.10\text{ L/min}$ at lower throttle angles.
3. **Optocoupler Relay High-Impedance Requirement:** The 5 V optocoupler relay board requires a 5 V pull-up to fully turn off. To prevent 3.3 V GPIO back-powering, the firmware releases the pin to high-impedance mode (`pinMode(PIN_SV, INPUT)`) when commanded closed.
4. **FreeRTOS Single-Core Loop Yield:** The main `loop()` function includes a mandatory `delay(2)` yield call to prevent triggering the ESP-IDF FreeRTOS Task Watchdog Timer (`IDLE0` starvation).
