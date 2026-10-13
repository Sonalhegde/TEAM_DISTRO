# ESP8266 + YF-S401 Flow Sensor Bring-Up & Serial Studio Telemetry

Standalone test sketch and Serial Studio dashboard project for evaluating the **YF-S401 Micro Water Flow Sensor** on an **ESP8266** microcontroller (NodeMCU, Wemos D1 Mini, ESP-12E/F).

---

## 1. Hardware Pinout & Wiring

| YF-S401 Sensor Wire | ESP8266 Pin | Notes / Description |
| :--- | :--- | :--- |
| **RED (VCC)** | **VIN (5V)** or **3.3V** | Power rail (operating voltage range: 3.5V – 12V DC). |
| **BLACK (GND)** | **GND** | Ground reference |
| **YELLOW (Signal)** | **D5 (GPIO14)** | Hall-effect square-wave pulse output |

> [!WARNING]
> **Logic Level Advisory:**
> If you power the YF-S401 with **5V**, its pulse output will transition up to 5V. To prevent over-voltage on the ESP8266 GPIO (3.3V rated), either:
> 1. Power the sensor directly from the **3.3V pin** (many YF-S401 units run reliably down to 3.3V), **OR**
> 2. Use a simple resistor voltage divider on the yellow wire (e.g., 1kΩ series + 2kΩ to GND).

---

## 2. Sensor Flow Calculation

The YF-S401 pulse rate characteristics:
- **Formula:** $F (\text{Hz}) = 98 \times Q (\text{L/min})$
- **Flow Rate:** $Q (\text{L/min}) = \frac{F}{98.0}$
- **Flow Rate (mL/s):** $Q (\text{mL/s}) = \frac{Q (\text{L/min}) \times 1000}{60} \approx Q \times 16.667$
- **Pulses per Liter:** $98 \times 60 = 5,880 \text{ pulses/L}$ (~$5.88 \text{ pulses/mL}$)

---

## 3. Telemetry Output Format

Frames are transmitted over Serial (UART) at **115200 baud** every **500 ms**:

```text
/*<Time_ms>,<Freq_Hz>,<Flow_Lpm>,<Flow_Mls>,<Total_L>,<Total_mL>,<Total_Pulses>,<Flow_State>*/\r\n
```

### Dataset Index Mapping:
1. **Time Since Boot:** Runtime since reset (`ms`).
2. **Pulse Frequency:** Real-time pulse rate (`Hz`).
3. **Flow Rate (L/min):** Volumetric flow rate (`0.0 - 6.0 L/min`).
4. **Flow Rate (mL/s):** High-resolution instantaneous flow (`0.0 - 100.0 mL/s`).
5. **Total Transferred Volume:** Total liquid passed (`0.0 - 10.0 Liters`).
6. **Total Volume (mL):** Cumulative liquid volume (`mL`).
7. **Raw Pulse Count:** Total Hall pulses captured (`pulses`).
8. **Flow Active State:** Flow state flag (`0` = IDLE, `1` = ACTIVE FLOW).

---

## 4. How to Import and Monitor in Serial Studio

1. **Install Serial Studio:**
   - Download the latest installer from [Serial Studio Releases](https://github.com/Serial-Studio/Serial-Studio/releases).
2. **Flash the ESP8266:**
   - Open [`yfs401_esp8266_test.ino`](file:///d:/TEAM_DISTRO/test%20codes/yfs401_esp8266_test/yfs401_esp8266_test.ino) in Arduino IDE.
   - Select Board: **NodeMCU 1.0 (ESP-12E Module)** or **LOLIN(WEMOS) D1 R2 & mini**.
   - Select COM Port and Upload.
3. **Open Project in Serial Studio:**
   - Launch Serial Studio.
   - Click the **Project** menu at top (or press `Ctrl+O` / click **Open Project**).
   - Select the JSON configuration:
     ```text
     d:\TEAM_DISTRO\test codes\yfs401_esp8266_test\yfs401_serial_studio.json
     ```
4. **Connect to ESP8266:**
   - In Serial Studio's left sidebar, go to **Connection Settings**.
   - Select **Serial Port**.
   - Choose your ESP8266 **COM Port** (e.g. `COM3` / `COM4`).
   - Set **Baud Rate:** `115200`.
   - Data bits: `8`, Parity: `None`, Stop bits: `1`, Flow Control: `None`.
   - Click **Connect** (or the Play / Connect icon).
5. **View Dashboard Widgets:**
   - **Dashboard Tab:** Watch the circular dial gauge for Flow Rate (L/min), bar meter for Total Volume, live status LED, and real-time numeric readouts.
   - **Plot Tab:** Live scrolling waveforms for Pulse Frequency (Hz) and Flow Rate (mL/s).
   - **Console Tab:** Raw ASCII telemetry frames (`/*...*/`).
