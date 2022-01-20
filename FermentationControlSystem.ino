#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

///////////////////////////////////////////////////////////////////////////

boolean DEBUG = false;
boolean CONFIG_MODE = false;
boolean SHOW_COOL_DOWN = false;
boolean SHOW_HEAT_UP = false;
int COOL_HEAT = 10;
const char* SSID = "WLAN";                      // Enter SSID here
const char* WLAN_PW = "74696325262072177928";   // Enter Password here
String HOSTNAME = "ESP-Fermentation";           // Enter Hostname here
const int TEMP = 2;                             // GPIO for OneWire-Bus
int CURR_TEMP = 0;
float CURR_TEMP_F = 0;
int TARGET_TEMP = 5;
int TEMP_HYSTERESIS = 1;
const int COOL_DOWN = 1;                        // GPIO for cooling down Relais
const int HEAT_UP = 3;                          // GPIO for heating up Relais
unsigned long lastMillis = 0;
int b = 0;

// Initialize MQTT Publisher
const char* MQTT_SERVER = "192.168.178.71";     // Enter MQTT Broker here
StaticJsonDocument<256> doc;
char json_msg[128];

WiFiClient espClient;
PubSubClient client(espClient);

// Initializing ESP Rotary Encoder
int encoderPinA = 14;
int encoderPinB = 12;
int encoderButton = 16;
int encoderPinALast = LOW;
int encoderPinANow = LOW;
int lastButtonState;                            // the previous state of button
int currentButtonState;                         // the current state of button
int a = 0;

// Initializing OLED Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Initializing a oneWire instance to communicate with any OneWire devices
OneWire oneWire(TEMP);
DallasTemperature sensors(&oneWire);

///////////////////////////////////////////////////////////////////////////



void setup() {
  Serial.begin(9600);
  delay(1000);

  initDisplay();
  display.println("Initialize...");
  display.display();
   
  connectWifi();
  display.printf("-Wifi: %s\n",WiFi.hostname().c_str());
  display.display();

  initRelayBoard();
  display.println("-RelayBoard");
  display.display();
  
  initRotaryEncoder();
  currentButtonState = digitalRead(encoderButton);
  display.println("-RotaryEncoder");
  display.display();

  // setup MQTT communication 
  client.setServer(MQTT_SERVER, 1883);
  display.println("-MQTT Publisher");
  display.display();

  publishMessage(0,0,0);

}

void loop() {

  lastButtonState    = currentButtonState;      // save the last state
  currentButtonState = digitalRead(encoderButton); // read new state

  if(lastButtonState == HIGH && currentButtonState == LOW) {
    Serial.println(CONFIG_MODE);
    CONFIG_MODE = !CONFIG_MODE;
  } 
  
  if (CONFIG_MODE == true) {
    
    Serial.println("Config Mode");
    
    // Switch off relais
    digitalWrite(COOL_DOWN, HIGH);
    digitalWrite(HEAT_UP, HIGH);

    // rotary encoder
    encoderPinANow = digitalRead(encoderPinA);
    if ((encoderPinALast == HIGH) && (encoderPinANow == LOW)) {
      if (digitalRead(encoderPinB) == HIGH) {
        if (TARGET_TEMP > 4) {
          TARGET_TEMP--;
        }
      } else {
        if (TARGET_TEMP < 25) {
          TARGET_TEMP++;
        }
      }
      if (DEBUG) {
        Serial.println(TARGET_TEMP);
      }
    }
    encoderPinALast = encoderPinANow;

    display.clearDisplay();
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(10, 14);
    display.print("CONFIG:");
    display.setFont(&FreeMonoBold18pt7b);
    display.setCursor(20, 50);
    display.print(TARGET_TEMP);
    display.print(" C");
    display.display();
    
  } else {

    //publish a message roughly every minute.
    if ((millis() - lastMillis) > 3000) {
      lastMillis = millis();
      
      CURR_TEMP_F = getTemp(0);
      display.clearDisplay();
      display.setFont(&FreeMonoBold18pt7b);
      display.setCursor(10, 22);
      display.print(CURR_TEMP_F, 1);
      display.print("C");
      display.setFont(&FreeMono9pt7b);

      CURR_TEMP = (int)CURR_TEMP_F;

      if (CURR_TEMP > TARGET_TEMP + TEMP_HYSTERESIS) {
        digitalWrite(COOL_DOWN, LOW);
        SHOW_COOL_DOWN = true;
      }
  
      if (CURR_TEMP < TARGET_TEMP) {
        digitalWrite(COOL_DOWN, HIGH);
        SHOW_COOL_DOWN = false;
      }
 
      if ((CURR_TEMP > 10) && (CURR_TEMP < TARGET_TEMP - TEMP_HYSTERESIS)) {
        digitalWrite(HEAT_UP, LOW);
        SHOW_HEAT_UP = true;
      }
  
      if (CURR_TEMP > TARGET_TEMP) {
        digitalWrite(HEAT_UP, HIGH);
        SHOW_HEAT_UP = false;
      }

      COOL_HEAT = 10;
  
      if (SHOW_COOL_DOWN == true) {
        display.setCursor(5, 41);
        display.print("<Cool down>");
        COOL_HEAT = 0;
      }
  
      if (SHOW_HEAT_UP) {
        display.setCursor(12, 41);
        display.print("<Heat up>");
        COOL_HEAT = 20;
      }

      display.setCursor(8, 60);
      display.print("Target:");
      display.print(TARGET_TEMP);
      display.print("C");
      display.display();
      
      a = a + 1;
      if (a > 10 ) {
        a = 0;
        publishMessage(TARGET_TEMP, CURR_TEMP, COOL_HEAT);
      }
    }
  }
}

