# Hydraulic Fault Analysis & Failure Mode and Effects Analysis (FMEA)

**Project:** Lam Research Challenge 3.0 – Practical Engineering Challenge  
**Team Name:** TEAM DISTRO  
**Team ID:** LRC-26-0528  
**Document Ref:** FMEA-HYD-001  

---

## 1. Overview of Hydraulic Fault Diagnosis

In mission-critical semiconductor fluid distribution systems (UPW, acids, solvents, and CMP slurries), flow anomalies directly impact wafer yield, chemical stoichiometry, and tool safety. Detecting whether an abnormal flow signal represents a genuine process deviation, mechanical pump failure, hydraulic blockage, or instrumentation glitch is a cornerstone of industrial process automation.

---

## 2. Diagnostic Condition Evaluation Matrix (Task 3: Q3.2)

| Condition | Pump Commanded | Valve State | FT-01 Pulse Signal | Measured Flow Rate | Root Physical or Instrumentation Cause | Recommended Corrective Action |
|---|---|---|---|---|---|---|
| **A** | **ON** | **Open** | **Normal** | **Normal** | **Normal Baseline Operation:** System operating at nominal design conditions ($3.0\text{ L/min}$). | Continue standard logging; maintain periodic monitoring. |
| **B** | **ON** | **Partially Closed** | **Reduced** | **Reduced** | **Valve Throttling / Partial Hydraulic Blockage:** Increased friction head loss due to valve restriction, particulate strainer fouling, or kinked 8mm tubing. | Verify ball valve aperture position; inspect in-line filters for particulate debris. |
| **C** | **ON** | **Open** | **Zero** | **Zero** | **Critical Mechanical / Electrical Stall:** Complete pump failure (burnt motor winding, sheared drive shaft, decoupled impeller), dry-running due to air lock, or complete downstream blockage. | Check pump power rail / current draw; verify reservoir fluid levels; inspect pump head. |
| **D** | **OFF** | **Open** | **Zero** | **Zero** | **System Standby / Commanded Idle:** Intentional shutdown state. No motive pressure generated. | Normal operational condition when master switch is OFF or sequence completes. |
| **E** | **ON** | **Open** | **Intermittent** | **Fluctuating** | **Cavitation / Air Entrainment / Vortex Ingestion:** Suction line vortex sucking air into sump pump; low Net Positive Suction Head ($NPSH_A < NPSH_R$); or loose sensor wire causing electrical bounce. | Submerge sump suction inlet below liquid level; check suction fittings for air leaks; verify sensor wiring. |

---

## 3. Resolving the FT-01 Observability Limitation

### The Core Problem:
When the system reports **Zero Flow (0 Hz)** while the pump is commanded ON and valves are OPEN (**Condition C**), **FT-01 alone provides insufficient observability**. Specifically, the system cannot distinguish between:
1. **Real Zero Flow (Pump / Hydraulic Failure):** Fluid has stopped moving because the pump motor is dead, the impeller is stripped, or the line is blocked.
2. **False Zero Flow (Instrumentation Sensor Failure):** Fluid is moving normally, but the FT-01 Hall-effect sensor has suffered an open wire, blown pull-up resistor, or the rotor is mechanically jammed by debris.

*Risk:* If the controller assumes zero flow means the sump is full and keeps pumping, or conversely assumes fluid is missing and runs pumps dry, catastrophic tool damage or hazardous chemical overflow occurs.

### Diagnostic Disambiguation Strategies:
To overcome this observability limit, the following sensor fusion inputs are implemented:

```
                          [Microcontroller / Supervisory FSM]
                                          │
            ┌─────────────────────────────┼─────────────────────────────┐
            ▼                             ▼                             ▼
    [FT-01 Flow Sensor]          [Motor Current Shunt]          [Differential Pressure ΔP]
    Output: 0 Hz (Zero)          Output: I_motor                Output: P_discharge - P_suction
            │                             │                             │
            └─────────────────────────────┼─────────────────────────────┘
                                          │
                                   [FDD Logic Tree]
                                          │
                  ┌───────────────────────┴───────────────────────┐
                  ▼                                               ▼
         [I_motor ≈ 0 A]                                  [I_motor > Normal]
      (Electrical Open / Fuse)                       (Stalled Rotor / Mech Jam)
                  │                                               │
                  ▼                                               ▼
     [I_motor Normal + ΔP > 0]                       [I_motor Normal + ΔP ≈ 0]
      (FT-01 SENSOR FAILURE:                         (PUMP DECOUPLING / AIRLOCK:
       Fluid flowing, sensor dead!)                   Motor spins, no hydraulic head)
```

1. **Motor Current Signature Analysis (MCSA):**
   - $I = 0\text{ A} \implies$ Blown fuse, broken wire, or tripped MOSFET driver.
   - $I > 1.5 \times I_{\text{nominal}} \implies$ Motor rotor locked or mechanical blockage in pump chamber.
   - $I < 0.5 \times I_{\text{nominal}} \implies$ Pump dry-running or impeller sheared off drive shaft.

2. **Differential Pressure ($\Delta P$) Across Pump:**
   - If $\Delta P > 0$ across the pump head while FT-01 reads $0\text{ L/min}$, fluid is pressurized and moving $\implies$ **FT-01 sensor failure confirmed**.
   - If $\Delta P \approx 0$ while motor is powered $\implies$ **Hydraulic line empty or pump disconnected**.

3. **Volumetric Sump Depletion Rate ($\frac{dh}{dt}$):**
   - Correlating the change in liquid level with expected pumped volume creates an independent, non-intrusive check on flow rate:
     $$Q_{\text{actual}} = - A_{\text{sump}} \frac{dh}{dt}$$
