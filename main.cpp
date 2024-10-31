#include <Arduino.h>
#include <credentials.h>
#include <WiFi.h>
#include <time.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <UrlEncode.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>

///////////////////////////////////////////////////////////////////////////

#define VERSION "1.9.3"

#define DEBUG_SERIAL true                               // Enable debbuging over serial interface
#if DEBUG_SERIAL
#define debug(x) Serial.print(x)
#define debugf(x, ...) Serial.printf((x), ##__VA_ARGS__)
#define debugln(x) Serial.println(x)
#define debug_speed(x) Serial.begin(x)
#else
#define debug(x)
#define debugf(x, ...)
#define debugln(x)
#define debug_speed(x) 
#endif

#define ON false
#define OFF true 

// Defining GPIO PINS
#define COOL_DOWN 13                                    // GPIO for cooling down Relais
#define HEAT_UP 15                                      // GPIO for heating up Relais

#define BTN_TEMP_MINUS 14                               // GPIO for TEMP -1
#define BTN_TEMP_PLUS 27                                // GPIO for TEMP +1
#define BTN_PRESS_TIME 150                              // Milliseconds Button pressed

#define TEMP 32                                         // GPIO for OneWire-Bus
#define TEMPERATURE_PRECISION 10                        // Set Temp 12=0,0625°C(750ms) / 11=0,125°C(375ms) / 10=0,25(187,5ms) / 9=0.5°C(93,75ms)

// ======================================================================
// Setting parameters with default values
// ======================================================================


// Section for Wifi parameters
int WIFI_TIMEOUT_MS = 20000;                            // Wifi Timeout in msec
String RSSI_STATUS = "";                                // Wifi RSSI Status
String HOSTNAME = "ESP-Fermentation";                   // Enter Hostname here
TaskHandle_t keepWiFiAlive_handle;                      // Handle for keeping WiFi connection alive
String EXTERNAL_URL = "http://192.168.178.12:3000/d/cLwyHTWgz/fermentation-control-system?orgId=1"; // URL of external Website


// Section for temperature
float FRIDGE_TEMP_F = -111;
float ROOM_TEMP_F = -111;
float BEER_TEMP_F = -111;
float COMP_TEMP_F = -111;
int TARGET_TEMP = 10;                                   // Setting default target temp
int TEMP_HYSTERESIS = 1;                                // Parameter for temp hystersis 
boolean SHOW_COOL_DOWN = false;                         // Flag for cooling
boolean SHOW_HEAT_UP = false;                           // Flag for heating
static TimerHandle_t Temp_timer = NULL;                 // Handle to get temperatures
boolean SWITCH = true;           	                    // Enable Fermentation Control


// Section for OneWire sensors 
DeviceAddress COMP_TEMP = { 0x28, 0xAA, 0xFF, 0xB3, 0x55, 0x14, 0x01, 0x9A };
DeviceAddress CURR_TEMP = { 0x28, 0xAA, 0xFF, 0xB3, 0x55, 0x14, 0x01, 0x9A };
DeviceAddress ROOM_TEMP = { 0x28, 0xAA, 0xFF, 0xB3, 0x55, 0x14, 0x01, 0x9A };
DeviceAddress BEER_TEMP = { 0x28, 0xAA, 0xFF, 0xB3, 0x55, 0x14, 0x01, 0x9A };


// Section for Compressor and safety 
int COMP_RUNNING_TIME = 60;                             // Mininum Running Time of Compressor in sec
int COMP_TEMP_THRESHOLD = 50;                           // Max Temperature of compressor when switch off 
boolean COMP_OVERHEATED = false;                        // Compressor is overheated 
int comp_time_counter = COMP_RUNNING_TIME;              // time in seconds for cool switching
long lastCompMillis = 0; 
long lastMillis = 0;
int POWER_CONSUMPTION = 0;                              // Power consumption
static TimerHandle_t Compressor_timer = NULL;           // Handle for compressor switching task


// Section for carbonisation
float TARGET_CO2 = 5.1;                                 // Setting default target carbonisation
float PRESSURE = 0.0;                                   // Setting default target pressure 


// Section to set power consumption
int POWER_HEAT = 35;						            // Power consumption heating mode
int POWER_COOL = 70;						            // Power consumption cooling mode
int POWER_IDLE = 7;                                     // Power consumption idle mode


// Section to set MQTT related operation
String MQTT_BROKER = "192.168.178.12";                  // MQTT-Broker
const int mqtt_port = 1883;                             // MQTT Port
char mqtt_json_msg[128];                                // MQTT Message


