# CHANGES - Core Logic Revision (main/main.cpp)

**Project:** Lam Research Challenge 3.0, Stage 2 (TEAM DISTRO)
**File changed:** `unverified/main/main.cpp` (only file modified)
**Scope:** Core control and measurement logic only. Hardware pinout was confirmed working against the PCB schematic and was not redesigned.

> Status: these edits have **not yet been compiled, flashed or bench-tested**. Work through the checklist in section 5 before relying on them.

---

## 1. Intended system workflow (what the code now implements)

| Step | State | Solenoid OV-01 | Pump-1 | Pump-2 | Pump-3 | Notes |
|---|---|---|---|---|---|---|
| 0 | `IDLE` | Closed | OFF | OFF | OFF | Waiting for the push button |
| 1 | `CASE1_NORMAL` | Open | 50% | 100% | OFF | Starts on a short press. P1 fills the sump from the 10 L tank, P2 drains it through FT-01 back to the tank. Valve tests (25/50/75/100%) are done here with the manual ball valve. Totalizer is zeroed at start. |
| 2 | `CASE1_SURGE` | Open | OFF | 100% | OFF | External fluid raises the sump until the float closes. Pump-1 stops, Pump-2 keeps draining. |
| 3 | `CASE1_NORMAL` (resumed) | Open | 50% | 100% | OFF | Float must stay clear for 3 s, then Pump-1 restarts (no totalizer reset). Runs for the recovery dwell (15 s). |
| 4 | `CASE1_DRAIN` (new) | **Closed** | OFF | 100% | OFF | Sump is emptied completely. Done when FT-01 flow stays below 0.3 L/min for 3 s. |
| 5 | `CASE2_FLUSHING` | Closed | 50% | 100% | 100% | All three pumps run. Flush water arrival is detected as flow rising from ~0. Stops when flow collapses for 4 s (flush tank empty). |
| 6 | `CASE2_STOPPING` -> `IDLE` | Closed | OFF | OFF | OFF | Safe shutdown |

A short press at any time stops everything. A press held for 1.5 s is an emergency stop. The switch is a **momentary push button**, so the existing press/long-press logic is unchanged.

**Why the drain step exists:** the float is only a high-level switch. When it releases, the sump still holds most of its process fluid. Starting Case 2 at that moment would mix flush water with process fluid and make "flush tank finished" detection unreliable. The paper also requires Pump-1 to restart after the level drops, so the drain happens after a short recovery dwell.

---

## 2. Bugs fixed

| # | Problem | Fix |
|---|---|---|
| 1 | **Pulse-counter wrap.** Wrap correction added 65536, but the counter clears at its limit (true wrap is 32768). Each wrap (~110 s at 3 L/min) injected ~32k phantom pulses, i.e. a ~5 L spike in `total_L`. | Wrap now adds `PCNT_HIGH_LIMIT + 1` (32768). Limits are named constants. |
| 2 | **Phantom flow at zero pulses.** `FLOW_CUTOFF_HZ` was unused, so a non-zero offset `b` (after APPLY) reported flow with no pulses and fed the totalizer. | Noise gate: below `FLOW_CUTOFF_HZ` (1 Hz) flow is reported as 0. |
| 3 | **Calibration pulse count error.** `S`/`E` snapshotted the once-per-second `total_pulses`, losing up to ~1 s of pulses (1-3% on a 30-60 s run) while time used microsecond timestamps. | `S` and `E` read the PCNT count directly. The live `run_pulses` in the telemetry frame uses the same source. |
| 4 | **Case 2 early-stop risk.** Residual Case 1 flow satisfied "flush detected" at once, so the priming fault could never fire and the sump could empty before flush water arrived, stopping the system early. | Resolved by the drain step: flow starts near 0 in Case 2, so flow above 0.3 L/min genuinely means flush water arrived. The 25 s priming fault now catches a closed flush-line ball valve. |
| 5 | **Wrong stop trigger.** The 4.2 L volume count was the stop trigger, but it counted residual process fluid and ignores the ~4.5 L physical tank size. | Primary stop is flow collapse (4 s). Volume is only a backup ceiling (`FLUSH_CEILING_L` = 5.0 L). |
| 6 | **Pump-1 short-cycling.** Pump-1 restarted the instant the float cleared; the level then rose and tripped the float again. | Float must read clear for `SURGE_CLEAR_CONFIRM_MS` (3 s) before restart. |
| 7 | **Blocking delay on surge recovery.** Recovery called the full start routine including `delay(200)`. | New `resumeCase1AfterSurge()` (no delay, no reset). `startCase1Normal()` is only for fresh starts. |
| 8 | **Totalizer polluted by earlier runs.** `total_L` counted from boot, including calibration runs. | Zeroed (`total_L`, `totalL`, `totalFlushL`) when a fresh Case 1 starts. Not zeroed on surge recovery. |
| 9 | **Premature jump to Case 2.** Case 2 started 8 s after the float released, with the sump still full. | Replaced by recovery dwell then drain step (section 1). |
| 10 | **APPLY always claimed success.** "VERIFIED: error dropped" printed unconditionally. | Old error is recomputed from the old constants, and the message is conditional. Least squares minimises absolute residuals, so % error can rise. |
| 11 | **`status` "Window N" was always 0.** | Now prints the pulses counted in the last telemetry window (`lastWindowN`). |
| 12 | **GPIO25 conflict.** `PIN_SV_ALT` (25) is R_PWM3 on the schematic, so opening the solenoid drove a pump-driver input. | Removed `PIN_SV_ALT` (and unused `PIN_FS_ALT`). Solenoid is on GPIO27 only. |
| 13 | **R_PWM2 / R_PWM3 floating at boot.** Only the L_PWM pins were configured. | GPIO19 and GPIO25 are set as outputs and held LOW at startup. |

