#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <Adafruit_INA219.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

#define SDA_PIN 14
#define SCL_PIN 15
#define V5V_EN_PIN 13
#define V24V_EN_PIN 22
#define VREF_EN_PIN 20
#define ASENSE_EN_PIN 3
#define RGB_LED_PIN 16
#define BUTTON1_PIN 21  //Active low.
#define BUTTON2_PIN 26  //Active low.
#define BATT_CHARGING_PIN 27  //Active low.
#define BATT_FULL_PIN 28  //Active low.

#define DAC_ADDRESS 0x60
//#define DAC_ADDRESS 0x62  //Dev kit only.
#define ASENSE_ADDRESS 0x40
#define IMU_ADDRESS 0x19

#define RGB_LED_COUNT 7 //The "teeth" are #1 and #5.  Center is #3.

#define BAUD_RATE 115200
#define STARTUP_DELAY 0

#define SSID "Vestibular Vampire"
#define PASSWORD "12345678"

#define BUTTON1_CURRENT 3.0
#define BUTTON1_SLOPE 200
#define BUTTON1_DURATION  1000

#define BUTTON2_CURRENT -3.0
#define BUTTON2_SLOPE 200
#define BUTTON2_DURATION  1000

// Slider values
float current_slider = 0.0; //Range: -3.5 to 3.5 mA
int slope_slider = 50;     //Range: 0 to 2000 ms
int duration_slider = 50;  //Range: 0 to 5000 ms

//Instantiate current sensor.
Adafruit_INA219 asense(ASENSE_ADDRESS);
//Instantiate DAC.
Adafruit_MCP4725 dac;
//Instantiate RGB LEDs.
Adafruit_NeoPixel rgb_leds = Adafruit_NeoPixel(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

float currentValue = 0.0;
int slopeValue = 0;
int durationValue = 0;
float offset = 0.085;   //Offset current (mA).
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


/////////////////////////////////////////////////
//Web page.


// Captive portal DNS settings
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Web server on port 80
WebServer server(80);

// Web UI HTML
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Vestibular Vampire</title>
  <style>
    body
    {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background-color: #f0f0f0;
      text-align: center;
    }

    h1
    {
      color: black;
      margin-bottom: 30px;
      font-size: 5.5em;
    }

    .slider-container {
      background: #fff;
      box-shadow: 0 2px 6px rgba(0,0,0,0.1);
      padding: 1em 1em;
      margin: 2.5rem 1rem 0 1rem;
    }

    label {
      font-size: 4em;
      display: block;
      margin-bottom: 1.5rem;
    }

    input[type=range] {
      -webkit-appearance: none;
      width: 100%;
      height: 30px;
      background: #ddd;
      border-radius: 5px;
      outline: none;
      transition: background 0.3s;
      margin: 3rem 0;
    }

    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 7rem;
      height: 7rem;
      border-radius: 50%;
      background: #b81d1d;
      cursor: pointer;
      box-shadow: 0 0 5px rgba(0,0,0,0.2);
    }

    input[type=range]::-moz-range-thumb {
      width: 7rem;
      height: 7rem;
      border-radius: 50%;
      background: #b81d1d;
      cursor: pointer;
    }

    .value-display {
      font-weight: bold;
      color: #b81d1d;
    }

    #sendButton {
      width: calc(100% - 40px);
      padding: 0.5em 0 0.5em 0;
      margin: 10rem 1rem 0 1rem;
      font-size: 3em;
      font-weight: bold;
      border: none;
      background-color: #b81d1d;
      color: white;
      cursor: pointer;
      box-shadow: 0 2px 4px rgba(0,0,0,0.2);
      transition: background-color 0.3s;
    }

  </style>
</head>
<body>
  <h1>Vestibular Vampire</h1>

  <div class="slider-container">
    <label>Current (mA): <span id="val1" class="value-display">0</span></label>
    <input type="range" id="slider1" min="-3.5" max="3.5" step="0.1" value="0">
  </div>

  <div class="slider-container">
    <label>Slope (ms): <span id="val2" class="value-display">200</span></label>
    <input type="range" id="slider2" min="0" max="2000" step="10" value="200">
  </div>

  <div class="slider-container">
    <label>Duration (ms): <span id="val3" class="value-display">500</span></label>
    <input type="range" id="slider3" min="0" max="5000" step="10" value="500">
  </div>

  <br />

  <button id="sendButton">Send</button>

  <script>
    const sliders = ['slider1', 'slider2', 'slider3'];

    sliders.forEach(id => {
      const slider = document.getElementById(id);
      const valSpan = document.getElementById("val" + id.slice(-1));
      slider.addEventListener("input", () => {
        valSpan.textContent = slider.value;
      });
    });

    document.getElementById("sendButton").addEventListener("click", () => {
      const s1 = document.getElementById("slider1").value;
      const s2 = document.getElementById("slider2").value;
      const s3 = document.getElementById("slider3").value;

      fetch(`/set?slider1=${s1}&slider2=${s2}&slider3=${s3}`)
        .then(response => {
          if (!response.ok) {
            alert("Failed to send values");
          }
        });
    });
  </script>
</body>
</html>
)rawliteral";



//End web page.
////////////////////////////////////////////////////////////////////


// Serve root HTML page
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

// Handle slider updates
void handleSet() {
  if (server.hasArg("slider1")) {
    current_slider = server.arg("slider1").toFloat();
  }
  if (server.hasArg("slider2")) {
    slope_slider = server.arg("slider2").toInt();
  }
  if (server.hasArg("slider3")) {
    duration_slider = server.arg("slider3").toInt();
  }

  server.send(200, "text/plain", "OK");

  Serial.print("Current slider: ");
  Serial.println(current_slider);
  Serial.print("Slope slider: ");
  Serial.println(slope_slider);
  Serial.print("Duration slider: ");
  Serial.println(duration_slider);

  apply();
}

