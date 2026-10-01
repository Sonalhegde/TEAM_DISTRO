"""
Interactive Calibration Tool for FT-01 Flow Transmitter
Solves Q = a*f + b and reports R^2, max error, and avg error per Lam Challenge Task 2.
"""
import numpy as np

def run_calibration(trials_data=None):
    print("=" * 65)
    print("  FT-01 SENSOR EXPERIMENTAL CALIBRATION REGRESSION SOLVER")
    print("  Model: Q = a * f + b  (Q in L/min, f in Hz)")
    print("=" * 65)

    if trials_data is None:
        # Default experimental data collected from calibrated rig
        trials_data = [
            {"trial": 1, "pulses": 450, "time_s": 60.0, "vol_L": 1.00},
            {"trial": 2, "pulses": 912, "time_s": 60.0, "vol_L": 2.00},
            {"trial": 3, "pulses": 1380, "time_s": 60.0, "vol_L": 3.00},
            {"trial": 4, "pulses": 2070, "time_s": 60.0, "vol_L": 4.50},
            {"trial": 5, "pulses": 2772, "time_s": 60.0, "vol_L": 6.00},
        ]

    f_list = []
    q_list = []

    print(f"{'Trial':<6} | {'Pulses (N)':<11} | {'Time (s)':<9} | {'Freq f (Hz)':<12} | {'Vol V (L)':<10} | {'Flow Q (L/min)':<14}")
    print("-" * 75)

    for item in trials_data:
        f = item["pulses"] / item["time_s"]
        q = (item["vol_L"] / item["time_s"]) * 60.0
        f_list.append(f)
        q_list.append(q)
        print(f"{item['trial']:<6} | {item['pulses']:<11} | {item['time_s']:<9.1f} | {f:<12.2f} | {item['vol_L']:<10.2f} | {q:<14.2f}")

    f_arr = np.array(f_list)
    q_arr = np.array(q_list)

    # Perform Linear Regression
    a, b = np.polyfit(f_arr, q_arr, 1)
    q_pred = a * f_arr + b
    residuals = q_arr - q_pred

    ss_res = np.sum(residuals**2)
    ss_tot = np.sum((q_arr - np.mean(q_arr))**2)
    r2 = 1.0 - (ss_res / ss_tot)
    max_err = np.max(np.abs(residuals))
    avg_err = np.mean(np.abs(residuals))

    print("\n" + "=" * 65)
    print("                 CALIBRATION REPORT RESULTS")
    print("=" * 65)
    print(f"  R^2 (Coefficient of Determination) = {r2:.5f}")
    print(f"  a (Calibration Coefficient)        = {a:.5f} L/min/Hz")
    print(f"  b (Offset / Zero Error)            = {b:.5f} L/min")
    print(f"  Maximum Calibration Error          = {max_err:.5f} L/min ({max_err/6.0*100:.2f}% FS)")
    print(f"  Average Calibration Error          = {avg_err:.5f} L/min ({avg_err/6.0*100:.2f}% FS)")
    print("=" * 65)
    print(f"  CALIBRATION EQUATION:  Q = {a:.5f} * f + ({b:.5f})")
    print("=" * 65)

    return a, b, r2, max_err, avg_err

if __name__ == "__main__":
    run_calibration()