---

## 3. New and changed parameters

| Constant | Value | Meaning |
|---|---|---|
| `C1_RECOVERY_DWELL_MS` | 15000 | Normal Case 1 recirculation time after surge recovery, before draining |
| `C1_DRAIN_EMPTY_MS` | 3000 | Flow below `FLOW_MIN_VALID` for this long means the sump is empty |
| `C1_DRAIN_MAX_MS` | 120000 | Drain timeout. Advances to Case 2 anyway with a warning |
| `SURGE_CLEAR_CONFIRM_MS` | 3000 | Float must stay clear this long before Pump-1 restarts |
| `FLUSH_CEILING_L` | 5.00 | Backup volume ceiling in Case 2 (replaces `FLUSH_TANK_CAP_L`) |
| `PCNT_LOW_LIMIT` / `PCNT_HIGH_LIMIT` | -32768 / 32767 | Pulse counter limits (named constants) |
| `ST_CASE1_DRAIN` | 6 | New state, also added to the `status` name table |

Removed: `FLUSH_TANK_CAP_L`, `PIN_SV_ALT`, `PIN_FS_ALT`. `C2_HOLDOFF_MS` was added during the review and removed again once the drain step made it unnecessary.

---

## 4. Behaviour not changed

- Push-button handling (short press toggles start/stop, 1.5 s hold is an emergency stop).
- Pump duty cycles (P1 50%, P2 100% in Case 1; Case 2 is P1 50%, P2 100%, P3 100%) and PWM setup.
- Serial Studio frame format (13 fields) and the calibration CLI (`S`, `E`, `V`, `FIT`, `APPLY`).
- Pin assignments for everything except the GPIO25 solenoid alias.

---

## 5. Bench verification checklist

1. **Counter wrap:** run 3 minutes at ~3 L/min and confirm `total_L` has no jumps (this assumes the counter clears to 0 at its limit).
2. **Calibration:** run `S`/`E`/`V` for 5 flows. Check run pulses against a manual pulse count.
3. **Zero-flow gate:** after `APPLY`, with pumps off, flow and totalizer must read 0.
4. **Surge:** pour until the float closes. Pump-1 stops, Pump-2 keeps running, and Pump-1 restarts ~3 s after the float clears (no rapid cycling).
5. **Recovery dwell:** is 15 s long enough for the video? Adjust `C1_RECOVERY_DWELL_MS` if not.
6. **Drain:** confirm FT-01 reads below 0.3 L/min when Pump-2 runs dry, and the drain advances in a few seconds (not the 2-minute timeout).
7. **Case 2:** with the flush-line ball valve open, flow should rise from ~0 and the system should stop a few seconds after the flush tank empties. With the valve closed, the priming fault should trip at 25 s.
8. **Pump dry-run tolerance:** Pump-2 runs dry briefly at the end of the drain and during flush priming. Confirm the pump tolerates this.
9. **Button:** short press stops from every state, including the drain.

---

## 6. Known leftovers (not changed)

- Dead or unused code: `masterInterlockArmed` (gates nothing), `C1_GRACE_MS`, `C1_NOFLOW_FAULT_MS`, `ppl` and its CLI command, `demo` trial data, and the GPIO14/34/35 interrupt handlers. Strip these before submission.
- GPIO12 is driven as a second Pump-1 enable. It is a boot-strapping pin, so remove it unless it is proven necessary.
- No float guard during Case 2 (cutting Pump-1 and Pump-3 if the float trips again). The paper does not require it.
- If no surge is ever poured, the sequence never advances to Case 2.
- `README.md` in this folder still describes older behaviour (PI flow control, `sp` / `cal0` commands, 500 ms frames, 20 kHz PWM). Update it. Also confirm `telemetry/serial_studio_project.json` matches the 13-field frame the firmware now prints.
- The build resolves `arduino-esp32 3.0.0` on ESP-IDF 5.2.7 with the version check bypassed. This builds but is unsupported, so pin the versions.