// Redirect unknown URLs to captive portal
void handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void apply()
{
  currentValue = current_slider;
  slopeValue = slope_slider;
  durationValue = duration_slider;

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
  if (millis() - lastPrintTime >= printInterval)
  {
    lastPrintTime = millis();
    current_mA = asense.getCurrent_mA();
    //Serial.print("\rCurrent: ");
    //Serial.print(current_mA);
    //Serial.print(" mA");
  }
}

int mapFloat(float x, float in_min, float in_max, int out_min, int out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

//Short startup animation of the RGB LEDs.  Blocking.
void rgb_startup()
{
  //Turn on one additional LED at a time in sequence.
  for (int i=0; i<RGB_LED_COUNT; i++)
  {
    //Set the LED red.
    rgb_leds.setPixelColor(i, rgb_leds.Color(255, 0, 0));
    //Display this change.
    rgb_leds.show();

    //Small delay before switching to the next LED.
    delay(100);
  }

  //Turn everything off again.
  rgb_leds.clear();
  rgb_leds.show();
}

//Set the RGB lights.
void set_rgb_leds()
{
  //If the unit is outputting a pulse, display light in the corresponding direction.
  if (sineWaveRunning)
  {
    //Turn off any other status indicators.
    rgb_leds.clear();

    //If it's a positive (right) pulse, light LEDs on one side.
    if (currentValue > 0)
    {
      rgb_leds.setPixelColor(0, rgb_leds.Color(200, 0, 0));
      rgb_leds.setPixelColor(1, rgb_leds.Color(200, 0, 0));
    }
    //If it's a negative (left) pulse, light LEDs on the other side.
    else if (currentValue < 0)
    {
      rgb_leds.setPixelColor(6, rgb_leds.Color(200, 0, 0));
      rgb_leds.setPixelColor(5, rgb_leds.Color(200, 0, 0));
    }
  }
  //Otherwise display battery status.
  else
  {
    //Turn off LEDs when done with a pulse.
    rgb_leds.clear();

    //Read the battery status pins, and display light accordingly.
    //If the battery is full, display green.  If charging, display yellow.  Otherwise off.
    if (!digitalRead(BATT_FULL_PIN))
    {
      rgb_leds.setPixelColor(3, rgb_leds.Color(0, 200, 20));
    }
    else if (!digitalRead(BATT_CHARGING_PIN))
    {
      rgb_leds.setPixelColor(3, rgb_leds.Color(200, 20, 0));
    }
    else
    {
      rgb_leds.setPixelColor(3, rgb_leds.Color(0, 0, 0));
    }
  }
  
  rgb_leds.show();
}

//Read the physical buttons.
void read_buttons()
{
  //Only proceed if the unit is idle.
  if (!sineWaveRunning)
  {
    //If button 1 is pressed, create a pulse.
    if (!digitalRead(BUTTON1_PIN))
    {
      //Create a pulse.
      current_slider  = BUTTON1_CURRENT;
      slope_slider    = BUTTON1_SLOPE;
      duration_slider = BUTTON1_DURATION;

      //Send the pulse.
      apply();
    }
    //If button 2 is pressed, create a pulse.
    else if (!digitalRead(BUTTON2_PIN))
    {
      //Create a pulse.
      current_slider  = BUTTON2_CURRENT;
      slope_slider    = BUTTON2_SLOPE;
      duration_slider = BUTTON2_DURATION;

      //Send the pulse.
      apply();
    }
  }
}

void setup()
{
  Serial.begin(BAUD_RATE);

  //Configure IO pins.
  pinMode(V5V_EN_PIN, OUTPUT);
  pinMode(V24V_EN_PIN, OUTPUT);
  pinMode(VREF_EN_PIN, OUTPUT);
  pinMode(ASENSE_EN_PIN, OUTPUT);
  pinMode(RGB_LED_PIN, OUTPUT);

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
  //Keep the 24V rail off for now (no output to the subject).
  digitalWrite(V24V_EN_PIN, LOW);

  //Set the I2C pins.
  Wire1.setSDA(SDA_PIN);
  Wire1.setSCL(SCL_PIN);
  Wire1.begin();

  //Start DAC.
  dac.begin(DAC_ADDRESS, &Wire1);
  sineWaveOffsetValue = mapFloat(offset, -4.0, 4.0, 0, 4095) - 2048;
  // Reset to midpoint
  dac.setVoltage(2048 + sineWaveOffsetValue, false);

  //Start current sensor.
  asense.begin(&Wire1);
  asense.setCalibration_16V_400mA();

  //Start RGB LEDs.
  rgb_leds.begin();
  //Initialize to off.
  rgb_leds.show();

  delay(STARTUP_DELAY);

  // Start WiFi in AP mode
  WiFi.softAP(SSID, PASSWORD);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // Start DNS server — redirect all requests to our IP
  dnsServer.start(DNS_PORT, "*", myIP);

  // Start web server
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server started");

  //Turn on the main output.
  digitalWrite(V24V_EN_PIN, HIGH);

  //Flash the LEDs to indicate startup is complete.
  rgb_startup();
}

void loop()
{
  dnsServer.processNextRequest();
  server.handleClient();
  printCurrentValues();
  handleSineWave();
  set_rgb_leds();
  read_buttons();
}