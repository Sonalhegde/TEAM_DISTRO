# Analysis & Telemetry Tools

This directory contains MATLAB and Python data processing tools for the flow rig.

## Contents
- **`ft01_calibration_plots.m`**: MATLAB script to perform linear regression ($Q = a \cdot f + b$) on pulse frequency data and generate calibration curve plots.
- **`valve_position_plot.m`**: MATLAB script to evaluate manual ball valve (BV-01) throttling characteristics (25%, 50%, 75%, 100%).
- **`calibration_tool.py`**: Python regression solver for FT-01 pulse-to-flow calibration.
- **`generate_plots.py`**: Python script using `matplotlib` to render SVG/PNG figures for engineering documentation.
- **`data_logger.py`**: Real-time serial data logger for command-line DAQ (`python data_logger.py --port COM3`).
- **`requirements.txt`**: Python dependencies (`pyserial`, `numpy`, `matplotlib`).
- **`data/`**:
  - `calibration_trials.csv`: Five-point gravimetric flow calibration dataset.
  - `valve_characterization.csv`: Steady-state flow rates vs. ball valve position.

For step-by-step execution instructions and expected outputs, refer to [`docs/SETUP_ANALYSIS.md`](../docs/SETUP_ANALYSIS.md).
