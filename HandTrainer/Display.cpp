#include "Display.h"
Display::Display(){};
void Display::display(byte targetGesture, byte currentGesture) {
  //refresh output message
  lcd.setCursor(0, 0);
  lcd.print("Target:  " + String(bitRead(targetGesture, 0))+String(bitRead(targetGesture, 1))+String(bitRead(targetGesture, 2))+String(bitRead(targetGesture, 3))+String(bitRead(targetGesture, 4)));
  lcd.setCursor(0, 1);
  lcd.print("Current: " + String(bitRead(currentGesture, 0))+String(bitRead(currentGesture, 1))+String(bitRead(currentGesture, 2))+String(bitRead(currentGesture, 3))+String(bitRead(currentGesture, 4)));
}
void Display::setup(byte targetGesture, byte currentGesture) {
  lcd.init();       //essential
  lcd.backlight();  //because backlight is off by default
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Target:  " + String(bitRead(targetGesture, 0))+String(bitRead(targetGesture, 1))+String(bitRead(targetGesture, 2))+String(bitRead(targetGesture, 3))+String(bitRead(targetGesture, 4)));
  lcd.setCursor(0, 1);
  lcd.print("Current: " + String(bitRead(currentGesture, 0))+String(bitRead(currentGesture, 1))+String(bitRead(currentGesture, 2))+String(bitRead(currentGesture, 3))+String(bitRead(currentGesture, 4)));
}
void Display::testingMode(byte targetGesture) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Target: " + String(bitRead(targetGesture, 0))+String(bitRead(targetGesture, 1))+String(bitRead(targetGesture, 2))+String(bitRead(targetGesture, 3))+String(bitRead(targetGesture, 4)));
  lcd.setCursor(0, 1);
  lcd.print("Testing Mode");
}