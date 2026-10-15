"""
Live Serial Data Logger & Telemetry Monitor for Lam Research Challenge 3.0
Team: TEAM DISTRO (LRC-26-0528)

Parses the production 13-field CSV telemetry frame emitted between '/*' and '*/'
and logs records to a local CSV file while printing status and state transitions.
"""

import sys
import time
import csv
import argparse
from datetime import datetime

try:
    import serial
except ImportError:
    serial = None

FRAME_COLUMNS = [
    "iso_timestamp",
    "time_s",
    "pulses",
    "window_s",
    "freq_hz",
    "q_lmin",
    "q_mls",
    "total_l",
    "total_pulses",
    "run_pulses",
    "run_time_s",
    "run_freq_hz",
    "q_actual_lmin",
    "error_pct"
]

def run_logger(port, baudrate=115200, output_csv="telemetry_log.csv"):
    print("=" * 70)
    print("      TEAM DISTRO - LIVE SERIAL TELEMETRY LOGGER & DAQ")
    print("      Frame: 13-field delimited /*time_s,pulses,...,error_pct*/")
    print("=" * 70)
    print(f"Connecting to {port} @ {baudrate} baud...")

    if serial is None:
        print("[ERROR] pyserial is not installed. Run: pip install pyserial")
        sys.exit(1)

    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        time.sleep(1.5)
        print(f"[OK] Port connected. Logging data to '{output_csv}'...\n")
    except Exception as e:
        print(f"[ERROR] Failed to open port {port}: {e}")
        return

    with open(output_csv, mode="w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(FRAME_COLUMNS)

        print(f"{'TIME (s)':<10} | {'PULSES':<8} | {'FREQ (Hz)':<10} | {'FLOW (L/min)':<13} | {'TOTAL (L)':<11} | {'RUN PULSES':<11} | {'ERR (%)':<8}")
        print("-" * 80)

        try:
            while True:
                raw_line = ser.readline().decode("utf-8", errors="ignore").strip()
                if not raw_line:
                    continue

                # Detect telemetry frame wrapped in /* and */
                if raw_line.startswith("/*") and raw_line.endswith("*/"):
                    payload = raw_line[2:-2].strip()
                    parts = payload.split(",")
                    if len(parts) >= 13:
                        now_iso = datetime.now().isoformat()
                        row = [now_iso] + [p.strip() for p in parts[:13]]
                        writer.writerow(row)
                        f.flush()

                        try:
                            t_s = float(parts[0])
                            p_win = int(parts[1])
                            f_hz = float(parts[3])
                            q_lpm = float(parts[4])
                            tot_l = float(parts[6])
                            run_p = int(parts[8])
                            err_p = float(parts[12])
                            print(f"{t_s:<10.2f} | {p_win:<8d} | {f_hz:<10.2f} | {q_lpm:<13.3f} | {tot_l:<11.4f} | {run_p:<11d} | {err_p:<8.2f}")
                        except (ValueError, IndexError):
                            print(f"[FRAME] {payload}")
                else:
                    # System log, FSM state change or diagnostic message
                    print(f"  {raw_line}")
        except KeyboardInterrupt:
            print("\n[STOP] Logging stopped by operator. Telemetry saved to", output_csv)
        finally:
            ser.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="TEAM DISTRO Flow Telemetry Logger")
    parser.add_argument("--port", type=str, default="COM3", help="Serial COM port (e.g. COM3 or /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--output", type=str, default="telemetry_log.csv", help="Output CSV filename")
    args = parser.parse_args()

    run_logger(args.port, args.baud, args.output)
