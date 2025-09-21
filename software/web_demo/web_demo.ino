#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <math.h>
#include <WiFi.h>

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

//Startup delay, (ms).
#define STARTUP_DELAY 3000

Adafruit_MCP4725 dac;

WiFiServer server(80);

String htmlPage = R"rawliteral(
<!DOCTYPE HTML><html>
<head><title>Vestibular Vampire Control</title></head>
<body>
  <h1>Control Panel</h1>
  <form action="/apply" method="POST">
    Current Value (-4 to 4):<br><input type="number" step="0.01" min="-4" max="4" name="current" required><br>
    Slope Value (positive integer):<br><input type="number" min="1" name="slope" required><br>
    Duration Value (ms):<br><input type="number" min="1" name="duration" required><br><br>
    <input type="submit" value="Apply">
  </form>
</body></html>
)rawliteral";

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

  //Initialize DAC communication, using the proper Wire1 device.
  dac.begin(DAC_ADDRESS, &Wire1);  

  float mappedOffsetD = mapFloat(offsetD, 0, 8.0, 0, 4095);
  float mappedLimitedArea = mapFloat(limitedArea, 0, 8.0, 0, 4095);
  float actualAmplitude = mappedLimitedArea / 2.0;  
  float actualOffset = baseOffset + mappedOffsetD;
  dac.setVoltage((int)actualOffset, false); 
  
  //Generate an array of random values, witin constraints.
  for (int i = 0; i < numSteps; i++)
  {
    noiseTable[i] = (int)(random(-maxAmplitude, maxAmplitude) + actualOffset);
    //Original line:
    //noiseTable[i] = (int)(random(-actualAmplitude, actualAmplitude) + actualOffset);

  }

  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  
  server.begin();


  //Turn on the main output.
  digitalWrite(V24V_EN_PIN, HIGH);
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