// Button
portMUX_TYPE synch = portMUX_INITIALIZER_UNLOCKED;
volatile long lastButtonPressMS = 0;


static TimerHandle_t MQTT_timer = NULL;                 // Handle to send MQTT Messages

// Section für WhatApp notification
boolean WHATSAPP_NOTIFICATION = false;
String phoneNumber = "491719348490";
String apiKey = "6942851";

// Section to log last reboot
const char* NTP_SERVER = "de.pool.ntp.org";
const char* TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";
tm timeinfo;
char LAST_REBOOT[20];                                   // Last Reboot


// ======================================================================
// Interrupt Routines
// ======================================================================


void IRAM_ATTR btnTempMinusISR() {
    portENTER_CRITICAL_ISR(&synch);
    if ((millis() - lastButtonPressMS) > BTN_PRESS_TIME) {
        lastButtonPressMS = millis();
        TARGET_TEMP--;
        if(TARGET_TEMP < 0) { TARGET_TEMP = 0; }
    }
    portEXIT_CRITICAL_ISR(&synch);
}

void IRAM_ATTR btnTempPlusISR() {
    portENTER_CRITICAL_ISR(&synch);
    if ((millis() - lastButtonPressMS) > BTN_PRESS_TIME) {
        lastButtonPressMS = millis();
        TARGET_TEMP++;
        if(TARGET_TEMP > 25) { TARGET_TEMP = 25; }
    }
    portEXIT_CRITICAL_ISR(&synch);
}

// ======================================================================
// Initialize Objects
// ======================================================================

// Initializing a oneWire instance to communicate with any OneWire devices
OneWire oneWire(TEMP);
DallasTemperature sensors(&oneWire);

// Initialize WebServer
AsyncWebServer server(80);

// Initialize MQTT Publisher
WiFiClient espClient;
PubSubClient mqttClient(espClient);


// ======================================================================
// Functions
// ======================================================================

void loadConfig();
void saveConfig(AsyncWebServerRequest *request);
void saveConfig();

void printAddress(DeviceAddress deviceAddress);
void initDS1820(boolean manual);
void initRelayBoard();
void getTemp();
void getTemp(TimerHandle_t xTimer);

void switchCompressor(TimerHandle_t xTimer);

void setPressure();

void keepWiFiAlive();

void publishMessage(TimerHandle_t xTimer);
void initMQTTClient();

void sendWhatsAppNotification(String Nofication);

void startWebServer();
String responseData();
String HTML_Parser(const String& var);


// ======================================================================
// Setup
// ======================================================================

