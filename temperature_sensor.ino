#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 2        // Data pin connected to pin 2

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);


int temperature=0;

int get_temp();

void setup(){
  pinMode(startButton, INPUT);
  // Starting Serial Monitor
  Serial.begin(9600);
  sensors.begin();
  Serial.println("DS18B20 Ready");
}

void loop()
{
// Whenever the button is pressed, the 7-segment display displays a number
// The number changes with each button press.
  
  int buttonState = digitalRead(startButton);
  if(buttonState == HIGH && (lastButtonState != HIGH)){
    temperature= get_temp();
    // When button is pressed, call the function displayNumber(count)
  } // If ends
// Make current button state as last button state for debouncing
  lastButtonState = buttonState;
}


int get_temp() {
  sensors.requestTemperatures();
  float tempF = sensors.getTempFByIndex(0);

  if (tempF == DEVICE_DISCONNECTED_F) {
    Serial.println("Error: Sensor not found!");
    return -999;
  }

  Serial.print("Temp: ");
  Serial.print(tempF);
  Serial.println(" °F");

  delay(1000);
  return (int)tempF;
}
