# Test Codes - Lam Research Challenge 3.0 (Stage 2)

This folder contains standalone diagnostic and hardware bring-up sketches for individual rig components.

## Available Tests

### 1. 	estcodeforpump (	estcodeforpump/testcodeforpump.ino)
- **Target:** Feed Centrifugal Pump (Motor 1 / P-01) via BTS7960 H-Bridge driver.
- **Microcontroller:** ESP32-WROOM-32 / ESP32-DevKitC (Arduino-ESP32 Core 3.x).
- **Capability:** Full capacity 100% PWM duty cycle test.
- **Duty Cycle Sequence:** 20s Full ON (PWM 255) -> 3s OFF (PWM 0) -> Loop.
- **Verification Status:** Successfully tested on **2/10/2026**.
