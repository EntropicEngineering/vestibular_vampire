#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <math.h>
#include <Uduino_Wifi.h>
#include <WiFi.h>
#include <Adafruit_INA219.h>

#define SDA_PIN 14
#define SCL_PIN 15
#define V5V_EN_PIN 13
#define V24V_EN_PIN 22
#define VREF_EN_PIN 20
#define ASENSE_EN_PIN 3
#define RGB_PIN 16
#define BUTTON1_PIN 21
#define BUTTON2_PIN 26
#define BATT_CHARGING_PIN 27
#define BATT_FULL_PIN 28

#define DAC_ADDRESS 0x62
#define ASENSE_ADDRESS 0x40

#define BAUD_RATE 115200

#define SSID "Vestibular Vampire"
#define PASSWORD "dracula"
#define UNITY_PORT 4222

//Startup delay, (ms).
#define STARTUP_DELAY 3000


//Instantiate current sensor.
Adafruit_INA219 asense(ASENSE_ADDRESS);
//Create Unity board name.
Uduino_Wifi uduino("vestibular_vampire");
//Instantiate DAC.
Adafruit_MCP4725 dac;

float currentValue = 0;
int slopeValue = 0;
int durationValue = 0;
float offset = 0.04;
float current_mA = 0;

unsigned long lastPrintTime = 0;
const unsigned long printInterval = 50;

unsigned long sineWaveStartTime = 0;
int sineWaveStep = 0;
bool sineWaveRunning = false;
int sineWaveSteps = 0;
int sineWaveDelayTime = 0;
int sineWaveAmplitude = 0;
int sineWaveOffsetValue = 0;
int sineWaveFinalDuration = 0;
unsigned long sineWaveFinalStartTime = 0;
bool sineWaveHoldFinalVoltage = false;
bool sineWaveReturnToMidpoint = false;

void setup(void)
{
  Serial.begin(BAUD_RATE);

  //Configure IO pins.
  pinMode(V5V_EN_PIN, OUTPUT);
  pinMode(V24V_EN_PIN, OUTPUT);
  pinMode(VREF_EN_PIN, OUTPUT);
  pinMode(ASENSE_EN_PIN, OUTPUT);
  pinMode(RGB_PIN, OUTPUT);

  //Inputs need internal pullups.
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BATT_CHARGING_PIN, INPUT_PULLUP);
  pinMode(BATT_FULL_PIN, INPUT_PULLUP);

  //Set IO pin defaults.
  //Bring up current sensor first.
  digitalWrite(ASENSE_EN_PIN, HIGH);
  delay(50);
  //Turn on the reference voltage.
  digitalWrite(VREF_EN_PIN, HIGH);
  delay(50);
  //Turn on the 5V rail (DAC).
  digitalWrite(V5V_EN_PIN, HIGH);
  //Keep the 24V rail off for now (no output to the user).
  digitalWrite(V24V_EN_PIN, LOW);

  //Set the I2C pins.
  Wire1.setSDA(SDA_PIN);
  Wire1.setSCL(SCL_PIN);
  Wire1.begin();

  //Start DAC.
  dac.begin(DAC_ADDRESS, &Wire1);

  //Start current sensor.
  asense.begin(&Wire1);
  asense.setCalibration_16V_400mA();

  uduino.setPort(UNITY_PORT);
  uduino.useSendBuffer(true);
  uduino.setConnectionTries(35);
  uduino.useSerial(true);

  //Wifi setup.
  uduino.connectWifi(SSID, PASSWORD);
  uduino.addCommand("a", Apply);

  delay(STARTUP_DELAY);

  //Turn on the main output.
  digitalWrite(V24V_EN_PIN, HIGH);
}

void loop()
{
  uduino.update();
  printCurrentValues();
  handleSineWave();
}

void Apply()
{
  int parameters = uduino.getNumberOfParameters();
  if (parameters >= 3) {
    currentValue = atof(uduino.getParameter(0));
    slopeValue = atoi(uduino.getParameter(1));
    durationValue = atoi(uduino.getParameter(2));

    if (currentValue >= -4 && currentValue <= 4 && slopeValue > 0 && durationValue > 0) {
      int inputValue = mapFloat(currentValue, -4.0, 4.0, 0, 4095);
      sineWaveOffsetValue = mapFloat(offset, -4.0, 4.0, 0, 4095) - 2048;
      sineWaveAmplitude = inputValue - 2048;
      sineWaveSteps = slopeValue;
      sineWaveDelayTime = slopeValue / sineWaveSteps; 
      sineWaveFinalDuration = durationValue;
      sineWaveStep = 0;
      sineWaveRunning = true;
      sineWaveHoldFinalVoltage = false;
      sineWaveReturnToMidpoint = false;
      sineWaveStartTime = millis(); 
    } else {
      //Serial.println("Invalid parameters");
    }
  } else {
    //Serial.println("Expected 3 parameters.");
  }
}

void handleSineWave()
{
  if (sineWaveRunning) {
    if (sineWaveStep < sineWaveSteps) {
      // Ascending
      if (millis() - sineWaveStartTime >= sineWaveDelayTime) {
        float angle = (PI / 2.0) * (float)sineWaveStep / (float)sineWaveSteps;
        int voltage = 2048 + sineWaveAmplitude * sin(angle) + sineWaveOffsetValue;
        dac.setVoltage(voltage, false);
        sineWaveStep++;
        sineWaveStartTime = millis();
      }
    } else if (!sineWaveHoldFinalVoltage) {
      // Holding
      int finalVoltage = 2048 + sineWaveAmplitude * sin(PI / 2.0) + sineWaveOffsetValue;
      dac.setVoltage(finalVoltage, false);
      sineWaveFinalStartTime = millis();
      sineWaveHoldFinalVoltage = true;
    } else if (sineWaveHoldFinalVoltage && (millis() - sineWaveFinalStartTime >= sineWaveFinalDuration)) {
      // Descending 
      if (sineWaveStep < 2 * sineWaveSteps) {
        if (millis() - sineWaveStartTime >= sineWaveDelayTime) {
          float angle = (PI / 2.0) * (float)(sineWaveStep - sineWaveSteps) / (float)sineWaveSteps;
          int voltage = 2048 + sineWaveAmplitude * sin(PI / 2.0 - angle) + sineWaveOffsetValue;
          dac.setVoltage(voltage, false);
          sineWaveStep++;
          sineWaveStartTime = millis();
        }
      } else if (sineWaveStep >= 2 * sineWaveSteps) {
        // Reset to midpoint
        dac.setVoltage(2048 + sineWaveOffsetValue, false);
        sineWaveRunning = false;
      }
    }
  }
}


void printCurrentValues()
{
  if (millis() - lastPrintTime >= printInterval) {
    lastPrintTime = millis();
    current_mA = asense.getCurrent_mA();
    uduino.println(current_mA);
  }
}

int mapFloat(float x, float in_min, float in_max, int out_min, int out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
