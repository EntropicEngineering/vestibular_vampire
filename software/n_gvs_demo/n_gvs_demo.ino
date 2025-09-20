#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <math.h>

#define SDA_PIN 14
#define SCL_PIN 15
#define DAC_ADDRESS 0x62

#define BAUD_RATE 115200

//Startup delay, (ms).
#define STARTUP_DELAY 5000

Adafruit_MCP4725 dac;

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

const int maxAmplitude = 2048;    
const int baseOffset = 2048;      
float offsetD = 0.04;             
float limitedArea = 3.6;          
const int numSteps = 100;          

int noiseTable[numSteps];         

void setup(void)
{
  Serial.begin(BAUD_RATE);
  //Set the I2C pins.
  Wire.setSDA(SDA_PIN);
  Wire.setSCL(SCL_PIN);

  dac.begin(DAC_ADDRESS);  
  Wire.setClock(1000000);         

  float mappedOffsetD = mapFloat(offsetD, 0, 8.0, 0, 4095);
  float mappedLimitedArea = mapFloat(limitedArea, 0, 8.0, 0, 4095);
  float actualAmplitude = mappedLimitedArea / 2.0;  
  float actualOffset = baseOffset + mappedOffsetD;
  dac.setVoltage((int)actualOffset, false); 
  delay(STARTUP_DELAY);  
  
  //Generate an array of random values, witin constraints.
  for (int i = 0; i < numSteps; i++)
  {
    noiseTable[i] = (int)(random(-maxAmplitude, maxAmplitude) + actualOffset);
    //Original line:
    //noiseTable[i] = (int)(random(-actualAmplitude, actualAmplitude) + actualOffset);

  }
}

void loop()
{
  int delayTime = 100000 / numSteps; 

  //Step through the array of random values.
  for (int i = 0; i < numSteps; i++)
  {
    //Set the output voltage based on the current random value.
    dac.setVoltage(noiseTable[i], false);
    delayMicroseconds(delayTime);  

    //Generate a new random value for this array index.
    noiseTable[i] = (int)(random(-maxAmplitude, maxAmplitude) + baseOffset);
    //Original line:
    //noiseTable[i] = (int)(random(-actualAmplitude, actualAmplitude) + baseOffset);
  }
}