void setup() {
    debug_speed(115200);
    debugln("===SETUP_BEGIN===");
    debugf("ChipModel: %s (Rev.%i) with %i Core(s) and %i MHz\n", ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(), ESP.getCpuFreqMHz());
    debugf("Flash: %i Byte (free: %i Byte)\n", ESP.getFlashChipSize(), ESP.getFreePsram());
    debugf("Sketch Size: %i Byte / SDK Version %s\n", ESP.getSketchSize(), ESP.getSdkVersion());

    // Mounting LittleFS
    if (!LittleFS.begin(true)) { 
        debugln("-!An error occurred during LittleFS mounting!");
        return; 
    } else {
        debugln("-LittlsFS mounted");
    } 

    loadConfig();                                       // Load Config
    keepWiFiAlive();                                    // Connect to Wifi 

    //initDS1820(false);                                // Initialize Tempsensors - get sensor adresses
    initDS1820(true);                                   // Initialize Tempsensors - set precision
    initRelayBoard();                                   // Initialize Relay board
    initMQTTClient();

    // GPIO for TEMP -1
    pinMode(BTN_TEMP_MINUS, INPUT_PULLUP);         
    attachInterrupt(BTN_TEMP_MINUS, btnTempMinusISR, FALLING);
    
    // GPIO for TEMP +1
    pinMode(BTN_TEMP_PLUS, INPUT_PULLUP);         
    attachInterrupt(BTN_TEMP_PLUS, btnTempPlusISR, FALLING);

    // Start Temp timer
    Temp_timer = xTimerCreate(
        "Timer for getting Temperatures",               // Name of timer
        2000/portTICK_PERIOD_MS,                        // Period of timer (in ticks)
        pdTRUE,                                         // Auto-reload
        (void *)1,                                      // Timer ID
        getTemp);                                       // Callback function

    // Start Compressor timer
    Compressor_timer = xTimerCreate(
        "Timer for Compressor",                         // Name of timer
        COMP_RUNNING_TIME*1000/portTICK_PERIOD_MS,      // Period of timer (in ticks)
        pdTRUE,                                         // Auto-reload
        (void *)2,                                      // Timer ID
        switchCompressor);                              // Callback function


    // Start MQTT timer
    MQTT_timer = xTimerCreate(
            "Timer for sending MQTT messages",          // Name of timer
            45000/portTICK_PERIOD_MS,                   // Period of timer (in ticks)
            pdTRUE,                                     // Auto-reload
            (void *)2,                                  // Timer ID
            publishMessage);                            // Callback function

    
    if( ( Temp_timer != NULL ) && ( MQTT_timer != NULL ) && ( Compressor_timer != NULL) ) { 
        // Start the software timers, using a block time of 0 (no block time). 
        xTimerStart(Temp_timer, 500);
        xTimerStart(MQTT_timer, portMAX_DELAY);
        xTimerStart(Compressor_timer, 10000);
    }

    startWebServer();                                   // Start WebServer

    // Get current time information
    configTime(0, 0, NTP_SERVER);
    setenv("TZ", TZ_INFO, 1);
    getLocalTime(&timeinfo);
    sprintf(LAST_REBOOT, "%02d.%02d.%04d %02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    debugf("-Reboot at: %s\n", LAST_REBOOT);
    sendWhatsAppNotification("booted");
    debugln("===SETUP_END===");
}


// ======================================================================
// Loop
// ======================================================================

void loop() {
    
    // Temp control of the fridge
    if (SWITCH) {
        if((int)round(COMP_TEMP_F) < COMP_TEMP_THRESHOLD) { 
            COMP_OVERHEATED = false;
            
            if (FRIDGE_TEMP_F  > TARGET_TEMP + TEMP_HYSTERESIS) { SHOW_COOL_DOWN = true; }

            if (FRIDGE_TEMP_F <= TARGET_TEMP - TEMP_HYSTERESIS) { SHOW_COOL_DOWN = false; } 

            if ((ROOM_TEMP_F <= TARGET_TEMP) && (FRIDGE_TEMP_F < TARGET_TEMP - TEMP_HYSTERESIS)) {                    
                digitalWrite(HEAT_UP, ON);
                SHOW_HEAT_UP = true;
            } else {
                digitalWrite(HEAT_UP, OFF);
                SHOW_HEAT_UP = false;
            }

        } else {
            COMP_OVERHEATED = true;            // Switch Compressor off, when overheated
            SWITCH = false;
            sendWhatsAppNotification("overheated");
        }
    } else {
        digitalWrite(COOL_DOWN, OFF);      // Switch off compressor immidiatly
        SHOW_COOL_DOWN = false; 
        SHOW_HEAT_UP = false; 
    }

    if(SHOW_COOL_DOWN) {
        POWER_CONSUMPTION = POWER_COOL;
    } else if (SHOW_HEAT_UP) {
        POWER_CONSUMPTION = POWER_HEAT;
    } else {
        POWER_CONSUMPTION = POWER_IDLE;
    }
  
}

// ======================================================================
// Functions
// ======================================================================

// function for loading configuration from file system
void loadConfig(){
 
  File config = LittleFS.open("/config.json","r");

  if(config) { 
    // Deserialize the JSON document
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, config);

    if (error) { 
      debugf("-! DeserializeJson failed! -> %s/n", error.f_str());
      return; 
    }
    JsonObject obj = doc.as<JsonObject>();

    HOSTNAME = doc["HOSTNAME"].as<String>();            
    //String hostname = obj["HOSTNAME"];
    //HOSTNAME = hostname.c_str();
    String mqtt_broker = doc["MQTT_BROKER"];     
    MQTT_BROKER = mqtt_broker.c_str();
    String ext_url = doc["EXTERNAL_URL"];     
    EXTERNAL_URL = ext_url.c_str();
    WHATSAPP_NOTIFICATION = doc["WHATSAPP_NOTIFICATION"];
    TARGET_TEMP = doc["TARGET_TEMP"];
    TARGET_CO2 = doc["TARGET_CO2"];
    TEMP_HYSTERESIS = doc["TEMP_HYSTERESIS"];
    COMP_RUNNING_TIME = doc["COMP_RUNNING_TIME"];
    COMP_TEMP_THRESHOLD = doc["COMP_TEMP_THRESHOLD"];
    POWER_HEAT = doc["POWER_HEAT"];
    POWER_COOL = doc["POWER_COOL"];
    POWER_IDLE = doc["POWER_IDLE"];

    debugf("-Config loaded [WHATSAPP_NOTIFICATION=%i]\n", WHATSAPP_NOTIFICATION);    
    }
  config.close();
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++)
  {
    debugf("0x%02X, ", deviceAddress[i]);
  }
}

