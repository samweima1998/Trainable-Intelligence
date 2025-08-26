#include <arduino.h>

#include <LiquidCrystal_I2C.h>
class Display {
private:
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2);
public:

  Display();
  void display(byte targetGesture, byte currentGesture);
  void setup(byte targetGesture, byte currentGesture);
  void testingMode(byte targetGesture);
};