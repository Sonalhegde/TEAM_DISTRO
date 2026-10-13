/*
================================================================================
  TEAM DISTRO - SENSOR TEST BENCH
  ESP8266 (NodeMCU / Wemos D1 Mini) | YF-S401 Flow Sensor Test Code
  Serial Studio Telemetry Streamer + Optional I2C OLED Hardware Display

  HARDWARE CONNECTIONS:
    Sensor: YF-S401 Micro Water Flow Sensor (0.3 - 6.0 L/min)
    - RED Wire    -> 3.3V (VCC) or 5V (VIN)
    - BLACK Wire  -> GND
    - YELLOW Wire -> ESP8266 D5 (GPIO14) [Hall pulse output]

    Optional Physical OLED Display (0.96" I2C SSD1306 128x64):
    - VCC -> 3.3V
    - GND -> GND
    - SCL -> ESP8266 D1 (GPIO5)
    - SDA -> ESP8266 D2 (GPIO4)

  YF-S401 CALIBRATION SPECIFICATION:
    - Standard formula: F (Hz) = 98 * Q (L/min)
    - Flow Rate Q (L/min) = F / 98.0
    - Pulses per Liter = 98 * 60 = 5,880 pulses/Liter (~5.88 pulses/mL)
    
  SERIAL STUDIO TELEMETRY FORMAT:
    /*<time_ms>,<freq_hz>,<flow_lpm>,<flow_mls>,<total_l>,<total_ml>,<total_pulses>,<flow_state>*\r\n
================================================================================
*/

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Optional Physical Hardware Display Configuration (0.96" I2C OLED SSD1306)
// Set to 1 if you have an SSD1306 OLED connected (SDA=D2, SCL=D1).
// Requires "Adafruit SSD1306" and "Adafruit GFX Library" installed in Arduino IDE.
// -----------------------------------------------------------------------------
#define ENABLE_OLED_DISPLAY 0

#if ENABLE_OLED_DISPLAY
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C  // Typical I2C address: 0x3C or 0x3D

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif

// Pin Configuration: D5 is GPIO14 on ESP8266
#ifndef D5
#define D5 14
#endif

const uint8_t FLOW_SENSOR_PIN = D5;

// Calibration & Sensor Constants
const float CALIBRATION_FACTOR = 98.0f;                       // F = 98 * Q (L/min)
const float PULSES_PER_LITER   = CALIBRATION_FACTOR * 60.0f;  // 5880 pulses/L

// Telemetry Timing Configuration
const unsigned long TELEMETRY_INTERVAL_MS = 500;              // Emit every 500 ms (2 Hz)

// Pulse Accumulation Variables (volatile for ISR access)
volatile unsigned long isrPulseCount = 0;       // Pulses in current sampling window
volatile unsigned long isrTotalPulseCount = 0;  // Cumulative lifetime pulses

// Timing Variables
unsigned long lastMeasureTime = 0;

// Cumulative Total Volume (Liters)
float totalVolumeLiters = 0.0f;

/**
 * @brief Interrupt Service Routine (ISR) for flow pulse detection.
 * Marked with IRAM_ATTR to ensure execution directly from ESP8266 IRAM.
 */
void IRAM_ATTR onFlowPulse() {
  isrPulseCount++;
  isrTotalPulseCount++;
}

#if ENABLE_OLED_DISPLAY
void renderOledDisplay(float freqHz, float flowLpm, float totalL) {
  display.clearDisplay();
  
  // Header
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("YF-S401 FLOW SENSOR"));
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  // Large Frequency Display in Hz
  display.setCursor(0, 15);
  display.print(F("FREQ: "));
  display.setTextSize(2);
  display.print(freqHz, 1);
  display.setTextSize(1);
  display.print(F(" Hz"));

  // Flow Rate
  display.setCursor(0, 36);
  display.print(F("FLOW: "));
  display.print(flowLpm, 3);
  display.print(F(" L/min"));

  // Total Volume
  display.setCursor(0, 50);
  display.print(F("VOL : "));
  display.print(totalL, 3);
  display.print(F(" L"));

  display.display();
}
#endif