// function to initialize DS1820 sensors
void initDS1820(boolean manual){
    sensors.begin();

    if(manual) {
        if(sensors.isConnected(COMP_TEMP)) sensors.setResolution(COMP_TEMP, TEMPERATURE_PRECISION);
        if(sensors.isConnected(CURR_TEMP)) sensors.setResolution(CURR_TEMP, TEMPERATURE_PRECISION);
        if(sensors.isConnected(ROOM_TEMP)) sensors.setResolution(ROOM_TEMP, TEMPERATURE_PRECISION);
        if(sensors.isConnected(BEER_TEMP)) sensors.setResolution(BEER_TEMP, TEMPERATURE_PRECISION);

    } else {
        debugln("Searching for 1-Wire devices...");
        if (sensors.getAddress(COMP_TEMP, 0)) {
            debug("Device 0  has address: ");
            printAddress(COMP_TEMP);       
            debugln();
            sensors.setResolution(COMP_TEMP, TEMPERATURE_PRECISION);

        } else { 
            debugln("Unable to find address for Device 0");
        }

        if (sensors.getAddress(CURR_TEMP, 1)) {
            debug("Device 1  has address: ");
            printAddress(CURR_TEMP);
            debugln();
            sensors.setResolution(CURR_TEMP, TEMPERATURE_PRECISION);
        } else {
            debugln("Unable to find address for Device 1");
        }

        if (sensors.getAddress(ROOM_TEMP, 2)) {
            debug("Device 2  has address: ");
            printAddress(ROOM_TEMP);
            debugln();
            sensors.setResolution(ROOM_TEMP, TEMPERATURE_PRECISION);
        } else {
            debugln("Unable to find address for Device 2");
        }

        if (sensors.getAddress(BEER_TEMP, 3)) {
            debug("Device 3  has address: ");
            printAddress(BEER_TEMP);
            debugln();
            sensors.setResolution(BEER_TEMP, TEMPERATURE_PRECISION);
        } else {
            debugln("Unable to find address for Device 3");
        }
    }

   // getTemp();
}

// function to initialize Relay board
void initRelayBoard() {
    pinMode(COOL_DOWN, OUTPUT_OPEN_DRAIN);
    digitalWrite(COOL_DOWN, HIGH);
    pinMode(HEAT_UP, OUTPUT_OPEN_DRAIN);
    digitalWrite(HEAT_UP, HIGH);
    
    debugln("-Relay board initialized!");
}

// function to get temperature
void getTemp() {
    sensors.requestTemperatures();
    
    float temp_comp = sensors.getTempC(COMP_TEMP);
    if (COMP_TEMP_F == -111 || temp_comp > COMP_TEMP_F-3 && temp_comp < COMP_TEMP_F+3) {
        COMP_TEMP_F = temp_comp;
    } else {
        debugln("Failure on Sensor 'COMP_TEMP'!");
    }

    float temp_fridge = sensors.getTempC(CURR_TEMP);
    if (FRIDGE_TEMP_F == -111 || temp_fridge > FRIDGE_TEMP_F-3 && temp_fridge < FRIDGE_TEMP_F+3) { 
        FRIDGE_TEMP_F  = temp_fridge;
    } else {
        debugln("Failure on Sensor 'CURR_TEMP'!");
    }

    float temp_room = sensors.getTempC(ROOM_TEMP);
    if (ROOM_TEMP_F == -111 || temp_room > ROOM_TEMP_F-3 && temp_room < ROOM_TEMP_F+3) { 
        ROOM_TEMP_F = temp_room;
    } else {
        debugln("Failure on Sensor 'ROOM_TEMP'!");
    }

    float temp_beer = sensors.getTempC(BEER_TEMP);
    if (BEER_TEMP_F == -111 || temp_beer > BEER_TEMP_F-3 && temp_beer < BEER_TEMP_F+3) { 
        BEER_TEMP_F = temp_beer;
    } else {
        debugln("Failure on Sensor 'BEER_TEMP'!");
    }

    if (temp_fridge == DEVICE_DISCONNECTED_C) {
        initDS1820(true);
    } else {
        FRIDGE_TEMP_F = temp_fridge;
    }
}

