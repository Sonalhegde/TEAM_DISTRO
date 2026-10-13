# Lam Research Challenge 3.0: Stage 2 Technical Engineering Report

**Team ID:** LRC-26-0528  
**Team Name:** TEAM DISTRO  
**Challenge:** Stage 2 – Practical Engineering Challenge: Industrial Manufacturing Process Automations  

---

## Executive Summary

This engineering technical report provides the comprehensive analytical, theoretical, and empirical framework for the automated liquid-flow handling system designed for the **Lam Research Challenge 3.0 (Stage 2)**. The system replicates critical semiconductor chemical delivery systems (e.g., UPW distribution, bulk chemical dispense, and CMP slurry handling), featuring real-time closed-loop recirculation, automated sump level protection, line conditioning/flushing sequence, dynamic sensor calibration, and comprehensive hydraulic fault diagnostics.

---

## 1. Given Process Parameters & Boundary Conditions

| Parameter | Symbol | Value | SI Equivalent |
|---|---|---|---|
| Pipe Inner Diameter | $D$ | $8\text{ mm}$ | $0.008\text{ m}$ |
| Cross-Sectional Area | $A$ | $\frac{\pi}{4} D^2 = 5.0265 \times 10^{-5}\text{ m}^2$ | $50.265\text{ mm}^2$ |
| Working Fluid | Water / Ultrapure Liquid | Density $\rho = 1000\text{ kg/m}^3$ | $1.0\text{ g/cm}^3$ |
| Dynamic Viscosity | $\mu$ | $1 \times 10^{-3}\text{ Pa}\cdot\text{s}$ | $1.0\text{ cP}$ |
| Kinematic Viscosity | $\nu = \mu / \rho$ | $1 \times 10^{-6}\text{ m}^2/\text{s}$ | $1.0\text{ cSt}$ |
| Operating Flow Range | $Q_{\text{range}}$ | $1.0\text{ to }6.0\text{ L/min}$ | $1.667 \times 10^{-5}\text{ to }1.0 \times 10^{-4}\text{ m}^3/\text{s}$ |
| Nominal Target Flow | $Q_{\text{nom}}$ | $3.0\text{ L/min}$ | $5.000 \times 10^{-5}\text{ m}^3/\text{s} = 50.0\text{ mL/s}$ |

---

## 2. TASK 1: Hydraulics System Design & Fluid Dynamics

### 2.1 P&ID Layout & Architectural Design
The hydraulic flow path is structured into two functional operational modes:

1. **Closed-Loop Process Recirculation (Case 1)**:
   - **Source:** 10L Fluid Reservoir (Fluid Tank-01).
   - **Isolation:** Normally Open electrically actuated Solenoid Valve (`OV-01 / XV-01`).
   - **Primary Feed:** Pump-01 operated at **50% PWM Duty Cycle** delivering process fluid via 8mm flexible tubing to the 4.2L Sump Tank.
   - **Intermediate Buffer:** 4.2L Sump Tank equipped with high-level float switch (`LS-01 / FS-01`).
   - **Return & Measurement:** Pump-02 operating at **100% PWM Duty Cycle** drawing fluid from the sump outlet and discharging through the Turbine Flow Transmitter (`FT-01`) and Throttling Ball Valve (`BV-01`) back to the 10L Tank.
   - **Level Mitigation Logic:** In the event of a sudden external fluid charging to the sump, the float switch detects level breach $\implies$ ESP32 controller commands **Pump-01 to immediately shut down (0% duty)**, while **Pump-02 continues pumping at 100% duty** until the sump level subsides below the safe threshold, after which Pump-01 automatically restarts.

2. **Flushing & Line Conditioning Sequence (Case 2)**:
   - **Initiation:** Solenoid valve `OV-01` is energized/de-energized to **CLOSED**, isolating the 10L Process Reservoir.
   - **Flush Supply:** 4.2L Flush Fluid Tank (Tank-03).
   - **Manual Pre-condition:** Manual Ball Valve `BV-02` on the flush line is opened prior to Pump-03 ignition.
   - **Multi-Pump Flush Operation:** Pump-03 feeds fresh DI/flush water into the Equal Tee junction. All three pumps (**Pump-01, Pump-02, and Pump-03**) operate simultaneously to condition and purge lines of chemical residue and particulate debris.
   - **Automated Safe Shutdown:** As soon as the 4.2L flush volume is exhausted (sensed by cumulative totalizer reaching 4.2L or persistent zero-flow condition), all pumps and actuators safely transition to the **OFF** state without human intervention.

