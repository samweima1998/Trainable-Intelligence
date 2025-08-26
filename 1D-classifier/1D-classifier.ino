// Program for training a physical neural network
// Made by Samuël A.M. Weima for the paper by Pengrong Lyu and Samuël A.M. Weima.
using namespace std;

#include "Button.h"
#include "Display.h"
Display display;
String displayMessage;
// these values need not be present in the trainingSet
int inputMin = 0;        // Set lowest possible input, which is also the initial minimum threshold
int inputMax = 100;      // Set the highest possible input, which is also the initial maximum threshold
int realThreshold = 55;  // Real threshold as decided by user, will be used for generating random dataset
// timestamp, input, minimum threshold, maximum threshold, light type, successrate, successflag
int trainingHistory[7] = { 0, 0, inputMin, inputMax, 0, 0, 0 };
int trainingHistoryLength = sizeof(trainingHistory) / sizeof(trainingHistory[0]);
int oldTrainingHistory[7] = { 0, 0, inputMin, inputMax, 0, 0, 0 };
unsigned long startTime;
// 2-dimensional array of uvLED pin numbers:
int uvLED[1] = {
  11
};
int uvLEDLength = sizeof(uvLED) / sizeof(int);

// 2-dimensional array of blue LED pin numbers:
int blueLED[1] = {
  10
};
int blueLEDLength = sizeof(blueLED) / sizeof(int);

// can be used to track whether a pin is connected to blueLED (1), uvLED (2) or neither (0)
int pins[1] = {
  0
};

// used for checking the neuron gap, by taking the ratio between the resistance of the gap and a reference resistance and multiplying by 1023
int sensorInAnalog = A6;
// used for heating the neuron gap based on the input from the trainingSet
int gapHeaterPWMPin = 12;
int actuatorHeaterPWMPin = 13;
// used for seeding the random generator, keep unconnected
int randomPin = A7;
// buttons:
const int buttons[] = {
  A3
};
const int buttonLength = sizeof(buttons) / sizeof(int);
Button button0(buttons[0], 5);

int localThreshold;
int successRate;
int successStack = 0;
int failStack = 0;
int flawlessRound = 0;
int trainingCompleted = 0;

int testingMode = 0;

// illumination duty cylcle (intensity)
byte blueDutyCycle = 25;  // 0-255
byte uvDutyCycle = 255;   // 0-255

// illumination extend time to account for heating
int uvExtendTime = 0;    // in millis
int blueExtendTime = 0;  // in millis
int oldLEDstatus = 0;
int LEDstatus = 0;

