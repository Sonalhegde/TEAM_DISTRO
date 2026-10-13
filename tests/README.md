# Test Index and Procedures

This directory contains test suites for the simulation engine and the physical hardware bench.

## 1. Simulation Automated Test Suite (TC-01 to TC-16)

Location: [`simulation/`](simulation/)

Run the headless tests using Node.js:
```bash
node tests/simulation/run-tests.js
```

### Test Case Index

| ID | Objective | Input | Expected Result |
|---|---|---|---|
| TC-01 | Verify Case 1 nominal flow rate | P-01 at 50% PWM, P-02 at 100% PWM | FT-01 stabilizes at 3.0 L/min (±5%) |
| TC-02 | Verify stability over 5 minutes | Continuous recirculation | Flow stays bounded, zero fluid spills |
| TC-03 | Verify FT-01 sensor linear calibration | 5 calibration points | $R^2 \ge 0.995$, maximum error $\le 5\%$ |
| TC-04 | Verify calibrated vs datasheet accuracy | Calibration curve applied | Calibrated accuracy exceeds raw factor |
| TC-05 | Verify sump overflow mitigation | Inject 1.5 L fluid into sump | P-01 stops, P-02 evacuates, P-01 restarts |
| TC-06 | Verify cumulative volume totalizer | Fluid transfer to 10 L tank | Totalizer matches volume within $\pm 3\%$ |
| TC-07 | Verify Case 1 to Case 2 transition | Case 1 duration completes | OV-01 closes, pumps stop before P-03 starts |
| TC-08 | Verify Case 2 flush sequence | Open BV-01, start flush | 3 pumps run simultaneously, stops on empty |
| TC-09 | Verify dry-run safety trip | BV-01 closed during flush command | Zero flow detected, safe fault trip triggered |
| TC-10 | Verify emergency stop switch | Master switch turned OFF | All pumps and valves halt within 0.5 s |
| TC-11 | Verify fluid velocity operating points | Setpoints: 1, 3, and 6 L/min | Fluid velocity matches theoretical calculation |
| TC-12 | Verify throttling valve curve | BV-02 at 25%, 50%, 75%, 100% | Monotonic flow progression across operating band |
| TC-13 | Verify calibration data logging | Log 5 experimental trials | Frequency and flow match calibration points |
| TC-14 | Verify residence time equation | $V = 0.5\text{ L}, Q = 2.5\text{ L/min}$ | Residence time calculates to 12.0 s |
| TC-15 | Verify fault observation matrix | Conditions A through E | Simulated readings match diagnostic matrix |
| TC-16 | Verify dry-run prevention with calibration | Calibrated stop threshold | Pump-03 halts without dry running |

## 2. Hardware Bench Test: Pump PWM Test

Location: [`firmware-bench/pump-pwm-test/`](firmware-bench/pump-pwm-test/)

### Procedure
1. Flash [`testcodeforpump.ino`](firmware-bench/pump-pwm-test/testcodeforpump.ino) to the ESP32.
2. The firmware runs the pump at 100% PWM for 20 s.
3. The firmware pauses the pump at 0% PWM for 3 s.
4. The test repeats continuously.
5. Verification date: TO BE CONFIRMED (Date format to be confirmed by maintainer).
