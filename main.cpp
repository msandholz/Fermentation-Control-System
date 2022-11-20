#include <Arduino.h>
#include <Wifi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <EEPROM.h>
#include <RotaryEncoder.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <SPIFFS.h>

///////////////////////////////////////////////////////////////////////////

#define VERSION 0.5

#define DEBUG_SERIAL false      // Enable debbuging over serial interface
#define DEBUG_OLED true         // Enable debbuging over serial interface        

#define ROTARY_CLK 32           // GPIO for rotary encoder clock
#define ROTARY_DT 25            // GPIO for rotary encoder dt
#define ROTARY_BUTTON 27        // GPIO for rotary encoder button

#define COOL_DOWN 4             // GPIO for cooling down Relais
#define HEAT_UP 12              // GPIO for heating up Relais

#define ON false
#define OFF true 

#define TEMP 5                  // GPIO for OneWire-Bus

#define ROTARYSTEPS 2
#define ROTARYMIN 5
#define ROTARYMAX 25

// ======================================================================
// Setting parameters with default values
// ======================================================================

const char* WIFI_SSID = "WLAN";                      // WLAN-SSID
const char* WIFI_PW = "74696325262072177928";        // WLAN-Password
String HOSTNAME = "ESP-32";                          // Enter Hostname here
String MQTT_BROKER = "192.168.178.120";              // MQTT-Broker

int CURR_TEMP = 0;
float CURR_TEMP_F = 0;
float ROOM_TEMP_F = 0;
int TARGET_TEMP = 10;
int TEMP_HYSTERESIS = 1;
int POWER_HEAT = 120;
int POWER_COOL = 240;
int POWER_IDLE = 10;

boolean CONFIG_MODE = false;
boolean SWITCH = true;                        // Enanble Fermantaion Control
boolean SHOW_COOL_DOWN = false;
boolean SHOW_HEAT_UP = false;
                       
int currentButtonState = LOW;                 // the current reading from the input pin
int lastButtonState = LOW;                    // the last reading from the input pin
int currentCLKState = LOW;
int lastCLKState = LOW;
int mqtt_publish_time = 45;                   // time in seconds for publishing MQTT message
int mqtt_time_counter = 0;
long lastMillis = 0;

// ======================================================================
// Initialize Objects
// ======================================================================

// Initialize WebServer
AsyncWebServer server(80);          

// Initializing OLED Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Initializing a oneWire instance to communicate with any OneWire devices
OneWire oneWire(TEMP);
DallasTemperature sensors(&oneWire);

// Initialize MQTT Publisher
char json_msg[128];
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Initialize Rotary Encoder
RotaryEncoder encoder(ROTARY_CLK, ROTARY_DT, RotaryEncoder::LatchMode::TWO03);
int lastPos = -1;

// ======================================================================
// Functions
// ======================================================================

void loadConfig(){
    if (!SPIFFS.begin(true)) { 
        if(DEBUG_SERIAL){Serial.print("! An error occurred during SPIFFS mounting !");}
        return; 
    }

    File config = SPIFFS.open("/config.json","r");

    if(config) { 
        // Deserialize the JSON document
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, config);

        if(DEBUG_SERIAL){Serial.println(config.readString());}

        if (error) { 
            if(DEBUG_SERIAL){
                Serial.print("! DeserializeJson failed! -> ");
                Serial.println(error.f_str());
            }
            return; 
        }
        JsonObject obj = doc.as<JsonObject>();
                
        String hostname = obj["HOSTNAME"];
        HOSTNAME = hostname.c_str();
        String mqtt_broker = doc["MQTT_BROKER"];     
        MQTT_BROKER = mqtt_broker.c_str();
        TARGET_TEMP = doc["TARGET_TEMP"];
        TEMP_HYSTERESIS = doc["TEMP_HYSTERESIS"];
        POWER_HEAT = doc["POWER_HEAT"];
        POWER_COOL = doc["POWER_COOL"];
        POWER_IDLE = doc["POWER_IDLE"];
        
        if(DEBUG_OLED){
            display.println("-Config loaded");
            display.display();
        }
    }
    config.close();
}

