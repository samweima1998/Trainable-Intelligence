// Program for training a physical neural network
// Made by Samuël A.M. Weima for the paper by Pengrong Lyu and Samuël A.M. Weima.
using namespace std;

#include "Display.h"
Display display;

// used for checking the neuron gap, by taking the ratio between the resistance of the gap and a reference resistance and multiplying by 1023
int sensorInDigital[5] = {
    A0, A1, A2, A3, A6};
int sensorInDigitalLength = sizeof(sensorInDigital) / sizeof(int);

// used to detect barreljack power insertion
int modeSelectPin = A7;

// illumination extend time to account for heating
int uvExtendTime = 1000; // in millis
int blueExtendTime = 0;   // in millis
unsigned long extendTimer[5] = {
    0, 0, 0, 0, 0};
int timeExtended[5] = {
    0, 0, 0, 0, 0};
int LEDstatus[5] = {
    0, 0, 0, 0, 0};
int oldLEDstatus[5] = {
    0, 0, 0, 0, 0};

int actuatorHeaterPWM = 10; // max 0 for max heating
int oldActuatorHeaterPWM = 10;
int gapHeaterPWM = 1000; // max 0 for max heating
int oldGapHeaterPWM = 1000;
unsigned long waitTime = 10000; //Time to wait for temperature to stabilize
byte targetGapstate = B00000;
byte currentGapstate = B00000;

bool trainingHistoryTrackerFlag = false;
unsigned long startTime;

#include "nrfx_pwm.h"
#include "nrf_gpio.h"

#define PWM_TOP 1000

// illumination duty cylcle (intensity)
int blueDutyCycle = 900; // PWM_TOP-0 0 means always on, PWM_TOP means always off
int uvDutyCycle = 10;    // PWM_TOP-0 0 means always on, PWM_TOP means always off

// UV LEDs: D3, D5, D7, D9, D11
#define UV0 NRF_GPIO_PIN_MAP(1, 12) // D3
#define UV1 NRF_GPIO_PIN_MAP(1, 13) // D5
#define UV2 NRF_GPIO_PIN_MAP(0, 23) // D7
#define UV3 NRF_GPIO_PIN_MAP(0, 27) // D9
#define UV4 NRF_GPIO_PIN_MAP(1, 1)  // D11

// Blue LEDs: D2, D4, D6, D8, D10
#define B0 NRF_GPIO_PIN_MAP(1, 11) // D2
#define B1 NRF_GPIO_PIN_MAP(1, 15) // D4
#define B2 NRF_GPIO_PIN_MAP(1, 14) // D6
#define B3 NRF_GPIO_PIN_MAP(0, 21) // D8
#define B4 NRF_GPIO_PIN_MAP(1, 2)  // D10

// Heaters: D12, D13
#define gapHeaterPWMPin NRF_GPIO_PIN_MAP(1, 8)       // D12
#define actuatorHeaterPWMPin NRF_GPIO_PIN_MAP(0, 13) // D13

nrfx_pwm_t pwm0 = NRFX_PWM_INSTANCE(0); // B0, B1, B2, B3
nrfx_pwm_t pwm1 = NRFX_PWM_INSTANCE(1); // B4, UV0, UV1, UV2
nrfx_pwm_t pwm2 = NRFX_PWM_INSTANCE(2); // UV3, UV4, gapHeaterPWMPin, actuatorHeaterPWMPin

nrf_pwm_values_individual_t values0;
nrf_pwm_values_individual_t values1;
nrf_pwm_values_individual_t values2;

nrf_pwm_values_individual_t prev_values0 = {0};
nrf_pwm_values_individual_t prev_values1 = {0};
nrf_pwm_values_individual_t prev_values2 = {0};

nrf_pwm_sequence_t seq0;
nrf_pwm_sequence_t seq1;
nrf_pwm_sequence_t seq2;

void setup_pwm(nrfx_pwm_t *pwm,
               nrf_pwm_sequence_t *seq,
               nrf_pwm_values_individual_t *values,
               uint32_t pin0, uint32_t pin1, uint32_t pin2, uint32_t pin3)
{
  seq->values.p_individual = values;
  seq->length = NRF_PWM_VALUES_LENGTH(*values);
  seq->repeats = 0;
  seq->end_delay = 0;

  nrfx_pwm_config_t config = {
      .output_pins = {pin0, pin1, pin2, pin3},
      .irq_priority = APP_IRQ_PRIORITY_LOWEST,
      .base_clock = NRF_PWM_CLK_1MHz,
      .count_mode = NRF_PWM_MODE_UP,
      .top_value = PWM_TOP,
      .load_mode = NRF_PWM_LOAD_INDIVIDUAL,
      .step_mode = NRF_PWM_STEP_AUTO};

  nrfx_pwm_init(pwm, &config, NULL);
}