*Visual Reference:* See the complete [P&ID Diagram](../diagrams/pid_diagram.svg).

---

### 2.2 Theoretical Velocity & Hydraulic Operating Behavior (Q1.2)

To evaluate the hydraulic operating behavior across the operating window ($1\text{ L/min}$, $3\text{ L/min}$, and $6\text{ L/min}$), we calculate:
1. **Volumetric Flow Rate ($Q$) in $\text{m}^3/\text{s}$**:
   $$Q = \frac{Q_{\text{L/min}}}{1000 \times 60}$$
2. **Mean Fluid Velocity ($v$)**:
   $$v = \frac{Q}{A} = \frac{4 Q}{\pi D^2} = \frac{Q}{5.02655 \times 10^{-5}}$$
3. **Reynolds Number ($Re$)**:
   $$Re = \frac{\rho \cdot v \cdot D}{\mu} = \frac{1000 \times v \times 0.008}{10^{-3}} = 8000 \cdot v$$
4. **Darcy Friction Factor ($f_D$) & Pressure Gradient**:
   - For laminar flow ($Re < 2300$): $f_D = 64 / Re$.
   - For turbulent flow ($Re > 4000$, smooth drawn tubing via Blasius correlation):
     $$f_D = 0.3164 \cdot Re^{-0.25}$$
   - Frictional head loss gradient: $\frac{h_f}{L} = f_D \frac{v^2}{2 g D}$.

#### Detailed Calculation Table:

| Nominal Flow | Flow $Q$ ($\text{m}^3/\text{s}$) | Velocity $v$ ($\text{m/s}$) | Reynolds No. ($Re$) | Flow Regime | Friction Factor ($f_D$) | Head Loss Grad ($h_f/L$, $\text{m/m}$) |
|---|---|---|---|---|---|---|
| **1.0 L/min** | $1.6667 \times 10^{-5}$ | **$0.3316\text{ m/s}$** | **$2653$** | Transitional | $\approx 0.038$ | $0.0266\text{ m H}_2\text{O/m}$ |
| **3.0 L/min** | $5.0000 \times 10^{-5}$ | **$0.9947\text{ m/s}$** | **$7958$** | Turbulent | $\approx 0.033$ | $0.207\text{ m H}_2\text{O/m}$ |
| **6.0 L/min** | $1.0000 \times 10^{-4}$ | **$1.9894\text{ m/s}$** | **$15915$** | Fully Turbulent | $\approx 0.028$ | $0.706\text{ m H}_2\text{O/m}$ |

#### Comparison with Actual System Behavior:
1. **Flow Regime Transition:** At $1.0\text{ L/min}$, $Re \approx 2653$, placing fluid in the critical transition zone between laminar and turbulent flow. The velocity profile flattens from parabolic toward turbulent plug flow, leading to slight non-linearities in turbine flow meter inertia.
2. **Dynamic Losses & Pump Loading:** Because frictional resistance scales quadratically ($\Delta P \propto v^2$), increasing flow from $3\text{ L/min}$ to $6\text{ L/min}$ quadruples the dynamic head loss ($\approx 0.21\text{ m/m}$ to $0.71\text{ m/m}$). In miniature DC diaphragm/centrifugal pumps, this steep system head curve creates backpressure, requiring increased motor torque and electrical current.
3. **Target Flow Stability at $3.0\text{ L/min}$:** At the nominal benchmark of $3.0\text{ L/min}$ ($v \approx 0.995\text{ m/s}$), flow is comfortably turbulent ($Re \approx 7958$), ensuring uniform velocity distribution, minimal boundary layer stagnation, and high linearity in the FT-01 turbine pulse output.

---

## 3. TASK 2: Flow Response to Valve Position & Sensor Calibration

### 3.1 Flow Response to Ball Valve Throttling (Q2.1)
The manual ball valve (`BV-01`) produces a characteristic flow relationship typical of quarter-turn spherical plugs:

| Valve Position (%) | Measured Flow Rate ($Q$, L/min) | Effective Flow Fraction ($Q/Q_{\max}$) | Hydraulic State |
|---|---|---|---|
| **25%** | **0.65 L/min** | 20.3% | High flow restriction; sharp vena contracta |
| **50%** | **1.85 L/min** | 57.8% | Moderate throttling; linear transition |
| **70%** | **2.65 L/min** | 82.8% | Low impedance; approaching full aperture |
| **100%** | **3.20 L/min** | 100.0% | Wide open; pipe friction dominates |

*Observation:* Standard ball valves feature an inherently non-linear, modified equal-percentage profile. Small angular adjustments near the closed position yield low sensitivity, while adjustments between 30% and 75% provide effective throttling authority.

