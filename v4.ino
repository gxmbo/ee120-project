// ============================================================
//  Fan + Fan Grate (controlled by servo) with Dual Temperature Sensors
//  - Button manually cycles fan modes (off and on) modes 0 and 1
//  - Inside sensor and ouside sensors auto control modes 2 and 3 
// ============================================================

#include <Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>


// ── PINS ────────────────────────────────────────────────────
const int buttonPin   = 7;
const int servoPin    = 5;
const int fanPin      = 3;
const int insidePin   = 2;   // Inside sensor pin
const int outsidePin  = 4;  // ousdie sensor pin
const int redPin      = 8;  
const int greenPin    = 9; 
const int bluePin     =10;

// ── TEMPERATURE SETTINGS ────────────────────────────────────
const float high_temp  = 80.0;  // °F ideal max temperature in home (pertains to auto mode 2)
const float low_temp = 55.0;  // °F  ideal min temperature in home (pertains to auto mode 2)
int TEMP_READ_INTERVAL = 5000; //time between temoperature reads 


//--- Global variables--------------------------------------
int fan_off= 0;
int fan_on=255;
int grate_close=0;
int grate_open=90;
int total_modes=4;
int mode=0;
int servoSpeed=20;


// ── STATE VARIABLES ─────────────────────────────────────────
bool lastButtonState = HIGH;
unsigned long lastTempRead = 0;
int currentAngle=0;
unsigned long buffer=0;
unsigned long cool_down=10000;// this ensures that the fan and grate dont rapidly open and close when temp thresholds get close to auto mode criteria
bool first_run=true;// this pairs with cool_down so it will not lock out auto mode when program is first initiated 

float tempInside  = 0.0;
float tempOutside = 0.0;


// ── OBJECTS ─────────────────────────────────────────────────
Servo servo;
OneWire insideWire(insidePin); //using libraries to attach the temperature sensor (DS18B20)
OneWire outsideWire(outsidePin);
DallasTemperature insideSensors(&insideWire);
DallasTemperature outsideSensors(&outsideWire);


// ── FUNCTION DECLARATIONS ───────────────────────────────────
void moveServo(int targetAngle,int fan_on_off); //this controls the fan grate being opened and closed and fan on and off
void  readTemperatures(); // calls temeperature read for both sensors
void modeColor(int mode); // toggels between the 4 different colors to indicate mode 



// start of program initialzing the setup
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

  servo.write(grate_close); //this makes sure to start the servo at 0 for calibration
  analogWrite(fanPin,fan_off); // this ensures the fan is off at start 

  Serial.println("System initialized.");
}


// start of main loop
void loop() {

  // Read temperatures on a timed interval
  if (millis() - lastTempRead >= TEMP_READ_INTERVAL) {
    lastTempRead = millis();
    readTemperatures();
  }

  // Check for button press (HIGH to LOW transition)
  bool buttonState = digitalRead(buttonPin);
  if (buttonState == LOW && lastButtonState == HIGH) {
    delay(50); // debounce, was glitchy witout this 

    mode = (mode + 1) % total_modes; //starts at 0 after cycling through all modes 
    Serial.print("Button pressed: Mode= ");
    Serial.println(mode); //helps trouble shoot current modes 
  }
  lastButtonState = buttonState;
  
  // start of 4 different modes 
  if(mode==0){
    modeColor(mode);//red is off but has power 
    moveServo(grate_close,fan_off);
  }
  else if(mode==1){ 
    modeColor(mode);//greeen
    moveServo(grate_open,fan_on);
    
  }
  else if(mode==2){ 
    modeColor(mode);//blue
    if (tempInside == DEVICE_DISCONNECTED_F || (millis()-buffer < cool_down && first_run==false)) return;// dont act on bad data or shut on/off if recently triggered 
    else{ 

      if (tempInside >= low_temp && tempOutside<= high_temp && tempInside>= tempOutside) {
      moveServo(grate_open,fan_on);
      } 
      else{
        moveServo(grate_close,fan_off);
      }
    }
  }
  else if(mode==3){
    modeColor(mode);//yellow
    if (tempInside == DEVICE_DISCONNECTED_F || (millis()-buffer < cool_down && first_run==false)) return;// dont act on bad data or shut on/off if recently triggered 
    else{
      if(tempInside > tempOutside && tempInside > low_temp){
        moveServo(grate_open,fan_on);
      }
      else{
      moveServo(grate_close,fan_off);
      }
    }
  }
}
  


//read temperatures and print values to serial 
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

// moves the servo at a smooth, controlled speed and call to turn on fan at same time
// prints out waht the grate and fan state shouold be 
void moveServo(int targetAngle, int fan_on_off) {
  if (currentAngle == targetAngle) return; 

  Serial.print("Moving to "); Serial.println(targetAngle);

  if (currentAngle < targetAngle) {
    for (int i = currentAngle; i <= targetAngle; i++) {
      servo.write(i);
      delay(servoSpeed);
    }
    Serial.println("Grate: OPEN");
    buffer=millis();
    first_run=false;
  } else {
    for (int i = currentAngle; i >= targetAngle; i--) {
      servo.write(i);
      delay(servoSpeed);
    }
    Serial.println("Grate: CLOSED");
    buffer=millis();
    first_run=false;
  }

  currentAngle = targetAngle;
  analogWrite(fanPin, fan_on_off);
}
  

//changes the state of the color based on mode 
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
    case 2: // threshold auto
      digitalWrite(bluePin,1); // set to blue
      break;
    case 3: // comparison auto
      digitalWrite(redPin,1); // set to yellow
      digitalWrite(greenPin,1);
      break;
    default: // in case something's wrong
      digitalWrite(redPin,1); // set to white
      digitalWrite(greenPin,1);
      digitalWrite(bluePin,1);
  }
}