void saveConfig(AsyncWebServerRequest *request) {
    if (!SPIFFS.begin(true)) { 
        if(DEBUG_SERIAL){Serial.print("!An error occurred during SPIFFS mounting!");}
        return; 
    }

    File config = SPIFFS.open("/config.json","w");

    if(config) { 
        // Serialize the JSON document
        DynamicJsonDocument doc(1024);

        int paramsNr = request->params();
        for(int i=0;i<paramsNr;i++){
            AsyncWebParameter* p = request->getParam(i);
            
            if(p->name() == "HOSTNAME") {
                HOSTNAME = p->value();
                doc["HOSTNAME"] = HOSTNAME;
            }

            if(p->name() == "MQTT_BROKER") {
                MQTT_BROKER = p->value();
                doc["MQTT_BROKER"] = MQTT_BROKER;
            }

            if(p->name() == "TEMP_HYSTERESIS") {
                TEMP_HYSTERESIS = p->value().toInt();
                doc["TEMP_HYSTERESIS"] = TEMP_HYSTERESIS;
            }

            if(p->name() == "TARGET_TEMP") {
                TARGET_TEMP = p->value().toInt();
                doc["TARGET_TEMP"] = TARGET_TEMP;
            }

            if(p->name() == "POWER_HEAT") {
                POWER_HEAT = p->value().toInt();
                doc["POWER_HEAT"] = POWER_HEAT;
            }

            if(p->name() == "POWER_COOL") {
                POWER_COOL = p->value().toInt();
                doc["POWER_COOL"] = POWER_COOL;
            }

            if(p->name() == "POWER_IDLE") {
                POWER_IDLE = p->value().toInt();
                doc["POWER_IDLE"] = POWER_IDLE;                
            }
        }
    
        serializeJsonPretty(doc, config);
    }
    config.close();
}

void saveConfig() {
 if (!SPIFFS.begin(true)) { 
        if(DEBUG_SERIAL){Serial.print("!An error occurred during SPIFFS mounting!");}
        return; 
    }

    File config = SPIFFS.open("/config.json","w");

    if(config) { 
        // Serialize the JSON document
        DynamicJsonDocument doc(1024);
        doc["HOSTNAME"] = HOSTNAME;
        doc["MQTT_BROKER"] = MQTT_BROKER;
        doc["TEMP_HYSTERESIS"] = TEMP_HYSTERESIS;
        doc["TARGET_TEMP"] = TARGET_TEMP;
        doc["POWER_HEAT"] = POWER_HEAT;
        doc["POWER_COOL"] = POWER_COOL;
        doc["POWER_IDLE"] = POWER_IDLE;                
        serializeJsonPretty(doc, config);
    }   
    config.close();
}

void connectWifi() {
    //connect to your local wi-fi network
    //WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);

    WiFi.setHostname(HOSTNAME.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PW);
    
    pinMode(BUILTIN_LED,OUTPUT);
    if(DEBUG_SERIAL){
        Serial.print("Connecting to WiFi: ");
        Serial.print(WIFI_SSID);
    }

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(100);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        if(DEBUG_SERIAL){ Serial.print(".");}
    }
    
    if(DEBUG_SERIAL){
        Serial.println("OK!");
        Serial.print("Hostname: ");
        Serial.println(WiFi.getHostname());
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());  
    }
    
    if(DEBUG_OLED){
            display.println("-WiFi connected");
            display.display();
    }
}

String processor(const String& var){

    if(var == "HOSTNAME"){ return HOSTNAME; }
    if(var == "ROOM_TEMP"){  return String(ROOM_TEMP_F);  }
    if(var == "TARGET_TEMP"){  return String(TARGET_TEMP);  }
    if(var == "FRIDGE_TEMP"){  return String(CURR_TEMP_F);  }
  
    if(var == "POWER_CONSUMPTION") {   
        int power_consumption = POWER_IDLE;
        
        if(SHOW_HEAT_UP) { power_consumption = POWER_HEAT; }
        if(SHOW_COOL_DOWN) { power_consumption = POWER_COOL; }
            
        return String(power_consumption);
    }

    if(var == "SWITCH") {
        String switch_state = "";
        if (SWITCH == true) {switch_state = " checked"; }
        return switch_state;  
    }

    if (var == "COOL_HEAT_STATUS") {
        String cool_heat_status = "";       
        if(SHOW_HEAT_UP) {
            cool_heat_status = " <i class='fa fa-fire' style='font-size:24px;color:orange'></i>";
        } 
        if(SHOW_COOL_DOWN) {
            cool_heat_status = "<i class='fas fa-snowflake' style='font-size:24px;color:blue'></i>";
        } 
        return cool_heat_status;
    } 
    
    if(var == "WIFI_SSID"){  return String(WIFI_SSID);  }
    if(var == "WIFI_PW"){  return String(WIFI_PW);  }
    if(var == "MQTT_BROKER"){  return String(MQTT_BROKER);  }
    if(var == "TEMP_HYSTERESIS"){  return String(TEMP_HYSTERESIS);  }

    if(var == "POWER_HEAT"){  return String(POWER_HEAT);  }
    if(var == "POWER_COOL"){  return String(POWER_COOL);  }
    if(var == "POWER_IDLE"){  return String(POWER_IDLE);  }

    if(var == "VERSION"){  return String(VERSION);  } 

    return String();   
}