float getTemp(int index) {
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(index);
  if (temperatureC < -9) {
       return -9;
  } else {
      return temperatureC;
  }
}

void publishMessage(int TARGET_TEMP, int CURR_TEMP, int COOL_HEAT) {
  
  display.setCursor(119, 10);
  if(!client.connected()) {
    b = 0;
    if(DEBUG){Serial.print("Connecting to MQTT Broker");}
    while (!client.connected()) {
      client.connect(HOSTNAME.c_str());
      if(DEBUG){Serial.print(".");}      
      
      delay(50);
      
      b = b + 1;
      if(b > 4) {
        b = 0;
        display.print("e");
        break;
      } 
    }
    if(DEBUG){Serial.println("connected!");}
  }
  client.loop();

  if (client.connect(HOSTNAME.c_str())) {

    // build JSON object
    doc["sender"] = "FermentationCabinet";
    doc["cool_heat"] = COOL_HEAT;
    doc["target_temp"] = TARGET_TEMP;
    doc["top_temp"] = getTemp(1);
    doc["bottom_temp"] = CURR_TEMP;
    int msg_size = serializeJson(doc, json_msg);
   
    client.publish("test", json_msg, msg_size);
    display.print("*");
  }

  display.display();
}

void connectWifi() {
  
  //connect to your local wi-fi network
  //WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.begin(SSID, WLAN_PW);
  WiFi.hostname(HOSTNAME.c_str());
  WiFi.mode(WIFI_STA);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    if (DEBUG) {Serial.print(".");}
  }

  if (DEBUG) {
    Serial.printf("WiFi connected: %s\n", WiFi.hostname().c_str());
  }
}

void initRotaryEncoder() {
  // Setup ESP Rotary Encoder
  pinMode (encoderPinA, INPUT_PULLUP);
  pinMode (encoderPinB, INPUT_PULLUP);
  pinMode (encoderButton, INPUT_PULLUP);
  if (DEBUG) {
    Serial.println("RotaryEncoder started!");
  }
}


void initRelayBoard() {
  pinMode(COOL_DOWN, OUTPUT);
  digitalWrite(COOL_DOWN, HIGH);
  pinMode(HEAT_UP, OUTPUT);
  digitalWrite(HEAT_UP, HIGH);
  if (DEBUG) {
    Serial.println("5.Relais started!");
  }
}
  

void initDisplay() {
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for (;;); // Don't proceed, loop forever
  }
  display.display();
  delay(2000); // Pause for 2 seconds

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
}
