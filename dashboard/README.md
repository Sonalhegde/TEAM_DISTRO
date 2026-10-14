# Serial Studio Telemetry Dashboard

This directory houses the live dashboard profile for **Serial Studio Data Acquisition (DAQ)**.

## Contents
- **`serial_studio_project.json`**: Official Serial Studio configuration mapping the ESP32 telemetry frames to interactive dashboard widgets.

## Monitored Metrics
- **Instantaneous Flow Rate (L/min)**: Circular dial gauge ($0.0 - 6.0\text{ L/min}$).
- **Instantaneous Flow Rate (mL/s)**: Real-time time-series trend line ($0 - 100\text{ mL/s}$).
- **Total Transferred Volume**: Volumetric progress bar ($0 - 10\text{ L}$).
- **FT-01 Pulse Frequency**: Pulse frequency line chart ($0 - 500\text{ Hz}$).
- **Sump High Level Alarm**: Digital status indicator ($0 = \text{Normal}, 1 = \text{Alarm}$).
- **Controller State ID**: Deterministic FSM state code.

For configuration and connection steps, see [`docs/SETUP_ESP32.md`](../docs/SETUP_ESP32.md#5-opening-the-serial-studio-dashboard).