void startWebServer(){
    // start OTA WebServer
    AsyncElegantOTA.begin(&server);
    server.begin();
    if(DEBUG_SERIAL){Serial.println("OTA WebServer started!");}

    // Make index.html available
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        if(DEBUG_SERIAL){Serial.println("Requested index.html page");}
        request->send(SPIFFS, "/index.html", String(), false, processor);
    });

    // Make config.html available
    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
        if(DEBUG_SERIAL){Serial.println("Requested config.html page");}
        request->send(SPIFFS, "/config.html", String(), false, processor);
    });

    // Save config-parameters
    server.on("/save-config", HTTP_GET, [](AsyncWebServerRequest *request){
        if(DEBUG_SERIAL){Serial.println("Requested config.html page");}
        saveConfig(request);
        request->send(SPIFFS, "/config.html", String(), false, processor);
    });

    // Make check.html available
    server.on("/check", HTTP_GET, [](AsyncWebServerRequest *request){
        if(DEBUG_SERIAL){Serial.println("Requested check.html page");}
        request->send(SPIFFS, "/check.html", String(), false, processor);
    });

    // Make style.css available
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/style.css","text/css");
    });

    // Enable / Disbale Fermentation Control
    server.on("/switch", HTTP_GET, [](AsyncWebServerRequest *request){       
        SWITCH = !SWITCH;
        request->redirect("/");
    });

    // Make SMA Power Monitor available
    server.on("/power_meter", HTTP_GET, [](AsyncWebServerRequest *request){       
        DynamicJsonDocument doc(20);
        int power_consumption = POWER_IDLE;
        
        if(SHOW_HEAT_UP) { power_consumption = POWER_HEAT; }
        if(SHOW_COOL_DOWN) { power_consumption = POWER_COOL; }
               
        doc["power"] = power_consumption;
      
        String buf;
        serializeJson(doc, buf);
        request->send(200, "application/json", buf);
    });

    if(DEBUG_OLED){
        display.println("-WebServer started");
        display.display();
    }
}

void initRelayBoard() {
    pinMode(COOL_DOWN, OUTPUT_OPEN_DRAIN);
    digitalWrite(COOL_DOWN, HIGH);
    pinMode(HEAT_UP, OUTPUT_OPEN_DRAIN);
    digitalWrite(HEAT_UP, HIGH);
    
    if(DEBUG_SERIAL) { Serial.println("Relais initialized!");     }
    if(DEBUG_OLED){
        display.println("-Relay OK");
        display.display();
    }
}
  
void initRotaryEncoder() {
  // Setup ESP Rotary Encoder
  //pinMode (ROTARY_CLK, INPUT_PULLUP);
  //pinMode (ROTARY_DT, INPUT_PULLUP);
  pinMode (ROTARY_BUTTON, INPUT_PULLUP);

  if(DEBUG_SERIAL) {Serial.println("RotaryEncoder started!");}
  if(DEBUG_OLED){
        display.println("-Rotary encoder OK");
        display.display();
    }
}

