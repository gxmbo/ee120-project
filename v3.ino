// ============================================================
//  Fan + Servo Controller with Dual Temperature Sensors
//  - Button manually cycles fan modes
//  - Inside sensor auto-enables fan if temp exceeds threshold
//  - Outside sensor logged for reference
//  - 5 minute cooldown prevents rapid open/close cycling
//  - Serial output only on state changes
// ============================================================

#include <Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ── PINS ────────────────────────────────────────────────────
const int buttonPin   = 7;
const int servoPin    = 5;
const int fanPin      = 3;
const int insidePin   = 2;
const int outsidePin  = 4;
const int redPin      = 8;
const int greenPin    = 9;
const int bluePin     = 10;

// ── TEMPERATURE SETTINGS ────────────────────────────────────
const float high_temp = 72.0;
const float low_temp  = 55.0;
const int   TEMP_READ_INTERVAL = 2000;

// ── COOLDOWN SETTING ────────────────────────────────────────
const unsigned long GRATE_COOLDOWN = 300000; // 5 minutes in ms

// ── FAN / GRATE STATES ──────────────────────────────────────
const int fan_off      = 0;
const int fan_on       = 255;
const int grate_closed = 0;
const int grate_open   = 90;
const int total_modes  = 4;
int mode               = 0;

// ── SERVO SETTINGS ──────────────────────────────────────────
const int servoSpeed = 50; // ms per degree, higher = slower
int currentAngle     = 0;  // explicitly tracked — more reliable than servo.read()

// ── STATE VARIABLES ─────────────────────────────────────────
bool lastButtonState        = HIGH;
unsigned long lastTempRead  = 0;
unsigned long lastGrateMove = 0;
bool grateIsOpen            = false;

float tempInside  = 0.0;
float tempOutside = 0.0;

// ── CHANGE-TRACKING VARIABLES ────────────────────────────────
int   lastReportedMode     = -1;
bool  lastReportedGrate    = false;
int   lastReportedFanSpeed = -1;
float lastReportedInside   = -999.0;
float lastReportedOutside  = -999.0;

// ── OBJECTS ─────────────────────────────────────────────────
Servo servo;
OneWire insideWire(insidePin);
OneWire outsideWire(outsidePin);
DallasTemperature insideSensors(&insideWire);
DallasTemperature outsideSensors(&outsideWire);

// ── FUNCTION DECLARATIONS ───────────────────────────────────
void moveServo(int targetAngle);
void readTemperatures();
void fan_state(int mode, int open_close, int fan_on_off);
void modeColor(int mode);
void trySetGrate(bool shouldOpen, int mode);
void printStatusIfChanged(int currentFanSpeed);

const char* modeLabel(int m) {
  switch (m) {
    case 0: return "OFF (manual)";
    case 1: return "ON  (manual)";
    case 2: return "AUTO - cool when inside warm & outside cool";
    case 3: return "AUTO - cool when inside warmer than outside";
    default: return "UNKNOWN";
  }
}

