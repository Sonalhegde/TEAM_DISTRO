/*
================================================================================
  LAM RESEARCH CHALLENGE 3.0 - STAGE 2
  ESP32-WROOM-32 / ESP32-DEVKITC | MOTOR 1 / PUMP FULL-CAPACITY PWM TEST
  Successfully tested at 2/10/2026

  WIRING (BTS7960 Motor Driver):
    L_EN1  -> GPIO13 | R_EN1  -> GPIO26 (Driver bridge enable pins)
    L_PWM1 -> GPIO32 | R_PWM1 -> GPIO33 (Driver PWM & direction pins)
    LED    -> GPIO2  (ESP32 onboard status LED)

  OPERATION:
    Power ON -> Pump ON 100% PWM (20s) -> Pump OFF (3s) -> Repeat forever
================================================================================
*/

// Hardware Pin Definitions
#define L_EN1    13              // Left bridge enable pin
#define R_EN1    26              // Right bridge enable pin
#define L_PWM1   32              // Left PWM pin
#define R_PWM1   33              // Right PWM pin
#define LED_PIN  2               // ESP32 onboard status LED pin

// PWM Configuration (Arduino-ESP32 Core 3.x)
#define PWM_FREQ 1000            // PWM frequency: 1 kHz
#define PWM_RES  8               // Resolution: 8-bit (range 0 - 255)
#define MOTOR_PWM 255            // Full capacity: 100% duty cycle (255)

// Cycle Timings
#define MOTOR_ON_TIME  20000UL   // Pump ON duration = 20 seconds
#define MOTOR_OFF_TIME 3000UL    // Pump OFF duration = 3 seconds

void motorON() {
  ledcWrite(L_PWM1, 0);          // Drive low-side PWM to 0
  ledcWrite(R_PWM1, MOTOR_PWM);  // Drive high-side PWM to full speed (255)
  digitalWrite(LED_PIN, HIGH);   // Turn ON status LED
}

void motorOFF() {
  ledcWrite(R_PWM1, 0);          // Stop high-side PWM output
  ledcWrite(L_PWM1, 0);          // Stop low-side PWM output
  digitalWrite(LED_PIN, LOW);    // Turn OFF status LED
}

void setup() {
  pinMode(L_EN1, OUTPUT);        // Set enable pins as outputs
  pinMode(R_EN1, OUTPUT);
  digitalWrite(L_EN1, HIGH);     // Enable left half-bridge
  digitalWrite(R_EN1, HIGH);     // Enable right half-bridge

  pinMode(LED_PIN, OUTPUT);      // Configure onboard LED
  digitalWrite(LED_PIN, LOW);    // Initialize LED in OFF state

  ledcAttach(L_PWM1, PWM_FREQ, PWM_RES); // Attach L_PWM1 pin to PWM channel
  ledcAttach(R_PWM1, PWM_FREQ, PWM_RES); // Attach R_PWM1 pin to PWM channel

  motorOFF();                    // Ensure motor starts in OFF state
  delay(100);                    // Brief driver stabilization delay
  motorON();                     // Start pump immediately at 100% PWM
}

void loop() {
  delay(MOTOR_ON_TIME);          // Run pump at maximum PWM for 20 seconds
  motorOFF();                    // Stop pump
  delay(MOTOR_OFF_TIME);         // Rest pump for 3 seconds
  motorON();                     // Restart pump at maximum PWM
}
