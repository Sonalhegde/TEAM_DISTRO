# Data Analysis and MATLAB Plotting Guide

This guide describes how to run the offline data analysis scripts for the **TEAM DISTRO** flow rig. It covers sensor calibration regressions and valve throttling characterization.

---

## 1. System Requirements and Environment

### Supported Environments
- **MATLAB**: R2020a, R2021a/b, R2022a/b, R2023a/b, R2024a or newer.
  - Required toolboxes: MATLAB Base (no specialized toolboxes required; uses built-in `readtable`, `polyfit`, `polyval`).
- **GNU Octave (Open-Source Alternative)**: Version 6.0 or newer.

### Repository Data Paths
```text
TEAM_DISTRO/
├── analysis/
│   ├── ft01_calibration_plots.m      MATLAB script: Flow sensor calibration fit
│   ├── valve_position_plot.m         MATLAB script: Ball valve characteristic curve
│   └── data/
│       ├── calibration_trials.csv    Empirical pulse, time, frequency, and volume data
│       └── valve_characterization.csv Throttling flow rates across valve positions
└── media/                            Output folder for generated PNG plots
```

---

## 2. Running FT-01 Sensor Calibration Analysis (`ft01_calibration_plots.m`)

This script fits a linear calibration equation ($\dot{Q} = a \cdot f + b$) to measured calibration trial data using ordinary least squares.

### Expected Input
File: `analysis/data/calibration_trials.csv`
Columns:
- `trial_id`: Trial sequence number (1 to 6)
- `pulses`: Total pulses counted by hardware PCNT ($N$)
- `duration_s`: Test duration in seconds ($t$)
- `freq_hz`: Average frequency ($f = N / t$)
- `volume_L`: Measured fluid collection in litres ($V$)
- `actual_flow_Lpm`: Reference flow rate ($Q = V / t \times 60$)
- `k_factor_pulses_per_L`: Empirical K-factor ($K = N / V$)

### Steps to Run in MATLAB GUI
1. Open MATLAB.
2. In the **Current Folder** toolbar, navigate to the `analysis/` folder inside `TEAM_DISTRO`.
3. In the Current Folder pane, double-click `ft01_calibration_plots.m` to open it in the **Editor**.
4. Click the green **Run** button (or press `F5`).

### Steps to Run from MATLAB Command Line
```matlab
cd('analysis');
ft01_calibration_plots;
```

### Steps to Run from Terminal / Shell (Batch Mode)
- **Windows**:
  ```powershell
  matlab -batch "run('analysis/ft01_calibration_plots.m')"
  ```
- **GNU Octave**:
  ```bash
  octave --no-gui analysis/ft01_calibration_plots.m
  ```

### Expected Output
The script prints the regression metrics to the Command Window:
```text
=== FT-01 Calibration Results ===
Slope a: 0.00924 L/min/Hz
Intercept b: 0.04011 L/min
R^2: 0.99668
Max Error: 0.0412 L/min (0.69% FS)
Avg Error: 0.0189 L/min (0.32% FS)
Plot saved to: .../TEAM_DISTRO/media/sensor_calibration_curve.png
```

### Verify it worked
1. Open `media/sensor_calibration_curve.png`.
2. Verify that the plot displays the red scatter points representing the physical catch-and-weigh trials, with the dashed blue linear regression trendline showing $R^2 \ge 0.995$.

---

## 3. Running Ball Valve Characterization Plotter (`valve_position_plot.m`)

This script plots flow rate as a function of manual ball valve (BV-01) handle position (from 25% throttled to 100% full bore).

### Expected Input
File: `analysis/data/valve_characterization.csv`
Columns:
- `valve_pos_pct`: Valve opening percentage (25%, 50%, 75%, 100%)
- `flow_rate_Lpm`: Measured steady-state flow rate in L/min

### Steps to Run in MATLAB GUI
1. In the MATLAB Editor or Current Folder toolbar, open `valve_position_plot.m`.
2. Click the green **Run** button (or press `F5`).

### Steps to Run from MATLAB Command Line
```matlab
cd('analysis');
valve_position_plot;
```

### Steps to Run from Terminal / Shell (Batch Mode)
- **Windows**:
  ```powershell
  matlab -batch "run('analysis/valve_position_plot.m')"
  ```
- **GNU Octave**:
  ```bash
  octave --no-gui analysis/valve_position_plot.m
  ```

### Expected Output
The Command Window displays:
```text
Plot saved to: .../TEAM_DISTRO/media/valve_characteristic_curve.png
```

### Verify it worked
1. Open `media/valve_characteristic_curve.png`.
2. Confirm the curve shows monotonic flow increase:
   - 25% position $\rightarrow$ 1.62 L/min
   - 50% position $\rightarrow$ 2.45 L/min
   - 75% position $\rightarrow$ 2.88 L/min
   - 100% position $\rightarrow$ 3.02 L/min

---

## 4. Python Telemetry Utilities (Optional)

In addition to MATLAB scripts, the repository provides Python 3 CLI utilities for real-time serial logging and plotting:

```bash
cd analysis
pip install -r requirements.txt

# Log live telemetry frames from ESP32 to CSV:
python data_logger.py --port COM3 --baud 115200 --output run_data.csv

# Generate Matplotlib plots from logged CSV:
python generate_plots.py --input run_data.csv
```

### Verify it worked
A timestamped `.csv` file and corresponding `.png` figures are generated in the current working directory.