void setup() {
  // Initialize Serial interface for Serial Studio DAQ
  Serial.begin(115200);
  delay(100);

  // Configure sensor pin as INPUT with internal pull-up
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);

  // Attach interrupt to detect falling edge of Hall-effect pulses
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), onFlowPulse, FALLING);

#if ENABLE_OLED_DISPLAY
  // Wire.begin(SDA, SCL) -> D2 (GPIO4), D1 (GPIO5) on ESP8266
  Wire.begin(4, 5);
  if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 20);
    display.println(F("YF-S401 DAQ INIT"));
    display.setCursor(10, 35);
    display.println(F("TEAM DISTRO"));
    display.display();
    delay(1000);
  }
#endif

  lastMeasureTime = millis();

  // Startup banner
  Serial.println();
  Serial.println(F("# ESP8266 YF-S401 Flow Sensor DAQ Initialized on Pin D5 (GPIO14)"));
  Serial.println(F("# Baud: 115200 | Telemetry: Serial Studio Frame Format (/*...*/)\r\n"));
}

void loop() {
  unsigned long currentMillis = millis();

  // Non-blocking sampling interval
  if (currentMillis - lastMeasureTime >= TELEMETRY_INTERVAL_MS) {
    unsigned long interval = currentMillis - lastMeasureTime;
    lastMeasureTime = currentMillis;

    // Critical Section: Atomically snapshot and reset interval pulse counter
    noInterrupts();
    unsigned long windowPulses = isrPulseCount;
    isrPulseCount = 0;
    unsigned long lifetimePulses = isrTotalPulseCount;
    interrupts();

    // 1. Calculate Frequency (Hz = pulses per second)
    float frequencyHz = (windowPulses * 1000.0f) / (float)interval;

    // 2. Calculate Flow Rate: Q (L/min) = Frequency (Hz) / 98.0
    float flowRateLpm = frequencyHz / CALIBRATION_FACTOR;

    // Zero-clamping to avoid noise when pump/fluid is stopped
    if (frequencyHz < 0.1f) {
      frequencyHz = 0.0f;
      flowRateLpm = 0.0f;
    }

    // 3. Flow Rate in mL/s = (L/min * 1000) / 60
    float flowRateMls = (flowRateLpm * 1000.0f) / 60.0f;

    // 4. Cumulative Total Volume
    totalVolumeLiters = (float)lifetimePulses / PULSES_PER_LITER;
    float totalVolumemL = totalVolumeLiters * 1000.0f;

    // 5. Flow Active Status (0 = Idle, 1 = Flowing)
    int flowState = (flowRateLpm > 0.02f) ? 1 : 0;

    // =========================================================================
    // Emit Telemetry Frame for Serial Studio
    // Format: /*<time>,<freq>,<lpm>,<mls>,<total_l>,<total_ml>,<pulses>,<state>*/
    // =========================================================================
    Serial.print(F("/*"));
    Serial.print(currentMillis);
    Serial.print(F(","));
    Serial.print(frequencyHz, 2);
    Serial.print(F(","));
    Serial.print(flowRateLpm, 3);
    Serial.print(F(","));
    Serial.print(flowRateMls, 2);
    Serial.print(F(","));
    Serial.print(totalVolumeLiters, 3);
    Serial.print(F(","));
    Serial.print(totalVolumemL, 1);
    Serial.print(F(","));
    Serial.print(lifetimePulses);
    Serial.print(F(","));
    Serial.print(flowState);
    Serial.println(F("*/"));

#if ENABLE_OLED_DISPLAY
    renderOledDisplay(frequencyHz, flowRateLpm, totalVolumeLiters);
#endif
  }
}
