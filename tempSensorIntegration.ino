// ============================================================
//  Fan + Servo Controller with Dual Temperature Sensors
//  - Button manually cycles fan modes (off > low > med > full)
//  - Inside sensor auto-enables fan if temp exceeds threshold
//  - Outside sensor is logged for reference
// ============================================================

#include <Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>


// ── PINS ────────────────────────────────────────────────────
const int buttonPin   = 7;
const int servoPin    = 5;
const int fanPin      = 3;
const int insidePin   = 2;   // Inside sensor
const int outsidePin  = 4;   // Outside sensor


// ── TEMPERATURE SETTINGS ────────────────────────────────────
const float TEMP_ON_THRESHOLD  = 80.0;  // °F — auto-enable fan above this
const float TEMP_OFF_THRESHOLD = 75.0;  // °F — auto-disable fan below this (hysteresis)
const int   TEMP_READ_INTERVAL = 2000;  // ms between temperature reads


// ── FAN MODES ───────────────────────────────────────────────
//  { servoAngle, fanSpeed }
//  servoAngle : 0 = closed, 90 = open
//  fanSpeed   : 0 = off, 255 = full power
int modeMatrix[][2] = {
  {0,   0  },  // Mode 0: closed, fan off
  {90,  255}   // Mode 1: open,   fan full
};
const int totalModes = sizeof(modeMatrix) / sizeof(modeMatrix[0]);


// ── STATE VARIABLES ─────────────────────────────────────────
int  manualMode      = 0;      // Mode selected by the button
int  activeMode      = 0;      // Mode actually applied (may be overridden by temp)
bool autoOverride    = false;  // True when temp control has taken over
bool lastButtonState = HIGH;
unsigned long lastTempRead = 0;

float tempInside  = 0.0;
float tempOutside = 0.0;


// ── OBJECTS ─────────────────────────────────────────────────
Servo servo;

OneWire insideWire(insidePin);
OneWire outsideWire(outsidePin);
DallasTemperature insideSensors(&insideWire);
DallasTemperature outsideSensors(&outsideWire);


// ── FUNCTION DECLARATIONS ───────────────────────────────────
void  applyMode(int m);
void  moveServo(int targetAngle);
void  readTemperatures();
void  checkAutoControl();


// ============================================================
void setup() {
  Serial.begin(9600);
  insideSensors.begin();
  outsideSensors.begin();

  servo.attach(servoPin);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(fanPin, OUTPUT);

  applyMode(0);
  Serial.println("System initialized.");
}


// ============================================================
void loop() {

  // 1. Read temperatures on a timed interval
  if (millis() - lastTempRead >= TEMP_READ_INTERVAL) {
    lastTempRead = millis();
    readTemperatures();
    checkAutoControl();
  }

  // 2. Check for button press (HIGH → LOW transition)
  bool buttonState = digitalRead(buttonPin);
  if (buttonState == LOW && lastButtonState == HIGH) {
    delay(50); // debounce

    manualMode = (manualMode + 1) % totalModes;
    Serial.print("Button pressed — manual mode: ");
    Serial.println(manualMode);

    // Only apply manual mode if auto-override is not active,
    // or if the user is choosing a higher mode than the override
    if (!autoOverride || manualMode > activeMode) {
      applyMode(manualMode);
    }
  }
  lastButtonState = buttonState;
}


// ============================================================
//  Read both sensors and print to Serial
// ============================================================
void readTemperatures() {
  insideSensors.requestTemperatures();
  outsideSensors.requestTemperatures();

  tempInside  = insideSensors.getTempFByIndex(0);
  tempOutside = outsideSensors.getTempFByIndex(0);

  if (tempInside  == DEVICE_DISCONNECTED_F) Serial.println("WARNING: Inside sensor disconnected!");
  if (tempOutside == DEVICE_DISCONNECTED_F) Serial.println("WARNING: Outside sensor disconnected!");

  Serial.print("Inside: ");  Serial.print(tempInside);  Serial.print(" °F  |  ");
  Serial.print("Outside: "); Serial.print(tempOutside); Serial.println(" °F");
}


// ============================================================
//  Auto-enable or disable fan based on inside temperature
// ============================================================
void checkAutoControl() {
  if (tempInside == DEVICE_DISCONNECTED_F) return; // Don't act on bad data

  if (!autoOverride && tempInside >= TEMP_ON_THRESHOLD) {
    // Temp too high — kick on Mode 1 (full)
    autoOverride = true;
    Serial.println("Auto ON: inside temp exceeded threshold.");
    if (activeMode < 1) applyMode(1);

  } else if (autoOverride && tempInside <= TEMP_OFF_THRESHOLD) {
    // Cooled down — return to whatever the button had set
    autoOverride = false;
    Serial.println("Auto OFF: inside temp back to normal.");
    applyMode(manualMode);
  }
}


// ============================================================
//  Apply a mode: set servo angle and fan speed
// ============================================================
void applyMode(int m) {
  activeMode = m;
  int targetAngle = modeMatrix[m][0];
  int fanSpeed    = modeMatrix[m][1];

  Serial.print("Applying mode "); Serial.print(m);
  Serial.print(" — angle: "); Serial.print(targetAngle);
  Serial.print(", fan: ");    Serial.println(fanSpeed);

  analogWrite(fanPin, fanSpeed);
  moveServo(targetAngle);
}


// ============================================================
//  Move servo one degree at a time (smooth, controlled speed)
// ============================================================
void moveServo(int targetAngle) {
  const int servoSpeed = 50; // ms per degree — higher = slower
  int currentAngle = servo.read();

  int step = (targetAngle > currentAngle) ? 1 : -1;
  while (currentAngle != targetAngle) {
    currentAngle += step;
    servo.write(currentAngle);
    delay(servoSpeed);
  }
}