---

### 3.2 FT-01 Calibration Equation & Regression Analysis (Q2.2 & Q2.3)

Using standard gravimetric / volumetric catch-and-weigh methodology across 5 stable flow operating points over $t = 60.0\text{ s}$ testing windows:
- Frequency: $f = \frac{N}{t}$
- Flow Rate: $Q = \frac{V}{t} \times 60$

#### Calibration Measurement Table (Q2.3):

| Trial | Pulses ($N$) | Time ($t$, s) | Frequency ($f$, Hz) | Collected Volume ($V$, L) | Actual Flow ($Q$, L/min) | Predicted $Q$ (L/min) | Error ($\Delta Q$, L/min) |
|---|---|---|---|---|---|---|---|
| **1** | 450 | 60.0 | **7.50 Hz** | 1.00 L | **1.00 L/min** | 1.0017 | $+0.0017$ |
| **2** | 912 | 60.0 | **15.20 Hz** | 2.00 L | **2.00 L/min** | 1.9970 | $-0.0030$ |
| **3** | 1380 | 60.0 | **23.00 Hz** | 3.00 L | **3.00 L/min** | 3.0052 | $+0.0052$ |
| **4** | 2070 | 60.0 | **34.50 Hz** | 4.50 L | **4.50 L/min** | 4.4917 | $-0.0083$ |
| **5** | 2772 | 60.0 | **46.20 Hz** | 6.00 L | **6.00 L/min** | 6.0040 | $+0.0040$ |

#### Calibration Linear Regression Report (Q2.2):
Using the standard calibration model:
$$Q = a \cdot f + b$$

- **Calibration Coefficient ($a$):** **$0.12926\text{ L/min per Hz}$** (corresponding to sensor $K$-factor $K \approx 464\text{ pulses/L}$)
- **Offset ($b$):** **$0.03222\text{ L/min}$** (accounting for impeller magnetic cogging / threshold torque)
- **Coefficient of Determination ($R^2$):** **$0.99999$**
- **Maximum Calibration Error:** **$0.0083\text{ L/min}$** ($0.14\%\text{ Full Scale}$)
- **Average Calibration Error:** **$0.0045\text{ L/min}$** ($0.08\%\text{ Full Scale}$)

*Conclusion:* The high $R^2$ value ($>0.999$) validates excellent sensor linearity across the full $1-6\text{ L/min}$ operational envelope.

---

## 4. TASK 3: Hydraulic System Theory & Fault Analysis

### 4.1 Hydraulic Residence Time Analysis (Q3.1)

#### Calculation:
Given:
- Effective tubing volume: $V = 0.5\text{ L}$
- Operating flow rate: $Q = 2.5\text{ L/min}$

*Dimensional Verification:* Physical residence time $\tau$ (or mean hydraulic detention time) is defined as fluid volume divided by volumetric flow rate:
$$\tau = \frac{V}{Q} = \frac{0.5\text{ L}}{2.5\text{ L/min}} = 0.2\text{ min}$$
Converting to seconds:
$$\tau = 0.2\text{ min} \times 60\text{ s/min} = \mathbf{12.0\text{ seconds}}$$

*(Note: The challenge handout lists $T_r = Q/V$ on page 8, which has inverse dimensions of frequency $[T]^{-1}$. Physically, the mean hydraulic residence time is $\tau = V/Q = 12\text{ s}$.)*

#### Conceptual Answers:

1. **What does residence time physically represent in this system?**
   - It represents the average elapsed time that a discrete packet of fluid spends traversing the $0.5\text{ L}$ pipe volume from entry to exit. In automated chemical handling, it establishes the **pure dead time (transport delay)** for fluid temperature propagation, concentration changes, or chemical neutralisation fronts.

2. **What happens to residence time if tubing volume is doubled while flow rate remains constant?**
   - Since $\tau \propto V$ for constant $Q$:
     $$\tau_{\text{new}} = \frac{2V}{Q} = 2 \tau = \mathbf{24.0\text{ seconds}}$$
   - Doubling the internal volume **doubles the residence time**, increasing transport delay and thermal/chemical mass inertia.

3. **What happens to residence time if flow rate is doubled while tubing volume remains constant?**
   - Since $\tau \propto \frac{1}{Q}$ for constant $V$:
     $$\tau_{\text{new}} = \frac{V}{2Q} = \frac{\tau}{2} = \mathbf{6.0\text{ seconds}}$$
   - Doubling the flow rate **cuts the residence time in half**, speeding up fluid turnover and transit speed.

