# Firmware Guide

This folder contains the firmware for the ESP32 NodeMCU microcontroller.

## 1. Pin Map

| Component | Tag | Type | ESP32 GPIO | Description |
|---|---|---|:---:|---|
| Flow Sensor | FT-01 | Input (Interrupt) | 35 | Pulse signal with 4.7 kΩ pull-up |
| Float Switch | LS-01 | Digital Input | 34 | Sump level switch (Internal pull-up) |
| Run Switch | Master Switch | Digital Input | 32 | SPST run switch (Active LOW) |
| Solenoid Valve | OV-01 | Digital Output | 25 | Active HIGH = Open, LOW = Closed |
| Pump-01 | P-01 | PWM Output (Ch 0) | 18 | 5 kHz PWM, 50% duty cycle |
| Pump-02 | P-02 | PWM Output (Ch 1) | 19 | 5 kHz PWM, 100% duty cycle |
| Pump-03 | P-03 | PWM Output (Ch 2) | 21 | 5 kHz PWM, flush pump |

## 2. Finite State Machine (FSM)

The controller operates as a deterministic Finite State Machine (FSM):
1. `IDLE`: The system waits for the master run switch.
2. `CASE1`: Normal closed-loop recirculation. Pump-01 runs at 50% PWM. Pump-02 runs at 100% PWM. Target flow is 3.0 L/min.
3. `CASE1_ALARM`: Float switch trips due to external liquid. Pump-01 stops immediately. Pump-02 runs at 100% to drain the sump.
4. `FLUSH`: Valve OV-01 closes. The operator opens valve BV-01. All three pumps run simultaneously to flush the network.
5. `DONE`: Flow stops after the 4.2 L flush volume completes. All pumps stop safely.

## 3. Sensor Calibration Equation

The pulse frequency $f$ converts to volumetric flow rate $Q$ with this linear equation:

$$Q = 0.129 \cdot f + 0.032\text{ L/min}$$

## 4. Firmware Targets

### Arduino IDE Format
- Location: [`arduino/team_distro_esp32.ino`](arduino/team_distro_esp32.ino)
- Open the `.ino` file in the Arduino IDE. Select the `ESP32 Dev Module` board. Click **Upload**.

### PlatformIO Format
- Location: [`platformio/`](platformio/)
- Open the folder in PlatformIO. Run:
  ```bash
  pio run -t upload
  ```

### Experimental ESP-IDF Codebase
- Location: [`esp-idf-experimental/`](esp-idf-experimental/)
- This directory contains an experimental implementation written with native Espressif C++ and CMake.
- It is labelled "unverified" because it has not completed the bench verification protocol on the physical rig.
