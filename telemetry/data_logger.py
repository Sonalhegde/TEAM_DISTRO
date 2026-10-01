"""
Live Serial Data Logger & Telemetry Monitor for Lam Research Challenge 3.0
Team: TEAM DISTRO (LRC-26-0528)
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

def run_logger(port, baudrate=115200, output_csv="telemetry_log.csv"):
    print("=" * 60)
    print("  TEAM DISTRO - LIVE DAQ & TELEMETRY MONITOR")
    print("=" * 60)
    print(f"Connecting to {port} @ {baudrate} baud...")

    if serial is None:
        print("[ERROR] pyserial is not installed. Run: pip install pyserial")
        sys.exit(1)

    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        time.sleep(2) # Wait for ESP32 reset
        print(f"[OK] Connected. Logging data to '{output_csv}'...\n")
    except Exception as e:
        print(f"[ERROR] Failed to open port {port}: {e}")
        return

    with open(output_csv, mode="w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["Timestamp_ISO", "ESP_Time_ms", "State_ID", "Freq_Hz", "Flow_Lpm", "Flow_Mls", "Total_Liters", "Float_Active"])
        
        print(f"{'TIME':<10} | {'STATE':<14} | {'FREQ (Hz)':<10} | {'FLOW (L/min)':<12} | {'FLOW (mL/s)':<12} | {'TOTAL (L)':<10}")
        print("-" * 78)

        state_names = {
            0: "IDLE",
            1: "CASE1_NORMAL",
            2: "CASE1_ALARM",
            3: "CASE2_FLUSH",
            4: "SAFE_SHUTDOWN"
        }

        try:
            while True:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
                if line.startswith("TELEMETRY"):
                    parts = line.split(",")
                    if len(parts) >= 8:
                        _, t_ms, state_id, freq, q_lpm, q_mls, total_l, fsw = parts
                        now_iso = datetime.now().isoformat()
                        writer.writerow([now_iso, t_ms, state_id, freq, q_lpm, q_mls, total_l, fsw])
                        f.flush()

                        s_name = state_names.get(int(state_id), "UNKNOWN")
                        print(f"{float(t_ms)/1000.0:>9.2f}s | {s_name:<14} | {float(freq):>9.2f}  | {float(q_lpm):>11.3f}  | {float(q_mls):>11.2f}  | {float(total_l):>9.3f} L")
                elif line:
                    print(f" [ESP32 LOG] {line}")
        except KeyboardInterrupt:
            print("\n[STOP] Logging terminated by user. Data successfully saved.")
        finally:
            ser.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="TEAM DISTRO Flow Telemetry Logger")
    parser.add_argument("--port", type=str, default="COM3", help="Serial port (e.g. COM3 or /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--output", type=str, default="telemetry_log.csv", help="Output CSV filename")
    args = parser.parse_args()

    run_logger(args.port, args.baud, args.output)