void initMQTTClient() {

    mqttClient.setServer(MQTT_BROKER.c_str(), 1883);

    if(DEBUG_OLED){
        display.println("-MQTT Client OK");
        display.display();
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

    if(DEBUG_OLED){
        display.println("Initializing...");
        display.display();
    }
}

void getTemp() {
  sensors.requestTemperatures();
  float temp_fridge= sensors.getTempCByIndex(0);
  if (temp_fridge > 2) { CURR_TEMP_F = temp_fridge; }
  
  float temp_room = sensors.getTempCByIndex(1);
  if (temp_room > 2) { ROOM_TEMP_F = temp_room; }
}

void publishMessage() {

    display.setCursor(119, 10);
    
    if(!mqttClient.connected()) {
        int b = 0;
        if(DEBUG_SERIAL){Serial.print("Connecting to MQTT Broker");}
        
        while (!mqttClient.connected()) {
            mqttClient.connect(HOSTNAME.c_str());
            if(DEBUG_SERIAL){Serial.print(".");}      
            delay(100);

            b = b + 1;
            if(b > 4) {
                b = 0;
                display.print("e");
                break;
            }      
        }
        if(DEBUG_SERIAL){Serial.println("OK!");}
    }
    mqttClient.loop();

    if (mqttClient.connect(HOSTNAME.c_str())) {
        StaticJsonDocument<256> doc;                            // build JSON object
        doc["sender"] = HOSTNAME;
        doc["target_temp"] = TARGET_TEMP;
        doc["room_temp"] = int(ROOM_TEMP_F);
        doc["fridge_temp"] = CURR_TEMP;

        int COOL_HEAT = 10;
        if(SHOW_COOL_DOWN) { COOL_HEAT = 0; }
        if(SHOW_HEAT_UP) { COOL_HEAT = 0; }
        doc["cool_heat"] = COOL_HEAT;

        int msg_size = serializeJson(doc, json_msg);

        mqttClient.publish("test", json_msg, msg_size);
     
        display.print("*");
    } 
    display.display();
}


// ======================================================================
// Setup
// ======================================================================
void setup() {
    if(DEBUG_SERIAL){
        Serial.begin(115200);
        Serial.println("Welcome to ESP-32!");
    }
    initDisplay();              // Initialize OLED Display
    initRelayBoard();           // Initialize Relais Board
    initRotaryEncoder();        // Intitalize RotaryEncoder
    loadConfig();               // Loading Config from config.json
    connectWifi();              // Initialize WiFi Connection
    startWebServer();           // Starting WebServer
    initMQTTClient();           // Initialize MQTT Client
}

// ======================================================================
// Loop
// ======================================================================
void loop() {

    getTemp();                                                      // get current Temperatures

    currentButtonState = digitalRead(ROTARY_BUTTON);                // read new state

    // read the state of the switch/button:
    if (lastButtonState == HIGH && currentButtonState == LOW) {     // button is pressed
        if(CONFIG_MODE) { saveConfig(); }   
        CONFIG_MODE = !CONFIG_MODE;
        encoder.setPosition(TARGET_TEMP / ROTARYSTEPS);             // start with the value of TARGET_TEMP.
    }
    lastButtonState = currentButtonState;                            // save the the last state

    String info_text = "";
    
    if(SWITCH) {                                                    // Steuerung nur zulassen, wenn SWITCH = true      
        if (CURR_TEMP > TARGET_TEMP + TEMP_HYSTERESIS) {
            digitalWrite(COOL_DOWN, ON);
            info_text = "<Cool down>";
            SHOW_COOL_DOWN = true;
        }

        if (CURR_TEMP < TARGET_TEMP) {
            digitalWrite(COOL_DOWN, OFF);
            info_text = "...";
            SHOW_COOL_DOWN = false;
        }

        if ((CURR_TEMP > 10) && (CURR_TEMP < TARGET_TEMP - TEMP_HYSTERESIS)) {
            digitalWrite(HEAT_UP, ON);
            info_text = "<Heat up>";
            SHOW_HEAT_UP = true;
        }

        if (CURR_TEMP > TARGET_TEMP) {
            digitalWrite(HEAT_UP, OFF);
            info_text = "...";
            SHOW_HEAT_UP = false;
        }

    } else {
        info_text = "<OFF>";
        digitalWrite(COOL_DOWN, OFF);
        SHOW_COOL_DOWN = false;
        digitalWrite(HEAT_UP, OFF);
        SHOW_HEAT_UP = false;
    }

    if(CONFIG_MODE) {

        encoder.tick();

        int newPos = encoder.getPosition() * ROTARYSTEPS;

        if (newPos < ROTARYMIN) {
            encoder.setPosition(ROTARYMIN / ROTARYSTEPS);
            newPos = ROTARYMIN;
        } else if (newPos > ROTARYMAX) {
            encoder.setPosition(ROTARYMAX / ROTARYSTEPS);
            newPos = ROTARYMAX;
        }

        TARGET_TEMP = newPos;
     
        // Show Config mode on OLED
        display.clearDisplay();
        display.setFont(&FreeMonoBold12pt7b);
        display.setCursor(10, 14);
        display.print("Target:");
        display.setFont(&FreeMonoBold18pt7b);
        display.setCursor(6, 50);
        display.print(TARGET_TEMP);
        display.print(" C");
        display.display();
    } else {
        //publish a message roughly every second.
        if ((millis() - lastMillis) > 1000) {
            lastMillis = millis();
            // Show Working mode on OLED
            display.clearDisplay();
            display.setFont(&FreeMonoBold18pt7b);
            display.setCursor(10, 22);
            display.print(CURR_TEMP_F, 1);
            display.print("C");
            display.setFont(&FreeMono9pt7b);

            CURR_TEMP = (int)CURR_TEMP_F;
            int offset = ((120 - (info_text.length()*9))/2);
            display.setCursor(offset, 41);
            display.print(info_text);

            display.setCursor(8, 60);
            display.print("Target:");
            display.print(TARGET_TEMP);
            display.print("C");
            display.display();
            
            mqtt_time_counter = mqtt_time_counter + 1;
            if(mqtt_time_counter > mqtt_publish_time) {
                mqtt_time_counter = 0;
                publishMessage();
            }
        }
    }
}