// function to get temperature with timer handle
void getTemp(TimerHandle_t xTimer) {
  getTemp();
  debugf("Temp Timer-> Target: %d°C|", TARGET_TEMP);
  debugf("Fridge: %f°C|", FRIDGE_TEMP_F);
  debugf("Beer: %f°C|",BEER_TEMP_F);
  debugf("Room: %f°C|", ROOM_TEMP_F);
  debugf("Comp: %f°C\n", COMP_TEMP_F);
}

// function to switch compressor on/off with timer handle
void switchCompressor(TimerHandle_t xTimer) {

    if(SHOW_COOL_DOWN) {
        digitalWrite(COOL_DOWN, ON);
    } else {
        digitalWrite(COOL_DOWN, OFF);
    }
    debugf("Switch Compressor: %d\n", SHOW_COOL_DOWN);
}

// function to keep wifi alive 
void keepWiFiAlive(){
	 
    String wifi_status[] = {"WL_IDLE_STATUS", "WL_NO_SSID_AVAIL", "WL_SCAN_COMPLETED", "WL_CONNECTED", "WL_CONNECT_FAILED", "WL_CONNECTION_LOST", "WL_DISCONNECTED"};
    debugf("-WiFi status: %s\n", wifi_status[WiFi.status()]);
    
    
    if(WiFi.status() != WL_CONNECTED) {

        // try to connect Wifi
    	WiFi.setHostname(HOSTNAME.c_str());
		WiFi.mode(WIFI_STA);
	    WiFi.begin(WIFI_SSID, WIFI_PW);

        debugf("-Connecting to WiFi '%s' ... ", WIFI_SSID); 
    	unsigned long startAttempTime = millis();

    	while(WiFi.status() != WL_CONNECTED && millis() - startAttempTime < WIFI_TIMEOUT_MS){};

        if(WiFi.status() != WL_CONNECTED) {     
      		debugln("failed!");
      	} else { 
            debugf("successful (Hostname:%s & IP:%s)\n", WiFi.getHostname(), WiFi.localIP().toString().c_str());
            sendWhatsAppNotification("reconnected");

            // Make it possible to access webserver at http://HOSTNAME.local
            if (!MDNS.begin(HOSTNAME)) {
                debugln("-Error setting up mDNS responder!");
            } else {
                debugf("-Access at http://%s.local\n", HOSTNAME);
            }
        }
    } 

    String RSSI_percent = String((100 + WiFi.RSSI())*2);
    RSSI_STATUS = "RSSI:" + String(WiFi.RSSI()) + "dBm(" + RSSI_percent + "%)";
}

// function to publish MQTT message 
void publishMessage(TimerHandle_t xTimer) {

    //display.setCursor(119, 10);
    keepWiFiAlive();
    
    if(!mqttClient.connected()) {
        int b = 0;
        debug("Connecting to MQTT Broker");
  
        while (!mqttClient.connected()) {
            mqttClient.connect(HOSTNAME.c_str());
            debug(".");      
            delay(100);

            b = b + 1;
            if(b > 4) {
                b = 0;
                debug("Failed");
                break;
            } else {
                debug("OK");
            }     
        }
        debugf(" [%i]!\n", mqttClient.state());
        
    }
    mqttClient.loop();

    if (mqttClient.connect(HOSTNAME.c_str())) {
        JsonDocument doc;                            // build JSON object
        doc["sender"] = HOSTNAME;
        doc["target_temp"] = TARGET_TEMP;
        doc["room_temp"] = ROOM_TEMP_F;
        doc["comp_temp"] = COMP_TEMP_F;
        doc["fridge_temp"] = FRIDGE_TEMP_F;
        doc["beer_temp"] = BEER_TEMP_F;

        int COOL_HEAT = 10;
        if(SHOW_COOL_DOWN) { COOL_HEAT = 0; }
        if(SHOW_HEAT_UP) { COOL_HEAT = 20; }
        doc["cool_heat"] = COOL_HEAT;

        int msg_size = serializeJson(doc, mqtt_json_msg);

        mqttClient.publish("test", mqtt_json_msg, msg_size);
     
        //display.print("*");
    } 
    //display.display();
    debugln("Send MQTT Message");
}

// function to initialize the MQTT client
void initMQTTClient() {
    mqttClient.setServer(MQTT_BROKER.c_str(), 1883);
    
    debugln("-MQTT Client initialized!");
}