void setup()
{
  Serial.begin(115200);

  while (!Serial)
  {
    ; // wait for serial port to connect. Needed for Native USB only
  }
  // initialize the output pins:
  setup_pwm(&pwm0, &seq0, &values0, B0, B1, B2, B3);
  setup_pwm(&pwm1, &seq1, &values1, B4, UV0, UV1, UV2);
  setup_pwm(&pwm2, &seq2, &values2, UV3, UV4, gapHeaterPWMPin, actuatorHeaterPWMPin);

  // Initialize with heaters on and all LEDs off
  values2.channel_2 = gapHeaterPWM;
  values2.channel_3 = actuatorHeaterPWM;
  LEDupdater(LEDstatus);

  // initialize sensor pins
  for (int thisPin = 0; thisPin < sensorInDigitalLength; thisPin++)
  {
    pinMode(sensorInDigital[thisPin], INPUT_PULLDOWN);
  }

  pinMode(modeSelectPin, INPUT_PULLUP); // default pulled DOWN via barreljack. If barrel plug power is inserted, pulls itself UP, signalling testing mode

  // initialize lcd display
  display.setup(targetGapstate, currentGapstate);

  startTime = millis();
}

void loop()
{
  if (Serial.available())
  {
    String data = Serial.readString();

    data.trim();

    // A '1' indicates a closed circuit, which results in a lowered finger
    if (data == "ZERO")
    {
      targetGapstate = B11111;
      gapHeaterPWM = 1000;
    }
    if (data == "ONE")
    {
      targetGapstate = B11101;
      gapHeaterPWM = 50;
    }
    if (data == "TWO")
    {
      targetGapstate = B11001;
      gapHeaterPWM = 100;
    }
    if (data == "THREE")
    {
      targetGapstate = B10001;
      gapHeaterPWM = 200;
    }
    if (data == "FOUR")
    {
      targetGapstate = B00001;
      gapHeaterPWM = 300;
    }
    if (data == "FIVE")
    {
      targetGapstate = B00000;
      gapHeaterPWM = 700;
    }
    if (data == "SIX")
    {
      targetGapstate = B01110;
      gapHeaterPWM = 100;
    }
    if (data == "SEVEN")
    {
      targetGapstate = B11111;
      gapHeaterPWM = 10;
    }
    if (data == "EIGHT")
    {
      targetGapstate = B11100;
      gapHeaterPWM = 100;
    }
    if (data == "NINE")
    {
      targetGapstate = B11111;
      gapHeaterPWM = 10;
    }
    if (data == "TEN")
    {
      targetGapstate = B11111;
      gapHeaterPWM = 10;
    }
    if (digitalRead(modeSelectPin) == 1)
    {
      display.testingMode(targetGapstate);
    }
    else
    {
      display.display(targetGapstate, currentGapstate);
    }
    if (gapHeaterPWM != oldGapHeaterPWM)
    {
      oldGapHeaterPWM = gapHeaterPWM;
      gapHeaterPWMupdater(gapHeaterPWM);
      Serial.println("Waiting for temperature to stabilize");
      delay(waitTime);
      Serial.println("Temperature assumed stabilized");
    }
  }
  if (digitalRead(modeSelectPin) == 1)
  {
    int CLEARstatus[5] = {
        0, 0, 0, 0, 0};
    LEDupdater(CLEARstatus);
    return;
  }

  trainingHistoryTrackerFlag = false;
  for (int actuator = 0; actuator < sensorInDigitalLength; actuator++)
  {
    // read the sensor value
    if (analogRead(sensorInDigital[actuator]) >= 950)
    {
      bitWrite(currentGapstate, actuator, 1);
    }

    else if (analogRead(sensorInDigital[actuator]) <= 800)
    {
      bitWrite(currentGapstate, actuator, 0);
    }

    if (bitRead(currentGapstate, actuator) == bitRead(targetGapstate, actuator))
    {
      if (oldLEDstatus[actuator] == 1)
      {
        if (timeExtended[actuator] == 1)
        {
          if ((millis() - extendTimer[actuator]) >= uvExtendTime)
          {
            timeExtended[actuator] = 0;
            LEDstatus[actuator] = 0;
          }
        }
        else
        {
          extendTimer[actuator] = millis();
          timeExtended[actuator] = 1;
          trainingHistoryTrackerFlag = true;
        }
      }
      if (oldLEDstatus[actuator] == 2)
      {
        if (timeExtended[actuator] == 2)
        {
          if ((millis() - extendTimer[actuator]) >= blueExtendTime)
          {
            timeExtended[actuator] = 0;
            LEDstatus[actuator] = 0;
          }
        }
        else
        {
          extendTimer[actuator] = millis();
          timeExtended[actuator] = 2;
          trainingHistoryTrackerFlag = true;
        }
      }
    }
    else
    {
      if (bitRead(targetGapstate, actuator) == 1)
      {
        LEDstatus[actuator] = 1;
      }
      else
      {
        LEDstatus[actuator] = 2;
      }
    }

    if (LEDstatus[actuator] != oldLEDstatus[actuator])
    {
      trainingHistoryTrackerFlag = true;
      oldLEDstatus[actuator] = LEDstatus[actuator];
    }
  }
  if (trainingHistoryTrackerFlag == true)
  {
    trainingHistoryTracker(LEDstatus, currentGapstate);
    display.display(targetGapstate, currentGapstate);
  }
  display.display(targetGapstate, currentGapstate);
  LEDupdater(LEDstatus);
}
void trainingHistoryTracker(int currentLEDstatus[5], byte currentGapstate)
{
  String trainingHistoryString = String(millis() - startTime);
  for (int unitIndex = 0; unitIndex < 5; unitIndex++)
  {
    trainingHistoryString += ", ";
    trainingHistoryString = trainingHistoryString + String(bitRead(targetGapstate, unitIndex));
    trainingHistoryString += ", ";
    trainingHistoryString = trainingHistoryString + String(bitRead(currentGapstate, unitIndex));
    trainingHistoryString += ", ";
    trainingHistoryString = trainingHistoryString + String(currentLEDstatus[unitIndex]);
  }

  Serial.println(trainingHistoryString);
}
void LEDupdater(int currentLEDstatus[5])
{
  // Set new values based on LED status
  nrf_pwm_values_individual_t new0 = values0;
  nrf_pwm_values_individual_t new1 = values1;
  nrf_pwm_values_individual_t new2 = values2;

  for (int i = 0; i < 5; i++)
  {
    int uv = PWM_TOP;
    int blue = PWM_TOP;

    if (currentLEDstatus[i] == 1)
      uv = uvDutyCycle;
    if (currentLEDstatus[i] == 2)
      blue = blueDutyCycle;

    switch (i)
    {
    case 0:
      new0.channel_0 = blue;
      new1.channel_1 = uv;
      break;
    case 1:
      new0.channel_1 = blue;
      new1.channel_2 = uv;
      break;
    case 2:
      new0.channel_2 = blue;
      new1.channel_3 = uv;
      break;
    case 3:
      new0.channel_3 = blue;
      new2.channel_0 = uv;
      break;
    case 4:
      new1.channel_0 = blue;
      new2.channel_1 = uv;
      break;
    }
  }

  // Check each PWM group for changes
  if (memcmp(&new0, &prev_values0, sizeof(new0)) != 0)
  {
    values0 = new0;
    seq0.values.p_individual = &values0;
    prev_values0 = new0;
    nrfx_pwm_stop(&pwm0, true);
    nrfx_pwm_simple_playback(&pwm0, &seq0, 1, NRFX_PWM_FLAG_LOOP);
  }

  if (memcmp(&new1, &prev_values1, sizeof(new1)) != 0)
  {
    values1 = new1;
    seq1.values.p_individual = &values1;
    prev_values1 = new1;
    nrfx_pwm_stop(&pwm1, true);
    nrfx_pwm_simple_playback(&pwm1, &seq1, 1, NRFX_PWM_FLAG_LOOP);
  }

  if (memcmp(&new2, &prev_values2, sizeof(new2)) != 0)
  {
    values2 = new2;
    seq2.values.p_individual = &values2;
    prev_values2 = new2;
    nrfx_pwm_stop(&pwm2, true);
    nrfx_pwm_simple_playback(&pwm2, &seq2, 1, NRFX_PWM_FLAG_LOOP);
  }
}
void gapHeaterPWMupdater(int currentgapHeaterPWM)
{
  nrf_pwm_values_individual_t new2 = values2;

  new2.channel_2 = currentgapHeaterPWM;

  if (memcmp(&new2, &prev_values2, sizeof(new2)) != 0)
  {
    values2 = new2;
    seq2.values.p_individual = &values2;
    prev_values2 = new2;
    nrfx_pwm_stop(&pwm2, true);
    nrfx_pwm_simple_playback(&pwm2, &seq2, 1, NRFX_PWM_FLAG_LOOP);
  }
}
