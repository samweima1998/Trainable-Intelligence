// Debounce.h

#ifndef _DEBOUNCE_h
#define _DEBOUNCE_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

class Button
{
private:
	//FIELDS
	unsigned long debounceDelay;
	int buttonState, lastButtonState, buttonPin;
	unsigned long lastDebounceTime;

public:
	//CONSTRUCTOR

	//Constructor of the Debounce class
	//Parameter:
	//ButtonPin:  Pin connected to button
	Button(int buttonPin, unsigned long debounceDelay);

	//METHODS

	//Method of the debounce function for a button
	//Returns true if pressed once, false if otherwise
	bool debounce();
};

#endif