// function to send notifications via WhatsApp
void sendWhatsAppNotification(String Notification) {

    if (WHATSAPP_NOTIFICATION) {
        String message;
        if (Notification == "booted") {
            message = "*" + HOSTNAME + ":* Booted at " + LAST_REBOOT;
            //sprintf(message, "%s: Booted at %s"), HOSTNAME, LAST_REBOOT;
            
        } else if ("reconnected") {
            message = "*" + HOSTNAME + ":* Wifi (re)connected! (http://"+HOSTNAME+".local)";

        }else if ("overheated") {
        message = "*" + HOSTNAME + ":* Compressor overheated: *" + COMP_TEMP_F +"°C!*";

        } else {
            message = "*" + HOSTNAME + ": default*";
        };


        // Data to send with HTTP POST
        String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + urlEncode(message);    
        HTTPClient http;
        http.begin(url);

        // Specify content-type header
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
        // Send HTTP POST request
        int httpResponseCode = http.POST(url);
        if (httpResponseCode == 200){
            debugln("WhatsApp Notification sent successfully!");
        } else {
            debug("Error sending the WhatsApp Notification-> HTTP response code: ");
            debugln(httpResponseCode);
        }

        // Free resources
        http.end();
    }
}

// function for parsing the HTML-pages
String HTML_Parser(const String& var) {

    if(var == "HOSTNAME"){ return HOSTNAME; }
    if(var == "EXTERNAL_URL"){ return EXTERNAL_URL; }
    if(var == "ROOM_TEMP"){  return String(ROOM_TEMP_F, 1);  }

    if(var == "COMP_COLOR") {
        if (COMP_TEMP_F < COMP_TEMP_THRESHOLD*0.8) {
            return "green";
        } else if (COMP_TEMP_F >= COMP_TEMP_THRESHOLD*0.8 && COMP_TEMP_F < COMP_TEMP_THRESHOLD) {
            return "amber";
        } else {
            return "red";
        }
    }

    if(var=="WHATSAPP_TRUE") {
        String whatsapp_true = "";
        if(WHATSAPP_NOTIFICATION == true) {whatsapp_true = " selected"; }
        return whatsapp_true;
    }

    if(var=="WHATSAPP_FALSE") {
        String whatsapp_false = "";
        if(WHATSAPP_NOTIFICATION == false) {whatsapp_false = " selected"; }
        return whatsapp_false;
    }

    if(var == "COMP_TEMP"){  return String(COMP_TEMP_F, 1);  }
    if(var == "TARGET_TEMP"){  return String(TARGET_TEMP);  }
    if(var == "TARGET_CO2"){  return String(TARGET_CO2, 1);  }
    if(var == "FRIDGE_TEMP"){  return String(FRIDGE_TEMP_F, 1);  }
    if(var == "BEER_TEMP"){  return String(BEER_TEMP_F, 1);  }
  
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
        
    if(var == "WIFI_SSID"){  return String(WIFI_SSID);  }
    if(var == "WIFI_PW"){  return String(WIFI_PW);  }
    if(var == "MQTT_BROKER"){  return String(MQTT_BROKER);  }
    if(var == "TEMP_HYSTERESIS"){  return String(TEMP_HYSTERESIS);  }
    if(var == "COMP_RUNNING_TIME"){  return String(COMP_RUNNING_TIME);  }
    if(var == "COMP_TEMP_THRESHOLD"){  return String(COMP_TEMP_THRESHOLD);  }
    if(var == "POWER_HEAT"){  return String(POWER_HEAT);  }
    if(var == "POWER_COOL"){  return String(POWER_COOL);  }
    if(var == "POWER_IDLE"){  return String(POWER_IDLE);  }

    if(var == "CPU") {  return String(ESP.getChipModel()) + " (Rev." + String(ESP.getChipRevision()) + ")"; }
    if(var == "MEMORY") {  return String(ESP.getFlashChipSize())+ " Byte";  }
    if(var == "SDK") {  return String(ESP.getSdkVersion());  }
    if(var == "RSSI_DBM") {  return String(WiFi.RSSI()) + "dBm"; }
    if(var == "RSSI_PERCENT") {  return String((100 + WiFi.RSSI())*2); } //  + "%"
    if(var == "LAST_REBOOT") {  return String(LAST_REBOOT); }
    if(var == "VERSION"){  return String(VERSION);  }
    

    return String();   
}

