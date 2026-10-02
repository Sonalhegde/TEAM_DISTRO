# Unverified Test - Lam Research Challenge 3.0 (Stage 2)

ESP32 (NodeMCU-32S) controller state machine firmware from "LAM Stage 2 PCB" schematic Rev 1.0:
- **Case 1: Control + Measurement** (Sump float switch HIGH -> open SOV, PI flow control to 3 L/min, totalize volume, float LOW -> stop).
- **Case 2: Flush + Shutdown** (SOV open, all 3 pumps run together, flow exhaustion detection -> pumps stop -> SOV closes -> OFF).
- **Tactile Switch Controls (G34)**:
  - IDLE: Short press = Case 1, Long press = Case 2
  - RUNNING: Any press = Stop everything
  - FAULT: Any press = Reset
- **Serial 115200 Commands**: `s1`, `s2`, `x`, `sp <L/min>`, `ppl <pulses/L>`, `cal0`, `cal <litres>`, `rt`, `?`