// evaluation parameters
int waitTime = 15000;  // in millis
int offset = 20;
float scalingFactor = 255 / (100 + offset);  // multiplied directly with input value, max 2.55; min 0.1
int targetSuccesRate = 90;                   // in testing mode this is automatically set to 0
int minimumSamples = 10;                     // should equal or be smaller than dataset, or program will run forever
const int randomSetLength = 10;              // determines length of random set for training/testing. Can be ignored when using predetermined set
int randomSet = 1;                           // set to 1 for random set, set to 0 for predetermined set
int trainingSet[randomSetLength][3];         // uncomment this line to generate random trainingSet
int trainingSetLength;                       // uncomment this line to generate random trainingSet
// comment out the 4 lines below when generating random trainingSet
// int trainingSet[][3] = {
//   { 100, 1, 0 }, { 50, 0, 0 }, { 40, 0, 0 }, { 95, 1, 0 }, { 70, 1, 0 }, { 45, 0, 0 }, { 23, 0, 0 }, { 59, 1, 0 }, { 80, 1, 0 }, { 75, 1, 0 }
// };  //, { 65, 1, 0 }, { 23, 0, 0 }, { 56, 1, 0 }, { 80, 1, 0 }, { 100, 1, 0 }
// int trainingSetLength = sizeof(trainingSet) / sizeof(trainingSet[0]);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for Native USB only
  }
  // initialize the output pins:
  for (int thisPin = 0; thisPin < uvLEDLength; thisPin++) {
    pinMode(uvLED[thisPin], OUTPUT);
    analogWrite(uvLED[thisPin], 0);
  }
  for (int thisPin = 0; thisPin < blueLEDLength; thisPin++) {
    pinMode(blueLED[thisPin], OUTPUT);
    analogWrite(blueLED[thisPin], 0);
  }
  // initialize buttons
  for (int thisPin = 0; thisPin < buttonLength; thisPin++) {
    pinMode(buttons[thisPin], INPUT_PULLUP);
  }
  // initialize sensor
  pinMode(gapHeaterPWMPin, OUTPUT);
  analogWrite(gapHeaterPWMPin, 0);
  pinMode(actuatorHeaterPWMPin, OUTPUT);
  analogWrite(actuatorHeaterPWMPin, 255);
  pinMode(sensorInAnalog, INPUT);



  // to generate a random trainingSet
  if (randomSet == 1) {
    randomSeed(analogRead(randomPin));  // for closer-to-random "random" numbers, use the fairly random reading on a dedicated unconnected pin
    // randomSeed(1);                      // to consistently get the same "random" numbers, use a fixed seed

    for (int trainingSetIndex = 0; trainingSetIndex < randomSetLength; trainingSetIndex++) {
      int randomInput = random(inputMin, inputMax);
      int expectedOutput;
      if (randomInput >= realThreshold) {
        expectedOutput = 1;
      } else {
        expectedOutput = 0;
      }
      trainingSet[trainingSetIndex][0] = randomInput;
      trainingSet[trainingSetIndex][1] = expectedOutput;
      trainingSet[trainingSetIndex][2] = 0;
      trainingSetLength = randomSetLength;
    }
  }
  Serial.println("Type 0 to train || Type 1 to test");
  while (true) {
    if (Serial.available()) {
      String data = Serial.readString();
      data.trim();

      if ((data == "TRAIN") || (data == "0")) {
        testingMode = 0;
        break;  // Just exits the while loop
      }
      if ((data == "TEST") || (data == "1")) {
        testingMode = 1;
        break;
      }
    }
  }
  // To visualize whether the device is in training or testing mode
  // initialize lcd display
  switch (testingMode) {
    case 0:
      displayMessage = "trainingMode";
      break;
    case 1:
      displayMessage = "testingMode";
      targetSuccesRate = 0;
      break;
  }
  display.setup(displayMessage);
  Serial.println(displayMessage);

  startTime = millis();
  // Serial.println("dataLogger connection confirmed, start program");
  Serial.println("time, input, lower limit threshold, upper limit threshold, LED state, success rate, success flag");
}