// function to start the WebServer
void startWebServer(){
   
    // Make test available
    server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request){
        debugln("Requested test page");
        request->send(200, "text/plain", "Hello world!");
    }); 

    // Make index.html available
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        debugln("Requested index.html page");
        request->send(LittleFS, "/index.html", String(), false, HTML_Parser);
    });

    // Make config.html available
    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
        debugln("Requested config.html page");
        request->send(LittleFS, "/config.html", String(), false, HTML_Parser);
    });

    // Make check.html available
    server.on("/check", HTTP_GET, [](AsyncWebServerRequest *request){
        debugln("Requested check.html page");
        request->send(LittleFS, "/check.html", String(), false, HTML_Parser);
    });

    // Make calc.html available
    server.on("/calc", HTTP_GET, [](AsyncWebServerRequest *request){
        debugln("Requested calc.html page");
        request->send(LittleFS, "/calc.html", String(), false, HTML_Parser);
    });

    // Save config-parameters
    server.on("/save-config", HTTP_GET, [](AsyncWebServerRequest *request){
        debugln("Save config request");
        saveConfig(request);
        request->redirect("/config");
    });

    // Reboot ESP
    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){       
        debugln("!!!Rebooting ESP!!!");
        request->send(200, "text/plain", "Rebooting ESP!");
        ESP.restart();
    });



    // Make style.css available
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/style.css","text/css");
    });

    // Enable / Disable Fermentation Control
    server.on("/switch", HTTP_GET, [](AsyncWebServerRequest *request){       
        SWITCH = !SWITCH;
        request->redirect("/");
    });

    // Make get data available
    server.on("/getdata", HTTP_GET, [](AsyncWebServerRequest *request){    

        String output = responseData();
        //debugln("/data response -> "+ output);
        request->send(200, "application/json", output);
    });

    // Make set data available
    server.on("/setdata", HTTP_GET, [](AsyncWebServerRequest *request){       
        
        int paramsNr = request->params();
       
        for(int i=0;i<paramsNr;i++){
            const AsyncWebParameter* p = request->getParam(i);
            
            if(p->name() == "TARGET_TEMP") {
                TARGET_TEMP = p->value().toInt();
            }
            if(p->name() == "TARGET_CO2") {
                TARGET_CO2 = p->value().toFloat();
            }
            if(p->name() == "SWITCH") {
                if (p->value() == "true"){
                    SWITCH = true;
                } else {
                    SWITCH = false;
                }
            }
        }    
        debugf("/data request -> TARGET_TEMP:%d + TARGET_CO2:%1.1f + Switch:%i\n", TARGET_TEMP, TARGET_CO2, SWITCH);

        String output = responseData();
        //debugln("/data response -> "+ output);
        request->send(200, "application/json", output);
    });

    // Make get log-data available
    server.on("/getlog", HTTP_GET, [](AsyncWebServerRequest *request){             
        debug("/getlog requested!");
        request->send(LittleFS, "/logfile.log","text");
    });

    // Make SMA Power Monitor available
    server.on("/power_meter", HTTP_GET, [](AsyncWebServerRequest *request){       
		JsonDocument doc;
        int power_consumption = POWER_IDLE;
        
        if(SHOW_HEAT_UP) { power_consumption = POWER_HEAT; }
        if(SHOW_COOL_DOWN) { power_consumption = POWER_COOL; }
               
        doc["power"] = power_consumption;
      
        String buf;
        serializeJson(doc, buf);
        request->send(200, "application/json", buf);
    });

    // Make favicon.ico available
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        debugln("Requested favicon.ico page");
        request->send(LittleFS, "/hop-g.ico","image");
    });


    // Starting WebServer
    esp_netif_init();
    server.begin();
    //ElegantOTA.begin(&server);

    debugln("-WebServer started!");
}

// function to resonpse JSON data to the Browser
String responseData() {

        // Allocate a temporary JsonDocument
        JsonDocument data;

        data["SWITCH"] = SWITCH;
        data["TARGET_TEMP"] = TARGET_TEMP;
        data["TARGET_CO2"] = TARGET_CO2;
        data["POWER_CONSUMPTION"] = POWER_CONSUMPTION;
        data["ROOM_TEMP"] = String(ROOM_TEMP_F, 1);
        data["COMP_TEMP"] = String(COMP_TEMP_F, 1);

        // calculate color of compressor temperature 
        String COMP_TEMP_COLOR;
        if (COMP_TEMP_F < COMP_TEMP_THRESHOLD*0.8) {
            COMP_TEMP_COLOR = "green";
        } else if (COMP_TEMP_F >= COMP_TEMP_THRESHOLD*0.8 && COMP_TEMP_F < COMP_TEMP_THRESHOLD) {
        COMP_TEMP_COLOR =  "amber";
        } else {
        COMP_TEMP_COLOR = "red";
        }
        data["COMP_TEMP_COLOR"] = COMP_TEMP_COLOR;

        data["BEER_TEMP"] = String(BEER_TEMP_F, 1);
        data["FRIDGE_TEMP"] = String(FRIDGE_TEMP_F, 1);
        data["SHOW_COOL_DOWN"] = SHOW_COOL_DOWN;
        data["SHOW_HEAT_UP"] = SHOW_HEAT_UP;

        // Serialize JSON document to String
        String output;
        serializeJsonPretty(data, output);
        saveConfig();
        return output;
}

