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
const int outsidePin  = 4; 
const int redPin      = 8;  
const int greenPin    = 9; 
const int bluePin     =10;
// ── TEMPERATURE SETTINGS ────────────────────────────────────
const float high_temp  = 80.0;  // °F — auto-enable fan above this
const float low_temp = 55.0;  // °F — auto-disable fan below this (hysteresis)
int TEMP_READ_INTERVAL = 5000;
  // ms between temperature reads


// ── FAN MODES ───────────────────────────────────────────────
//  { servoAngle, fanSpeed }
//  servoAngle : 0 = closed, 90 = open
//  fanSpeed   : 0 = off, 255 = full power
//int modeMatrix[][2] = {
 // {0,   0  },  // Mode 0: closed, fan off
 // {90,  255}   // Mode 1: open,   fan full
//};
//const int totalModes = sizeof(modeMatrix) / sizeof(modeMatrix[0]);

int fan_off= 0;
int fan_on=255;
int grate_close=0;
int grate_open=90;
int total_modes=4;
int mode=0;
int servoSpeed=20;
int currentAngle=0;

// ── STATE VARIABLES ─────────────────────────────────────────

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
void moveServo(int targetAngle,int fan_on_off);
void  readTemperatures();
void modeColor(int mode);



// ============================================================
void setup() {
  Serial.begin(9600);
  insideSensors.begin();
  outsideSensors.begin();

  servo.attach(servoPin);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(fanPin, OUTPUT);
  pinMode(redPin,   OUTPUT); 
  pinMode(greenPin, OUTPUT); 
  pinMode(bluePin,  OUTPUT); 

  servo.write(grate_close);
  analogWrite(fanPin,fan_off);

  Serial.println("System initialized.");
}


// ============================================================
void loop() {

  // 1. Read temperatures on a timed interval
  if (millis() - lastTempRead >= TEMP_READ_INTERVAL) {
    lastTempRead = millis();
    readTemperatures();
  }

  // 2. Check for button press (HIGH → LOW transition)
  bool buttonState = digitalRead(buttonPin);
  if (buttonState == LOW && lastButtonState == HIGH) {
    delay(50); // debounce

    mode = (mode + 1) % total_modes;
    Serial.print("Button pressed: Mode= ");
    Serial.println(mode);

    // Only apply manual mode if auto-override is not active,
    // or if the user is choosing a higher mode than the override
  }
  lastButtonState = buttonState;
  
// ============================================================
// Changes color based on mode!
// ============================================================
  if(mode==0){
    modeColor(mode);
    moveServo(grate_close,fan_off);
  }
  else if(mode==1){
    modeColor(mode);
    moveServo(grate_open,fan_on);
    
  }
  else if(mode==2){
    modeColor(mode);
    if (tempInside == DEVICE_DISCONNECTED_F) return; // Don't act on bad data

    if (tempInside >= low_temp && tempOutside<= high_temp && tempInside>= tempOutside) {
      moveServo(grate_open,fan_on);
    } 
    else{
      moveServo(grate_close,fan_off);
    }
  }
  else if(mode==3){
    modeColor(mode);
    if(tempInside > tempOutside && tempInside > low_temp){
      moveServo(grate_open,fan_on);
    }
    else{
      moveServo(grate_close,fan_off);
    }
  }
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

  Serial.print("Inside: ");  Serial.print(tempInside);  Serial.println(" °F");
  Serial.print("Outside: "); Serial.print(tempOutside); Serial.println(" °F");
}


// ============================================================
//  Auto-enable or disable fan based on inside temperature
// ============================================================



// ============================================================
//  Move servo one degree at a time (smooth, controlled speed)
// ============================================================
void moveServo(int targetAngle, int fan_on_off) {
  if (currentAngle == targetAngle) return; 

  Serial.print("Moving to "); Serial.println(targetAngle);

  if (currentAngle < targetAngle) {
    for (int i = currentAngle; i <= targetAngle; i++) {
      servo.write(i);
      delay(servoSpeed);
    }
    Serial.println("Grate: OPEN");
  } else {
    for (int i = currentAngle; i >= targetAngle; i--) {
      servo.write(i);
      delay(servoSpeed);
    }
    Serial.println("Grate: CLOSED");
  }

  currentAngle = targetAngle;
  analogWrite(fanPin, fan_on_off);
}
  

// ============================================================
// Changes color based on mode!
// ============================================================
void modeColor(int mode) {
  // sets everything to 0 by default
  digitalWrite(redPin,0);
  digitalWrite(greenPin,0);
  digitalWrite(bluePin,0);

  switch (mode) {
    case 0: // always OFF
      digitalWrite(redPin,1); // set to red
      break;
    case 1: // always ON
      digitalWrite(greenPin,1); // set to green
      break;
    case 2: // threshold
      digitalWrite(bluePin,1); // set to blue
      break;
    case 3: // comparison
      digitalWrite(redPin,1); // set to yellow
      digitalWrite(greenPin,1);
      break;
    default: // in case something's wrong
      digitalWrite(redPin,1); // set to white
      digitalWrite(greenPin,1);
      digitalWrite(bluePin,1);
  }
}