void loop() {

  if (trainingCompleted == 0) {
    // clear test history at start of trainingSet
    successStack = 0;
    failStack = 0;
    for (int trainingIndex = 0; trainingIndex < trainingSetLength; trainingIndex++) {
      flawlessRound = 1;  // assume flawlessRound until proven wrong
      analogWrite(gapHeaterPWMPin, (trainingSet[trainingIndex][0] + offset) * scalingFactor);
      // Serial.println("Start pre-heating");

      trainingHistoryTracker(1, trainingSet[trainingIndex][0]);

      unsigned long waitStart = millis();
      while ((millis() - waitStart) < waitTime) {
      }
      // Serial.println("Temperature assumed equilibrated, initiate test");
      LEDstatus = 0;

      trainingSet[trainingIndex][2] = 0;  // flag test completion
      localThreshold = trainingSet[trainingIndex][0];
      while (trainingSet[trainingIndex][2] == 0) {
        if (trainingSet[trainingIndex][1] == 1) {
          if (analogRead(sensorInAnalog) >= 1012) {
            trainingSet[trainingIndex][2] = 1;
            LEDstatus = 0;
          } else {
            trainingSet[trainingIndex][2] = 0;
            LEDstatus = 1;
          }
          // threshold should finish below input; Pengrong change the reference resistance to 10k to decrease the current that pass trough the gap. So the value "930" should increase to 1012
          if (oldTrainingHistory[3] > localThreshold) {
            trainingHistoryTracker(3, localThreshold);
          }
        }
        if (trainingSet[trainingIndex][1] == 0) {
          if (analogRead(sensorInAnalog) <= 974) {
            trainingSet[trainingIndex][2] = 1;
            LEDstatus = 0;
          } else {
            trainingSet[trainingIndex][2] = 0;
            LEDstatus = 2;
          }
          // threshold should now be above input; Pengrong change the reference resistance to 10k to decrease the current that pass trough the gap. So the value "511" should increase to 974
          if (oldTrainingHistory[2] < localThreshold) {
            trainingHistoryTracker(2, localThreshold);
          }
        }
        if (LEDstatus != oldLEDstatus) {
          flawlessRound = 0;
          if (testingMode == 1) {
            break;  // when testing, do not shine any light. Instead simply register failure and proceed to next datapoint
          }
          trainingHistoryTracker(4, LEDstatus);
          if (LEDstatus == 0 && oldLEDstatus == 1) {
            // Serial.println("True Positive, turn off any UV light");
            delay(uvExtendTime);  // overexpose to account for heating
            analogWrite(uvLED[0], 0);
          }
          if (LEDstatus == 0 && oldLEDstatus == 2) {
            // Serial.println("True Negative, turn off any blue light");
            delay(blueExtendTime);  // overexpose to account for heating
            analogWrite(blueLED[0], 0);
          }
          if (LEDstatus == 1) {
            // Serial.println("False Negative, shine UV light");
            analogWrite(uvLED[0], uvDutyCycle);
            trainingHistoryTracker(2, inputMin);  // uv illumination means lower limit of threshold is at inputMin
          }
          if (LEDstatus == 2) {
            // Serial.println("False Positive, shine blue light");
            analogWrite(blueLED[0], blueDutyCycle);
            trainingHistoryTracker(3, inputMax);  // blue illumination means upper limit of threshold is at inputMax
          }
          oldLEDstatus = LEDstatus;
        }
      }
      if (flawlessRound == 1) {
        successStack += 1;
      } else {
        failStack += 1;
      }
      trainingHistoryTracker(6, flawlessRound);
    }
  }
  if (trainingCompleted == 0 && successRate > targetSuccesRate && (successStack + failStack) >= minimumSamples) {
    Serial.println("targetSuccessRate achieved over sufficient sample size, stop training");
    trainingCompleted = 1;
  }
  while (trainingCompleted == 1) {
    if (Serial.available()) {
      String data = Serial.readString();
      data.trim();

      if ((data == "TRAIN") || (data == "0")) {
        testingMode = 0;
        trainingCompleted = 0;
        display.display("trainingMode");
        Serial.println("start training");
      }
      if ((data == "TEST") || (data == "1")) {
        testingMode = 1;
        trainingCompleted = 0;
        display.display("testingMode");
        Serial.println("start testing");
      }
    }
  }
}
void trainingHistoryTracker(int dataType, int dataValue) {

  trainingHistory[0] = millis() - startTime;
  if ((successStack + failStack) > 0) {
    successRate = 100 * successStack / (successStack + failStack);  // Rounded down
  }
  trainingHistory[5] = successRate;
  trainingHistory[6] = 0;  // assume failure
  trainingHistory[dataType] = dataValue;
  String trainingHistoryString;
  for (int trainingHistoryDataType = 0; trainingHistoryDataType < trainingHistoryLength; trainingHistoryDataType++) {
    trainingHistoryString = trainingHistoryString + String(trainingHistory[trainingHistoryDataType]);
    if (trainingHistoryDataType < (trainingHistoryLength - 1)) {
      trainingHistoryString += ", ";
    }
  }

  Serial.println(trainingHistoryString);
  for (int trainingHistoryDataType = 0; trainingHistoryDataType < trainingHistoryLength; trainingHistoryDataType++) {
    oldTrainingHistory[trainingHistoryDataType] = trainingHistory[trainingHistoryDataType];
  }
}