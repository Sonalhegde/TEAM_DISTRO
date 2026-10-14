# ESP32 Firmware Setup, Build, and Tooling Guide

This guide provides comprehensive instructions for building, flashing, and monitoring the **TEAM DISTRO** ESP32 process controller. It is written for engineering students and lab operators.

---

## 1. Hardware Prerequisites and USB-Serial Drivers

The ESP32 development board (NodeMCU-32S / ESP32-DevKitC) communicates with your PC using an on-board USB-to-UART bridge IC, typically either a **Silicon Labs CP2102/CP2104** or a **WCH CH340G/CH341**.

### Step 1: Install Driver (if not automatically recognized)
- **CP210x Driver**: [Silicon Labs CP210x VCP Drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
- **CH340 Driver**: [WCH CH340 Driver](http://www.wch-ic.com/downloads/CH341SER_EXE.html)

### Step 2: Locate the Serial Port
- **Windows**: Open **Device Manager** (`Win + X` -> Device Manager). Expand **Ports (COM & LPT)**. Note the port number, e.g., `COM3` or `COM7`.
- **Linux**: Open terminal and run:
  ```bash
  ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
  ```
  Ensure your user belongs to the serial dialout group:
  ```bash
  sudo usermod -aG dialout $USER
  ```
  *(Log out and back in for group changes to apply).*
- **macOS**: Open terminal and run:
  ```bash
  ls /dev/cu.usbserial* /dev/cu.wchusbserial* 2>/dev/null
  ```

### Step 3: Bootloader Mode and Hardware Reset
If your ESP32 board fails to auto-reset into the ROM bootloader during flash:
1. Locate the two pushbuttons on the ESP32 board: **BOOT** (GPIO 0) and **EN** / **RST** (Enable/Reset).
2. When the flash tool displays `Connecting........_____`, press and hold the **BOOT** button.
3. Release the **BOOT** button once flashing starts (indicated by progress bars such as `Writing at 0x00010000... [===]`).
4. Once flashing reaches 100%, press the **EN** button once to reset into user firmware if the board does not reboot automatically.

### Verify it worked
Plug in the ESP32 with a data-capable micro-USB cable. Check that a valid COM port appears in your operating system device list.

---

## 2. Option A: ESP-IDF in VS Code (Recommended)

The controller firmware is native ESP-IDF v5.x with the Arduino-as-component extension.

### Step 1: Install ESP-IDF Extension
1. Open Visual Studio Code.
2. Open the Extensions sidebar (`Ctrl + Shift + X`).
3. Search for **ESP-IDF** (published by Espressif Systems) and click **Install**.

### Step 2: Configure ESP-IDF Extension
1. Press `F1` or `Ctrl + Shift + P` to open the Command Palette.
2. Type and select `ESP-IDF: Configure ESP-IDF Extension`.
3. Choose **Express Setup**.
4. Select ESP-IDF version **v5.2.x** (or **v5.1.x**).
5. Specify the installation directory (e.g. `C:\Espressif\frameworks` on Windows or `~/esp/esp-idf` on Linux/macOS).
6. Wait for the extension to download the framework, Python virtual environment, and toolchains.

### Step 3: Open Project and Set Target
1. Open the repository root in VS Code (`File` -> `Open Folder...` -> select `TEAM_DISTRO`).
2. Open the Command Palette (`Ctrl + Shift + P`) and select `ESP-IDF: Set Espressif Device Target`.
3. Choose `ESP32` from the list.
4. Select `ESP-IDF: Select Port to Use (COM / ttyUSB)` and select your board COM port.

### Step 4: Build, Flash, and Monitor
- In the VS Code status bar at the bottom:
  - Click the **Cylinder Icon** (or run `ESP-IDF: Build your Project`) to compile.
  - Click the **Lightning Icon** (or run `ESP-IDF: Flash your Project`) to flash the ESP32.
  - Click the **Computer Monitor Icon** (or run `ESP-IDF: Monitor your Device`) to view serial output.

### Command-Line Equivalent
Open an ESP-IDF terminal (`export.bat` on Windows or `. export.sh` on Linux) and run:
```bash
cd firmware
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```
*(Replace `COM3` with your detected port. Press `Ctrl + ]` to exit the monitor).*

### Verify it worked
The serial console displays the startup banner:
```text
============================================================
  LAM RESEARCH CHALLENGE 3.0 - STAGE 2 VERIFIED FIRMWARE    
  TEAM: DISTRO | PCB: LAM STAGE 2 PCB REV 1.0 (T-Works)     
  FLOW TRANSMITTER FT-01 DAQ & CALIBRATION ENGINE READY      
============================================================
[INIT] CAL_A = 0.009244, CAL_B = 0.040111 (Empirically Calibrated) | Window = 1.00s
```

---

## 3. Option B: PlatformIO in VS Code

PlatformIO can build the project using the `espidf` framework.

### Compatibility Note
The firmware requires the ESP-IDF 5.x Pulse Counter driver (`driver/pulse_cnt.h`) and `arduino-esp32` v3.x. Ensure your PlatformIO environment uses the upstream Espressif32 platform that targets ESP-IDF 5.x.

### Steps
1. Install the **PlatformIO IDE** extension in VS Code.
2. Add a `platformio.ini` in the `firmware/` directory:
   ```ini
   [env:esp32dev]
   platform = espressif32
   board = esp32dev
   framework = espidf
   monitor_speed = 115200
   upload_speed = 921600
   board_build.partitions = partitions.csv
   ```
3. Open PlatformIO sidebar -> click **Build**.
4. Connect the ESP32 board and click **Upload and Monitor**.

### Verify it worked
PlatformIO compiles the binary, uploads it to address `0x10000`, and opens the serial monitor at 115200 baud showing the initialization frame.

---

## 4. Option C: Arduino IDE Porting Analysis

Can this project be built directly in the standalone Arduino IDE?

### Technical Porting Assessment
- **Status: Feasible with Arduino-ESP32 Core 3.0+; Incompatible with Core 2.x.**
- **Why Core 2.x Fails**:
  The firmware uses the new ESP-IDF 5.x pulse counter driver (`driver/pulse_cnt.h` with `pcnt_new_unit`, `pcnt_new_channel`, and `pcnt_glitch_filter_config_t`). Arduino-ESP32 Core 2.0.x relies on ESP-IDF 4.4, which used the deprecated legacy API (`driver/pcnt.h`). Compiling on Core 2.x causes missing header errors.
- **How to Compile on Arduino IDE 2.x**:
  1. Open Arduino IDE -> **Settings** -> add the ESP32 package URL:
     `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
  2. Open **Tools** -> **Board** -> **Boards Manager**.
  3. Install **esp32 by Espressif Systems** version **3.0.0 or higher**.
  4. Create a sketch containing `firmware/main/main.cpp` (renamed with `.ino` extension).
  5. Select Board: **ESP32 Dev Module**.
  6. Click **Upload**.

### Verify it worked
The Arduino IDE Serial Monitor at 115200 baud prints the boot banner and telemetry frame lines.

---

## 5. Flashing and Verifying Final Calibration Constants

The FT-01 turbine flow sensor uses the calibrated linear transfer function:
$$\dot{Q}\ (\text{L/min}) = \text{CAL\_A} \times f\ (\text{Hz}) + \text{CAL\_B}$$

### Step 1: Confirm Config Header and Source Constants
Inspect `firmware/main/config.h` and `firmware/main/main.cpp`:
- `CAL_A` = `0.009244` (Slope coefficient in L/min per Hz)
- `CAL_B` = `0.040111` (Zero-flow offset in L/min)

### Step 2: Flash Firmware
```bash
idf.py -p COM3 flash monitor
```

### Step 3: Verify Over Serial CLI
1. With the serial monitor running at 115200 baud, type `status` and press Enter.
2. Confirm the printed output:
   ```text
   STATUS | State: IDLE | Interlock: LOCKED (SAFE STOP) | Sump: NORMAL | ...
          | CAL_A=0.009244, CAL_B=0.040111
   ```
3. Type `demo` and press Enter to verify the regression solver across the 5 reference calibration points. Confirm $R^2 \ge 0.995$ and average error $\le 2.0\%$.

### Verify it worked
The terminal displays:
```text
  Fitted Slope (a)                   : 0.009244 L/min per Hz
  Fitted Offset (b)                  : 0.040111 L/min
  Determination Coeff (R^2)          : 0.99668
```

---

## 6. Serial Studio Real-Time Telemetry Dashboard

The firmware broadcasts telemetry frames at 1.0 Hz in delimited CSV format.

### Frame Format
```text
/*time_s,pulses,window_s,freq_Hz,Q_Lmin,Q_mLs,total_L,total_pulses,run_pulses,run_time_s,run_freq_Hz,Q_actual_Lmin,error_pct*/
```

| Field Index | Variable | Units | Description |
| :---: | :--- | :--- | :--- |
| 1 | `time_s` | s | System uptime since boot |
| 2 | `pulses` | counts | Delta pulse count in sample window |
| 3 | `window_s` | s | Measured sample window duration |
| 4 | `freq_Hz` | Hz | Pulse frequency ($f = N / t$) |
| 5 | `Q_Lmin` | L/min | Calibrated flow rate |
| 6 | `Q_mLs` | mL/s | Instantaneous flow rate |
| 7 | `total_L` | L | Cumulative volume totalizer |
| 8 | `total_pulses` | counts | 64-bit cumulative pulses |
| 9 | `run_pulses` | counts | Calibration run pulses |
| 10 | `run_time_s` | s | Calibration run duration |
| 11 | `run_freq_Hz` | Hz | Calibration mean frequency |
| 12 | `Q_actual_Lmin` | L/min | Measured volume flow rate |
| 13 | `error_pct` | % | Flow measurement error |

### Step-by-Step Dashboard Setup
1. Download and install [Serial Studio](https://serial-studio.github.io/).
2. Open Serial Studio.
3. In the top toolbar, click **Project Setup** (gear/document icon).
4. Click **Load Project** and select `dashboard/serial_studio_project.json`.
5. In the left connection pane:
   - Select **Communication Mode**: `Serial Port`.
   - Select your ESP32 **Port** (e.g. `COM3`).
   - Set **Baud Rate**: `115200`.
   - Data Bits: `8`, Parity: `None`, Stop Bits: `1`, Flow Control: `None`.
6. Click **Connect**.

### Verify it worked
The Serial Studio dashboard lights up with real-time gauges:
- Gauge 1: Flow Rate (L/min)
- Gauge 2: Pulse Frequency (Hz)
- Plot 1: Flow Rate vs Time
- Plot 2: Total Transferred Volume (L)
- Status Widget: Sump Float Switch & Safety Interlock State
