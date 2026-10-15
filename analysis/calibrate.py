"""
FT-01 Turbine Flow Meter Calibration Regression Analysis
Lam Research Challenge 3.0 Stage 2 (Team DISTRO - LRC-26-0528)

Fits linear model Q = a * f + b using least-squares regression on physical
experimental rig data loaded from analysis/data/calibration_trials.csv.
"""

import os
import csv
import numpy as np

def load_trials(csv_path=None):
    if csv_path is None:
        base_dir = os.path.dirname(os.path.abspath(__file__))
        csv_path = os.path.join(base_dir, "data", "calibration_trials.csv")

    fit_trials = []
    val_trials = []

    with open(csv_path, "r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            item = {
                "trial": int(row["trial"]),
                "pulses": int(row["pulses"]),
                "time_s": float(row["time_s"]),
                "volume_L": float(row["volume_L"]),
                "role": row.get("role", "fit").strip().lower()
            }
            item["freq_hz"] = item["pulses"] / item["time_s"]
            item["q_actual"] = (item["volume_L"] / item["time_s"]) * 60.0
            if item["role"] == "validation":
                val_trials.append(item)
            else:
                fit_trials.append(item)

    return fit_trials, val_trials

def run_calibration(csv_path=None):
    fit_trials, val_trials = load_trials(csv_path)

    f_fit = np.array([t["freq_hz"] for t in fit_trials])
    q_fit = np.array([t["q_actual"] for t in fit_trials])

    # Least squares linear fit: Q = a * f + b
    p = np.polyfit(f_fit, q_fit, 1)
    a, b = p[0], p[1]

    q_pred = a * f_fit + b
    residuals = q_fit - q_pred
    ss_res = np.sum(residuals**2)
    ss_tot = np.sum((q_fit - np.mean(q_fit))**2)
    r2 = 1.0 - (ss_res / ss_tot)

    err_pct = np.abs(q_pred - q_fit) / q_fit * 100.0
    max_err = np.max(err_pct)
    avg_err = np.mean(err_pct)

    print("=" * 80)
    print("      FT-01 TURBINE FLOW SENSOR CALIBRATION REGRESSION (TASK 2.2)")
    print("      Model: Q (L/min) = a * f (Hz) + b")
    print("=" * 80)
    print(f"{'Trial':<6} | {'Pulses(N)':<10} | {'Time(s)':<8} | {'Freq(Hz)':<10} | {'Vol(L)':<8} | {'Q_act':<9} | {'Q_fit':<9} | {'Error(%)':<8}")
    print("-" * 80)
    for i, t in enumerate(fit_trials):
        print(f"#{t['trial']:<5} | {t['pulses']:<10} | {t['time_s']:<8.3f} | {t['freq_hz']:<10.2f} | {t['volume_L']:<8.3f} | {t['q_actual']:<9.3f} | {q_pred[i]:<9.3f} | {err_pct[i]:<8.2f}")
    print("=" * 80)
    print(f"  Slope (a)                  : {a:.6f} L/min per Hz")
    print(f"  Zero-Offset (b)            : {b:.6f} L/min")
    print(f"  Determination Coeff (R^2)  : {r2:.5f}")
    print(f"  Maximum Percentage Error   : {max_err:.2f} %")
    print(f"  Average Percentage Error   : {avg_err:.2f} %")

    if val_trials:
        print("-" * 80)
        print("  INDEPENDENT VALIDATION TRIAL EVALUATION")
        print("-" * 80)
        for vt in val_trials:
            qv_pred = a * vt["freq_hz"] + b
            ev_pct = abs(qv_pred - vt["q_actual"]) / vt["q_actual"] * 100.0
            print(f"  Validation Trial #{vt['trial']}: Pulses={vt['pulses']}, Time={vt['time_s']:.3f} s, Vol={vt['volume_L']:.3f} L")
            print(f"  Actual Flow: {vt['q_actual']:.3f} L/min | Calibrated: {qv_pred:.3f} L/min | Error: {ev_pct:.2f} %")
    print("=" * 80)

    return a, b, r2, max_err, avg_err

if __name__ == "__main__":
    run_calibration()
