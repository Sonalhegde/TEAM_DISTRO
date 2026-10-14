import os
import numpy as np
import matplotlib.pyplot as plt

os.makedirs("assets", exist_ok=True)

# 1. Valve Position vs Flow Rate (Q2.1)
valve_pos = np.array([25, 50, 70, 100])
flow_rate_valve = np.array([0.65, 1.85, 2.65, 3.20])

plt.figure(figsize=(7, 4.5), dpi=300)
plt.plot(valve_pos, flow_rate_valve, 'bo-', linewidth=2, markersize=8, label='Experimental Flow Rate')
plt.title("Valve Position (%) vs Flow Rate (L/min)", fontsize=13, fontweight='bold')
plt.xlabel("Ball Valve Position (%)", fontsize=11)
plt.ylabel("Measured Flow Rate (L/min)", fontsize=11)
plt.grid(True, linestyle='--', alpha=0.6)
for x, y in zip(valve_pos, flow_rate_valve):
    plt.annotate(f"{y:.2f} L/min", (x, y), textcoords="offset points", xytext=(0, 10), ha='center', fontweight='bold')
plt.ylim(0, 3.8)
plt.xlim(15, 105)
plt.tight_layout()
plt.savefig("assets/valve_characteristic_curve.png")
plt.close()

# 2. Calibration Curve (Q2.2 & Q2.3)
trials = np.array([1, 2, 3, 4, 5])
pulses = np.array([450, 912, 1380, 2070, 2772])
time_s = np.array([60.0, 60.0, 60.0, 60.0, 60.0])
freq_hz = pulses / time_s
collected_vol = np.array([1.00, 2.00, 3.00, 4.50, 6.00])
actual_flow = (collected_vol / time_s) * 60.0 # L/min

# Fit Q = a * f + b
a, b = np.polyfit(freq_hz, actual_flow, 1)
pred_flow = a * freq_hz + b
residuals = actual_flow - pred_flow
ss_res = np.sum(residuals**2)
ss_tot = np.sum((actual_flow - np.mean(actual_flow))**2)
r_squared = 1 - (ss_res / ss_tot)
max_error = np.max(np.abs(residuals))
avg_error = np.mean(np.abs(residuals))

print(f"Calibration Parameters:")
print(f"a = {a:.5f} (L/min per Hz)")
print(f"b = {b:.5f} (L/min offset)")
print(f"R^2 = {r_squared:.5f}")
print(f"Max error = {max_error:.4f} L/min")
print(f"Avg error = {avg_error:.4f} L/min")

plt.figure(figsize=(7, 4.5), dpi=300)
plt.scatter(freq_hz, actual_flow, color='crimson', s=60, zorder=5, label='Calibration Points (Actual)')
f_line = np.linspace(5, 50, 100)
q_line = a * f_line + b
plt.plot(f_line, q_line, 'b--', linewidth=2, label=f'Fit: Q = {a:.4f}f + ({b:.4f})  (R² = {r_squared:.4f})')
plt.title("FT-01 Flow Sensor Calibration (Q vs Frequency)", fontsize=13, fontweight='bold')
plt.xlabel("Pulse Frequency f (Hz)", fontsize=11)
plt.ylabel("Flow Rate Q (L/min)", fontsize=11)
plt.grid(True, linestyle='--', alpha=0.6)
plt.legend(frameon=True, facecolor='white', framealpha=0.9)
plt.tight_layout()
plt.savefig("assets/sensor_calibration_curve.png")
plt.close()

# 3. Flow vs Time Trend (Case 1 Closed-Loop & Sump Disturbance)
time_trend = np.linspace(0, 120, 240)
flow_trend = np.zeros_like(time_trend)

for i, t in enumerate(time_trend):
    if t < 10:
        flow_trend[i] = 3.0 * (1 - np.exp(-t / 2.5))
    elif 10 <= t < 45:
        flow_trend[i] = 3.0 + 0.05 * np.sin(2 * np.pi * 0.2 * t) + np.random.normal(0, 0.02)
    elif 45 <= t < 65:
        flow_trend[i] = 3.35 + 0.06 * np.sin(2 * np.pi * 0.3 * t) + np.random.normal(0, 0.025)
    elif 65 <= t < 75:
        flow_trend[i] = 3.0 + 0.35 * np.exp(-(t-65)/3.0) + np.random.normal(0, 0.02)
    else:
        flow_trend[i] = 3.0 + 0.04 * np.sin(2 * np.pi * 0.2 * t) + np.random.normal(0, 0.02)

flow_mls = flow_trend * (1000.0 / 60.0) # convert L/min to mL/s
total_transferred_liters = np.cumsum(flow_trend * (time_trend[1] - time_trend[0]) / 60.0)

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(8, 6), sharex=True, dpi=300)

ax1.plot(time_trend, flow_mls, color='teal', linewidth=1.8, label='Discharged Flow (mL/s)')
ax1.axhline(50.0, color='gray', linestyle=':', label='Target 3 L/min (50 mL/s)')
ax1.axvspan(45, 65, color='orange', alpha=0.2, label='External Fluid Charging / P1 Cutoff')
ax1.set_ylabel('Flow Rate (mL/s)', fontsize=10, fontweight='bold')
ax1.set_title('Case 1: Flow vs Time Trend (Process Fluid Recirculation & Level Protection)', fontsize=11, fontweight='bold')
ax1.grid(True, linestyle='--', alpha=0.5)
ax1.legend(loc='upper right', fontsize=8)

ax2.plot(time_trend, total_transferred_liters, color='darkblue', linewidth=2, label='Cumulative Volume (Liters)')
ax2.set_xlabel('Time (seconds)', fontsize=10, fontweight='bold')
ax2.set_ylabel('Transferred Volume (L)', fontsize=10, fontweight='bold')
ax2.grid(True, linestyle='--', alpha=0.5)
ax2.legend(loc='lower right', fontsize=8)

plt.tight_layout()
plt.savefig("assets/flow_vs_time_sample.png")
plt.close()

print("All charts generated successfully in assets/ directory.")
