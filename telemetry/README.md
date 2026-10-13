# Telemetry and Data Acquisition (DAQ)

This directory contains Python tools to collect real-time serial telemetry and generate experimental plots.

## 1. Installation

Install the required Python dependencies:
```bash
pip install -r requirements.txt
```

## 2. Real-Time Data Logger

Run the serial logger to record live flow data to a CSV file:
```bash
python data_logger.py --port COM3
```

Replace `COM3` with the active serial port of your ESP32 board.

## 3. Sensor Calibration Regression Tool

Run the regression solver to compute calibration coefficients $a$ and $b$ from bucket test data:
```bash
python calibration_tool.py
```

## 4. Plot Generator

Generate publication figures from recorded telemetry:
```bash
python generate_plots.py
```

The output plots save to `docs/figures/`.

## 5. Serial Studio Real-Time Dashboard

Use Serial Studio to visualize real-time dials and telemetry graphs:

1. Download and install Serial Studio from [https://serial-studio.github.io/](https://serial-studio.github.io/).
2. Open Serial Studio.
3. Click **Project Settings** (gear icon).
4. Click **Import JSON** and select [`serial_studio_project.json`](serial_studio_project.json).
5. Select your COM port and set the baud rate to **115200**.
6. Click **Connect** to display live dials and flow rate trends.
