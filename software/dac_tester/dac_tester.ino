#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <math.h>

#define SDA_PIN 14
#define SCL_PIN 15
#define DAC_ADDRESS 0x62

//Startup delay, (ms).
#define STARTUP_DELAY 1000

Adafruit_MCP4725 dac;

       
void setup(void)
{
  //Set the I2C pins.
  Wire1.setSDA(SDA_PIN);
  Wire1.setSCL(SCL_PIN);
  Wire1.begin();


  dac.begin(DAC_ADDRESS, &Wire1);  

  dac.setVoltage(2048, false); 
  delay(STARTUP_DELAY);  
}

void loop()
{
    dac.setVoltage(0, false);
    delay(1000);  
    dac.setVoltage(2048, false);
    delay(1000);  
    dac.setVoltage(4095, false);
    delay(1000); 
    dac.setVoltage(2048, false);
    delay(1000);   
}