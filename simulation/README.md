# Simulation Package

This directory contains the 3D digital twin simulation of the fluid handling rig.

## Purpose

The simulation models the physical rig, hydraulics, and ESP32 controller state machine.
It allows engineers to test operational procedures and verify safety responses before running the physical testbed.

## File Structure

- [`web/`](web/): Main web application files.
  - `index.html`: Web interface and interactive layout.
  - `sim.js`: Plant hydraulic physics, pulse generation, and controller state engine.
  - `scene.js`: Three.js scene setup, 3D component geometry, and fluid animation.
  - `app.js`: User interface interaction, telemetry charts, and live controls.
- [`vendor/`](vendor/): External libraries.
  - `three.min.js`: Three.js rendering library (runs offline).
- [`archive/`](archive/): Earlier archived simulation build.

## How to Run Offline

1. Open `web/index.html` in any modern web browser.
2. The simulation loads without an internet connection because Three.js is bundled locally.

## Controls and Features

- **Start / Stop:** Use the **Switch ON** and **Switch OFF** buttons to control the rig.
- **External Fluid Injection:** Click **Inject fluid** to introduce liquid to the sump and observe the level safety response.
- **Interactive 3D Navigation:**
  - Orbit: Click and drag with the left mouse button.
  - Pan: Click and drag with the right mouse button.
  - Zoom: Scroll with the mouse wheel.