// function to persist save config parameters on file system from webserver
void saveConfig(AsyncWebServerRequest *request) {

    File config = LittleFS.open("/config.json","w");

    if(config) { 
        // Serialize the JSON document
        JsonDocument doc;

        int paramsNr = request->params();
        for(int i=0;i<paramsNr;i++){
            const AsyncWebParameter* p = request->getParam(i);
            
            if(p->name() == "HOSTNAME") {
                HOSTNAME = p->value();
                doc["HOSTNAME"] = HOSTNAME;
            }

            if(p->name() == "MQTT_BROKER") {
                MQTT_BROKER = p->value();
                doc["MQTT_BROKER"] = MQTT_BROKER;
            }

            if(p->name() == "EXTERNAL_URL") {
                EXTERNAL_URL = p->value();
                doc["EXTERNAL_URL"] = EXTERNAL_URL;
            }

            if(p->name() == "WHATSAPP_NOTIFICATION") {
                int wa_n = p->value().toInt();
                if (wa_n == 0) {
                    WHATSAPP_NOTIFICATION = false;
                } else {
                    WHATSAPP_NOTIFICATION = true;
                }
                doc["WHATSAPP_NOTIFICATION"] = WHATSAPP_NOTIFICATION;

                debugf("/saveConfig -> WHATSAPP_NOTIFICATION:%i\n", p->value().toInt());
            }


            if(p->name() == "TEMP_HYSTERESIS") {
                TEMP_HYSTERESIS = p->value().toInt();
                doc["TEMP_HYSTERESIS"] = TEMP_HYSTERESIS;
            }

            if(p->name() == "COMP_RUNNING_TIME") { 
                COMP_RUNNING_TIME = p->value().toInt(); 
                doc["COMP_RUNNING_TIME"] = COMP_RUNNING_TIME; 
            }

            if(p->name() == "COMP_TEMP_THRESHOLD") { 
                COMP_TEMP_THRESHOLD = p->value().toInt(); 
                doc["COMP_TEMP_THRESHOLD"] = COMP_TEMP_THRESHOLD; 
            }

            if(p->name() == "TARGET_TEMP") {
                TARGET_TEMP = p->value().toInt();
                doc["TARGET_TEMP"] = TARGET_TEMP;
            }

            if(p->name() == "TARGET_CO2") {
                TARGET_TEMP = p->value().toFloat();
                doc["TARGET_CO2"] = TARGET_CO2;
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

// function to persist save config parameters on file system
void saveConfig() {

    File config = LittleFS.open("/config.json","w");
    if(config) { 
        // Serialize the JSON document
        JsonDocument doc;
        doc["HOSTNAME"] = HOSTNAME;
        doc["MQTT_BROKER"] = MQTT_BROKER;
        doc["EXTERNAL_URL"] = EXTERNAL_URL;
        doc["WHATSAPP_NOTIFICATION"] = WHATSAPP_NOTIFICATION;
        doc["TEMP_HYSTERESIS"] = TEMP_HYSTERESIS;
        doc["COMP_RUNNING_TIME"] = COMP_RUNNING_TIME;
        doc["COMP_TEMP_THRESHOLD"] = COMP_TEMP_THRESHOLD;
        doc["TARGET_TEMP"] = TARGET_TEMP;
        doc["TARGET_CO2"] = TARGET_CO2;
        doc["POWER_HEAT"] = POWER_HEAT;
        doc["POWER_COOL"] = POWER_COOL;
        doc["POWER_IDLE"] = POWER_IDLE;                
        serializeJsonPretty(doc, config);
    }   
    config.close();
}

// function to calculate the pressure
void setPressure(){
    
    float CO2 = TARGET_CO2 / 10;
    PRESSURE = CO2 / exp(-10.73797 + (2617.25 / (TARGET_TEMP + 273.15))) - 1.013;
    PRESSURE = round(PRESSURE*10)/10;
}