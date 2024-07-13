#include <Arduino.h>

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  delay(2000);
  Serial.println("Startup");
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("Running");
  delay(1000);
}
