#include <Servo.h>


// DEFINE PINS
const int buttonPin = 7;
const int servoPin = 5;
const int fanPin = 3;

const int intThermo = 12;
const int extThermo = 13;

// DEFINE VARIABLES
const int servoSpeed = 50; // ms per degree, higher = slower
int currentAngle = 0;
int mode = 0;
bool lastButtonState = HIGH;

int modeMatrix[][2] = {      // targetAngle (0 > 360; 90 = open; 0 = closed); fanSpeed (0 = off; 255 = 100% power)
  {0,   0},   // Mode 0: closed, fan off
  {90,  64},  // Mode 2: open, fan low
  {90,  128}, // Mode 3: open, fan medium
  {90,  255}  // Mode 4: open, fan full
};
int totalModes = sizeof(modeMatrix) / sizeof(modeMatrix[0]);

Servo servo_pin5;

void speed_Servo(int targetAngle);

void setup(){

  Serial.begin(9600); 
  servo_pin5.attach(servoPin);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(fanPin, OUTPUT);
  
  servo_pin5.write(modeMatrix[mode][0]);
  currentAngle = modeMatrix[mode][0];
  
  Serial.println("System Initialized...");

}

void loop(){ // ? hz
  int buttonState = digitalRead(buttonPin);

  //analogWrite(fanPin, 255);

// 1. Check for button press (Transition from HIGH to LOW)
  if (buttonState == LOW && lastButtonState == HIGH) {     
    delay(50); // Short debounce
    mode = (mode + 1) % totalModes;
    
    // Log the change so you can see it in the console!
    Serial.print("Mode Changed to: ");
    Serial.println(mode);

// 2. Only move the servo when the button is actually pressed
    int targetAngle = modeMatrix[mode][0];
    int fanSpeed = modeMatrix[mode][1]; 
    Serial.print("Fan speed set to: ");
    Serial.println(fanSpeed);
    analogWrite(fanPin, fanSpeed);
    speed_Servo(targetAngle);
  }

  // 3. Update lastButtonState for the next loop iteration
  lastButtonState = buttonState;
}

void speed_Servo(int targetAngle) {
  Serial.print("Moving to "); Serial.println(targetAngle);
  if (currentAngle < targetAngle) {
    for (int i = currentAngle; i <= targetAngle; i++) {
      servo_pin5.write(i);
      
      delay(servoSpeed);
    }
  } else {
    for (int i = currentAngle; i >= targetAngle; i--) {
      servo_pin5.write(i);
      delay(servoSpeed);
    }
  }
  currentAngle = targetAngle;
}