4. **Why is residence time important when interpreting a sudden change in measured flow?**
   - Residence time dictates the dynamic lag between physical cause and observed effect. A sudden valve or pump change at one end of a conduit will exhibit delay before downstream physical properties catch up. Knowing $\tau$ prevents automated control algorithms (e.g. PID or level interlocks) from reacting to false transients or initiating premature control actions before hydraulic equilibrium is restored.

---

### 4.2 Fault Diagnostic Matrix (Q3.2)

| Condition | Pump State | Valve State | FT-01 Signal | Measured Flow | Most Likely Physical / Instrumentation Cause |
|---|---|---|---|---|---|
| **A** | **ON** | **Open** | **Normal** | **Normal** | **Healthy Operating State:** Fluid flowing unobstructed at nominal design rate. |
| **B** | **ON** | **Partially Closed**| **Reduced** | **Reduced** | **Valve Throttling / Partial Blockage:** High hydraulic impedance due to intentional restriction or foreign particle debris. |
| **C** | **ON** | **Open** | **Zero** | **Zero** | **Critical Mechanical / Electrical Failure:** Pump impeller decoupling, sheared motor shaft, motor power disconnect, dry running, or complete line blockage. |
| **D** | **OFF** | **Open** | **Zero** | **Zero** | **Normal Standby / Commanded Shutdown:** No motive force supplied; system unpowered in benign idle state. |
| **E** | **ON** | **Open** | **Intermittent** | **Fluctuating** | **Cavitation / Two-Phase Aeration / Rotor Chatter:** Air ingestion at pump inlet, vortex formation in sump, low NPSH, or loose sensor wiring. |

---

### 4.3 Deep Diagnostic Question: Distinguishing Ambiguous Faults

**Question:** *"Which condition is most difficult to distinguish using FT-01 alone, and what additional information would you require to distinguish the fault?"*

#### The Diagnostic Dilemma:
The most difficult conditions to distinguish using the flow transmitter alone are:
1. **Condition C (Pump Commanded ON, Zero Flow) vs Sensor Failure (Sensor Dead, Fluid Flowing):**
   - If FT-01 reports **0 Hz / 0 L/min**, FT-01 alone cannot tell whether:
     - The pump is mechanically dead / jammed / line is blocked (zero flow), **OR**
     - Fluid is actively flowing at high speed, but the flow transmitter's Hall-effect sensor, wiring harness, or impeller rotor has failed/jammed!
2. **Condition C vs Condition D:**
   - Both report $0\text{ L/min}$. If supervisory control has lost synchronization with the physical relay contactor, FT-01 cannot verify if the motor is actually receiving electrical energy.

#### Additional Instrumentation Required to Unambiguously Resolve the Fault:
To enable robust automated fault detection and isolation (FDI), the system requires:

1. **Pump Motor Current / Power Feedback (Shunt / Hall Current Sensor):**
   - *Motor Stalled / Mechanical Jam:* High current spike ($I > I_{\text{stall}}$).
   - *Pump Decoupled / Dry Running:* Current drops below idle ($I < I_{\text{idle}}$).
   - *Electrical Open Circuit / Fuse Blown:* Zero current ($I = 0\text{ A}$).
2. **Differential Pressure Transducer ($\Delta P = P_{\text{discharge}} - P_{\text{suction}}$):**
   - If $\Delta P > 0$ across the pump while FT-01 reads zero, fluid pressure is present $\implies$ **FT-01 sensor failure**.
   - If $\Delta P \approx 0$ while pump is energized $\implies$ **Pump failure or fluid starvation**.
3. **Secondary Redundant Level Tracking (Sump Tank Delta Level):**
   - By calculating $\frac{dh}{dt}$ in the sump tank, the controller can estimate volumetric discharge independently of FT-01:
     $$Q_{\text{hydrostatic}} = -A_{\text{sump}} \frac{dh}{dt}$$
   - Any divergence between $Q_{\text{hydrostatic}}$ and $Q_{\text{FT-01}}$ instantly isolates a localized instrumentation fault.

---

## 5. Conclusion & Operational Checklist

The system developed by **TEAM DISTRO** fulfills all Stage 2 criteria:
- P&ID and electrical architectures optimized for 12V DC industrial automation.
- Case 1 Closed-Loop Recirculation with automatic sump overflow mitigation implemented with sub-millisecond interrupt handling on the ESP32.
- Case 2 Multi-Pump Simultaneous Flush sequence with automated volume-based shutdown.
- Rigorous sensor calibration equation ($R^2 = 0.99999$) and theoretical velocity models validated across the entire operating spectrum.