// ============================================================
void setup() {
  Serial.begin(9600);
  insideSensors.begin();
  outsideSensors.begin();

  servo.attach(servoPin);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(fanPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  // Write initial position and sync currentAngle
  servo.write(grate_closed);
  currentAngle = grate_closed;

  analogWrite(fanPin, fan_off);

  Serial.println("=== Fan Controller Initialized ===");
  Serial.println("Mode 0: OFF | Mode 1: ON | Mode 2: Auto-Cool | Mode 3: Auto-Delta");
  Serial.println("==================================");
}

// ============================================================
void loop() {
  int currentFanSpeed = grateIsOpen ? fan_on : fan_off;

  // 1. Read temperatures on a timed interval
  if (millis() - lastTempRead >= TEMP_READ_INTERVAL) {
    lastTempRead = millis();
    readTemperatures();
  }

  // 2. Check for button press (HIGH to LOW transition)
  bool buttonState = digitalRead(buttonPin);
  if (buttonState == LOW && lastButtonState == HIGH) {
    delay(50); // debounce
    mode = (mode + 1) % total_modes;
  }
  lastButtonState = buttonState;

  // 3. Apply mode logic
  modeColor(mode);

  if (mode == 0) {
    fan_state(mode, grate_closed, fan_off);
    currentFanSpeed = fan_off;
  }
  else if (mode == 1) {
    fan_state(mode, grate_open, fan_on);
    currentFanSpeed = fan_on;
  }
  else if (mode == 2) {
    if (tempInside == DEVICE_DISCONNECTED_F) return;
    if (tempInside >= low_temp && tempOutside <= high_temp && tempInside >= tempOutside) {
      trySetGrate(true, mode);
    } else {
      trySetGrate(false, mode);
    }
    currentFanSpeed = grateIsOpen ? fan_on : fan_off;
  }
  else if (mode == 3) {
    if (tempInside > tempOutside && tempInside > low_temp) {
      trySetGrate(true, mode);
    } else {
      trySetGrate(false, mode);
    }
    currentFanSpeed = grateIsOpen ? fan_on : fan_off;
  }

  // 4. Print status only when something meaningful changed
  printStatusIfChanged(currentFanSpeed);
}

// ============================================================
//  Move servo with explicit angle tracking (from ezo_UART)
//  Avoids servo.read() which can return stale/incorrect values
// ============================================================
void moveServo(int targetAngle) {
  if (currentAngle < targetAngle) {
    for (int i = currentAngle; i <= targetAngle; i++) {
      servo.write(i);
      delay(servoSpeed);
    }
  } else {
    for (int i = currentAngle; i >= targetAngle; i--) {
      servo.write(i);
      delay(servoSpeed);
    }
  }
  currentAngle = targetAngle;
}

// ============================================================
//  Attempt to open or close grate - respects 5 min cooldown
// ============================================================
void trySetGrate(bool shouldOpen, int mode) {
  unsigned long timeSinceLastMove = millis() - lastGrateMove;
  bool cooldownExpired = timeSinceLastMove >= GRATE_COOLDOWN;

  if (shouldOpen == grateIsOpen) return;

  if (!cooldownExpired) {
    static unsigned long lastCooldownPrint = 0;
    if (millis() - lastCooldownPrint >= 30000) {
      lastCooldownPrint = millis();
      Serial.print("Cooldown active. ");
      Serial.print((GRATE_COOLDOWN - timeSinceLastMove) / 1000);
      Serial.println("s remaining.");
    }
    return;
  }

  if (shouldOpen) {
    fan_state(mode, grate_open, fan_on);
    grateIsOpen = true;
  } else {
    fan_state(mode, grate_closed, fan_off);
    grateIsOpen = false;
  }

  lastGrateMove = millis();
}

// ============================================================
//  Read both sensors
// ============================================================
void readTemperatures() {
  insideSensors.requestTemperatures();
  outsideSensors.requestTemperatures();
  tempInside  = insideSensors.getTempFByIndex(0);
  tempOutside = outsideSensors.getTempFByIndex(0);
}

// ============================================================
//  Set fan and servo state
// ============================================================
void fan_state(int mode, int open_close, int fan_on_off) {
  moveServo(open_close);
  analogWrite(fanPin, fan_on_off);
}

// ============================================================
//  Print status to serial ONLY when something changed
// ============================================================
void printStatusIfChanged(int currentFanSpeed) {
  float roundedInside  = round(tempInside  * 10.0) / 10.0;
  float roundedOutside = round(tempOutside * 10.0) / 10.0;

  bool modeChanged  = (mode != lastReportedMode);
  bool grateChanged = (grateIsOpen != lastReportedGrate);
  bool fanChanged   = (currentFanSpeed != lastReportedFanSpeed);
  bool tempsChanged = (roundedInside != lastReportedInside || roundedOutside != lastReportedOutside);

  if (!modeChanged && !grateChanged && !fanChanged && !tempsChanged) return;

  Serial.println("----------------------------------");

  if (modeChanged) {
    Serial.print("MODE: ["); Serial.print(mode); Serial.print("] ");
    Serial.println(modeLabel(mode));
  }

  if (tempsChanged) {
    if (tempInside  == DEVICE_DISCONNECTED_F) Serial.println("WARNING: Inside sensor disconnected!");
    if (tempOutside == DEVICE_DISCONNECTED_F) Serial.println("WARNING: Outside sensor disconnected!");
    Serial.print("Temp  | Inside: ");  Serial.print(roundedInside);
    Serial.print(" °F  |  Outside: "); Serial.print(roundedOutside); Serial.println(" °F");
  }

  if (grateChanged || fanChanged) {
    Serial.print("Grate : "); Serial.println(grateIsOpen ? "OPEN" : "CLOSED");
    Serial.print("Fan   : "); Serial.println(currentFanSpeed > 0 ? "ON (255)" : "OFF (0)");
  }

  lastReportedMode     = mode;
  lastReportedGrate    = grateIsOpen;
  lastReportedFanSpeed = currentFanSpeed;
  lastReportedInside   = roundedInside;
  lastReportedOutside  = roundedOutside;
}

// ============================================================
//  Set RGB LED color based on mode
// ============================================================
void modeColor(int mode) {
  digitalWrite(redPin,   0);
  digitalWrite(greenPin, 0);
  digitalWrite(bluePin,  0);

  switch (mode) {
    case 0: digitalWrite(redPin, 1);                             break; // red
    case 1: digitalWrite(greenPin, 1);                           break; // green
    case 2: digitalWrite(bluePin, 1);                            break; // blue
    case 3: digitalWrite(redPin, 1); digitalWrite(greenPin, 1);  break; // yellow
    default: digitalWrite(redPin,1); digitalWrite(greenPin,1); digitalWrite(bluePin,1); break;
  }
}
