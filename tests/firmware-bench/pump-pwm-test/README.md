# Pump PWM Diagnostic Test

This directory contains the standalone hardware diagnostic sketch for testing the feed pump.

## Target Hardware

- Target Component: Feed Pump (Pump-01)
- Motor Driver: BTS7960 H-Bridge driver module
- Microcontroller: ESP32-WROOM-32 / NodeMCU-32S
- Sketch: [`testcodeforpump.ino`](testcodeforpump.ino)

## Test Sequence

1. The controller commands 100% PWM duty cycle (PWM value 255) for 20 s.
2. The controller commands 0% PWM duty cycle (PWM value 0) for 3 s.
3. The cycle repeats continuously.

## Verification Status

- Result: PASS.
- Test Date: TO BE CONFIRMED (Date format to be confirmed by maintainer).
