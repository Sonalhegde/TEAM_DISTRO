# Flow Sensor Calibration & Hydraulic Valve Characterization

**Project:** Lam Research Challenge 3.0 – Practical Engineering Challenge  
**Team Name:** TEAM DISTRO  
**Team ID:** LRC-26-0528  

---

## 1. Principles of Turbine Flow Sensing (FT-01)

The FT-01 flow transmitter utilizes an internal multi-bladed rotor embedded with permanent magnets. As liquid passes through the 8 mm flow nozzle, the fluid momentum exerts hydrodynamic torque on the blades, spinning the turbine at an angular velocity directly proportional to mean flow velocity:
$$\omega \propto v_{\text{fluid}}$$

A solid-state Hall-effect sensor on the external casing detects the passage of each magnetic pole, generating a 5V square wave output frequency:
$$f = \frac{N}{t} \quad [\text{Hz}]$$

Where:
- $N$ is the total pulse count detected by the microcontroller interrupt service routine.
- $t$ is the elapsed collection window in seconds.

---

## 2. Experimental Calibration Protocol (Task 2: Q2.2 & Q2.3)

### Calibration Model Formulation
A standard affine linear response model is fitted to account for turbine mechanical inertia and boundary layer shear:
$$Q = a \cdot f + b$$

Where:
- $Q$ = Volumetric flow rate in $\text{L/min}$
- $f$ = Measured pulse frequency in $\text{Hz}$
- $a$ = Calibration slope coefficient ($\text{L/min per Hz}$)
- $b$ = Zero-flow intercept / threshold offset ($\text{L/min}$)

### Experimental 5-Point Calibration Dataset
Data collected using a calibrated $1000\text{ mL}$ Class-A graduated cylinder and precision digital stopwatch ($t = 60.0\text{ s}$ per test point):

| Trial | Pulse Count ($N$) | Duration ($t$, s) | Frequency ($f = N/t$, Hz) | Collected Vol ($V$, L) | Actual Flow ($Q = V/t \cdot 60$, L/min) | Fitted $Q_{\text{model}}$ (L/min) | Residual Error $\varepsilon_i$ (L/min) |
|---|---|---|---|---|---|---|---|
| **1** | 450 | 60.0 | **7.50** | 1.00 | **1.00** | 1.0017 | $+0.0017$ |
| **2** | 912 | 60.0 | **15.20** | 2.00 | **2.00** | 1.9970 | $-0.0030$ |
| **3** | 1380 | 60.0 | **23.00** | 3.00 | **3.00** | 3.0052 | $+0.0052$ |
| **4** | 2070 | 60.0 | **34.50** | 4.50 | **4.50** | 4.4917 | $-0.0083$ |
| **5** | 2772 | 60.0 | **46.20** | 6.00 | **6.00** | 6.0040 | $+0.0040$ |

### Regression Parameters
- **Slope ($a$):** $0.12926\text{ L/min/Hz}$
- **Intercept ($b$):** $0.03222\text{ L/min}$
- **$R^2$ Statistic:** $0.99999$
- **Maximum Calibration Error:** $0.0083\text{ L/min}$ ($0.14\%\text{ Full Scale}$)
- **Average Calibration Error:** $0.0045\text{ L/min}$ ($0.08\%\text{ Full Scale}$)

![Sensor Calibration Curve](../assets/sensor_calibration_curve.png)

---

## 3. Valve Flow Coefficient & Characteristic Curve (Task 2: Q2.1)

To evaluate flow control authority, the manual ball valve (`BV-01`) was set to four distinct mechanical positions while recording steady-state flow:

| Ball Valve Aperture (%) | Measured Flow ($Q$, L/min) | Flow Fraction ($Q / Q_{100\%}$) | Hydraulic Behavior |
|---|---|---|---|
| **25%** | **0.65 L/min** | 20.3% | Heavy orifice throttling; high pressure drop |
| **50%** | **1.85 L/min** | 57.8% | Transition zone; linear valve authority |
| **70%** | **2.65 L/min** | 82.8% | Moderate restriction |
| **100%** | **3.20 L/min** | 100.0% | Wide open; system piping resistance limits flow |

![Valve Characteristic Curve](../assets/valve_characteristic_curve.png)

---

## 4. Totalization & Flow vs Time Dynamic Trends

In Case 1, continuous telemetry logging demonstrated both steady state target flow maintenance and instantaneous level fault mitigation:

![Flow vs Time Trend](../assets/flow_vs_time_sample.png)

- **0 to 10s:** Startup ramp to target $3.0\text{ L/min}$ ($50.0\text{ mL/s}$).
- **10 to 45s:** Stable closed-loop recirculation.
- **45 to 65s:** External fluid disturbance injected into sump. LS-01 activates $\implies$ Pump-01 cut off $\implies$ Pump-02 drains surcharge at full rate.
- **65s onwards:** Sump level restored to normal $\implies$ Pump-01 restarts and steady state is re-established.
