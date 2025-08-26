#include "Button.h"

Button::Button(int buttonPin, unsigned long debounceDelay)
	:buttonPin(buttonPin), debounceDelay(debounceDelay), lastDebounceTime(0), buttonState(LOW), lastButtonState(LOW)
{

}

bool Button::debounce()
{
	//Boolean to return if button is pressed
	bool state = false;

	// read the state of the switch into a local variable:
	int reading = digitalRead(buttonPin);

	// check to see if you just pressed the button
	// (i.e. the input went from LOW to HIGH), and you've waited long enough
	// since the last press to ignore any noise:

	// If the switch changed, due to noise or pressing:
	if (reading != lastButtonState) {
		// reset the debouncing timer
		lastDebounceTime = millis();
	}

	if ((millis() - lastDebounceTime) > debounceDelay) {
		// whatever the reading is at, it's been there for longer than the debounce
		// delay, so take it as the actual current state:

		// if the button state has changed:
		if (reading != buttonState) {
			buttonState = reading;

			// only runs if the new button state is LOW
			if (buttonState == LOW) {
				state = true;
			}
		}
	}

	// save the reading. Next time through the loop, it'll be the lastButtonState:
	lastButtonState = reading;

	return state;
}