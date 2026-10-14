# Documentation Index

This directory contains the engineering documentation, system design specifications, and user guides for the **TEAM DISTRO** liquid-handling and recirculation platform.

## Setup and Operation Guides

- [Repository Setup and Git Guide](SETUP_REPO.md)  
  Git clone, pull, branching workflow, commit rules, and conflict resolution guide for student developers.
- [ESP32 Firmware Build and Flashing Guide](SETUP_ESP32.md)  
  Step-by-step setup for ESP-IDF, VS Code, PlatformIO, Arduino compatibility analysis, USB-UART drivers, calibration flashing, and Serial Studio dashboard.
- [Data Analysis and MATLAB Plotting Guide](SETUP_ANALYSIS.md)  
  Execution instructions for MATLAB scripts (`ft01_calibration_plots.m` and `valve_position_plot.m`), expected datasets, and Python utilities.

## Engineering Reports and Specifications

- [Technical Engineering Report](technical-report.md)  
  Full engineering report with hydraulic derivations, fluid velocity calculations, and fault analysis.
- [Piping and Instrumentation Specification](pid-design-specification.md)  
  Design specification for all sensors, actuators, and operational states.
- [Flow Sensor Calibration and Valve Analysis](calibration-and-analysis.md)  
  Calibration equations, regression solver results, and valve throttling characteristics.
- [Failure Mode and Effects Analysis (FMEA)](fault-analysis-fmea.md)  
  Risk assessment matrix and root-cause troubleshooting table for conditions A through E.
- [Hardware Wiring and Commissioning Guide](hardware-wiring-guide.md)  
  Electrical connection table, ESP32 GPIO pinout, and step-by-step cold-check sequence.
- [Complete Testing Procedure Manual](COMPLETE_TESTING_PROCEDURE.md)  
  Physical rig testing checklist, Case 1 & Case 2 validation protocol, and CLI diagnostics.

## Figures and Schematics

- Electrical schematics, P&ID diagrams, and state machine diagrams are located in [`diagrams/`](diagrams/).
- Generated plots and calibration curve figures are located in [`../media/`](../media/).
