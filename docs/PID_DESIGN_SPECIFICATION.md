# Piping & Instrumentation Design (P&ID) Specification

**Project:** Lam Research Challenge 3.0 – Practical Engineering Challenge  
**Team Name:** TEAM DISTRO  
**Team ID:** LRC-26-0528  
**Document Ref:** P&ID-SPEC-001  

---

## 1. System Overview & Piping Architecture

The liquid-handling automation system is engineered for dual-mode operation:
1. **Normal Closed-Loop Recirculation with Dynamic Level Regulation (Case 1)**
2. **Automated Line Purging / Conditioning with Safe Shutdown (Case 2)**

All primary process lines utilize **8 mm flexible tubing** with **10 mm to 8 mm reducers** on pump inlet/outlet ports to minimize pressure drop and prevent cavitation.

---

## 2. Equipment Tagging & Instrument Schedule

### 2.1 Mechanical Equipment
| Tag | Item Description | Capacity / Rating | Process Function |
|---|---|---|---|
| **TK-01** | Primary Process Fluid Tank | $10.0\text{ Litres}$ (Transparent) | Stores bulk process fluid; receives recirculated return flow. |
| **TK-02** | Intermediate Sump Reservoir | $4.2\text{ Litres}$ (Transparent) | Buffer basin; receives flow from TK-01 and external liquid disturbances. |
| **TK-03** | Flush Fluid Reservoir | $4.2\text{ Litres}$ (Transparent) | Stores deionized flush water for line conditioning and purging. |
| **P-01** | Feed Pump (Pump-1) | 12V DC, $1-6\text{ L/min}$ | Delivers process fluid from TK-01 to TK-02; operates at **50% PWM Duty Cycle**. |
| **P-02** | Return Pump (Pump-2) | 12V DC, $1-6\text{ L/min}$ | Discharges fluid from TK-02 back to TK-01; operates at **100% PWM Duty Cycle**. |
| **P-03** | Flush Pump (Pump-3) | 12V DC, $1-6\text{ L/min}$ | Delivers flush liquid from TK-03 into the main distribution circuit during Case 2. |
| **OV-01** | Solenoid Isolation Valve (XV-01) | 12V DC Normally Open/Closed | Automatically isolates TK-01 during flush sequence. |
| **BV-01** | Manual Ball Valve 1 | 8 mm Full-Bore Spherical | Throttling valve on return line to establish target $3.0\text{ L/min}$ setpoint. |
| **BV-02** | Manual Ball Valve 2 | 8 mm Full-Bore Spherical | Manual isolation valve on the flush line before P-03 suction. |
| **TEE-01** | 8 mm Equal Tee Fitting | 3-Way $8\text{ mm}$ Ports | Combines the TK-01 supply line and TK-03 flush line into P-01 suction. |

### 2.2 Instrumentation & Electrical Controls
| Tag | Instrument | Signal / Range | Purpose |
|---|---|---|---|
| **FT-01** | Turbine Flow Transmitter | $0.5 - 50\text{ Hz}$ Pulse / $5\text{V}$ | Measures instantaneous flow rate and cumulative volume discharged to TK-01. |
| **LS-01** | Sump Level Float Switch | Dry Contact (NO / Pull-up) | Detects high liquid level threshold in TK-02 (external charging event). |
| **SW-01** | Master On/Off Switch | SPST Toggle Switch | Hardware enable/disable for system operation. |
| **U7** | ESP32 NodeMCU Module | 32-bit Dual-Core 240MHz | Executes real-time FreeRTOS control loops, interrupt routines, and telemetry. |
| **PCB-01** | Custom Control PCB | 12V DC Bus / SMPS Powered | Power distribution, MOSFET/H-Bridge drivers, and opto-isolated signal conditioning. |

---

## 3. Operational Modes & Flow Path Logic

### 3.1 Case 1: Closed-Loop Recirculation Path
```
[TK-01 10L Tank] 
      │
      ▼
[OV-01 Solenoid Valve] (OPEN)
      │
      ▼
[TEE-01 Equal Tee]
      │
      ▼
[Pump-01] (50% PWM Duty)
      │
      ▼
[TK-02 Sump Tank 4.2L]  <--- [External Fluid Charge Inflow]
      │                     [LS-01 High Level Float Switch]
      ▼
[Pump-02] (100% PWM Duty)
      │
      ▼
[FT-01 Flow Transmitter] (Telemetry & Totalization)
      │
      ▼
[BV-01 Throttle Ball Valve]
      │
      ▼
(Return to [TK-01 10L Tank])
```

#### Level Interlock Mechanism:
- Under nominal conditions, Pump-01 (50%) and Pump-02 (100%) maintain a steady state equilibrium.
- When an external fluid disturbance charges TK-02, liquid level rises until **LS-01 trips**.
- The ESP32 triggers an internal interrupt:
  - **Pump-01 instantly cuts off (0% PWM)**.
  - **Pump-02 remains at full power (100% PWM)**, rapidly evacuating the surcharge back to TK-01.
  - Once level subsides below LS-01 hysteresis band, **Pump-01 safely resumes 50% PWM**.

---

### 3.2 Case 2: Flushing Sequence & Safe Shutdown
```
[TK-01] ───X─── [OV-01 Solenoid] (CLOSED: 10L Tank ISOLATED)

[TK-03 Flush Tank 4.2L]
      │
      ▼
[BV-02 Ball Valve] (Manually OPENED prior to initiation)
      │
      ▼
[Pump-03] (ON: Flush Delivery)
      │
      ▼
[TEE-01 Equal Tee]
      │
      ├──> [Pump-01] (ON) ──> [TK-02 Sump] ──> [Pump-02] (ON) ──> [FT-01] ──> [Discharge / Drain]
```

- During Case 2, **all three pumps (P-01, P-02, P-03)** run simultaneously.
- Fluid cleans the intermediate tubing, pump chambers, and flow sensor.
- When the 4.2L flush reservoir is depleted, the sensor detects zero pulses and the totalizer logs 4.2L $\implies$ **System transitions into SAFE OFF**.
