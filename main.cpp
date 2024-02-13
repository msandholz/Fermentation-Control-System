#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <ElegantOTA.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RotaryEncoder.h>

///////////////////////////////////////////////////////////////////////////

#define VERSION "1.9"

#define DEBUG_SERIAL true      // Enable debbuging over serial interface
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

#define DEBUG_OLED true         // Enable debbuging over serial interface
#if DEBUG_OLED 
#define debug_oled(x) display.print(x);
#define debugln_oled(x) display.println(x);
#define debugf_oled(x, ...) display.printf((x), ##__VA_ARGS__);
#define debug_oled_show() display.display();
#else
#define debug_oled(x)
#define debugln_oled(x)
#define debugf_oled(x)
#define debug_oled_show()
#endif

#define SCREEN_WIDTH 128 		// OLED display width, in pixels
#define SCREEN_HEIGHT 64 		// OLED display height, in pixels
#define OLED_RESET -1 			// Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C 	// See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32        

#define ON false
#define OFF true 

// Defining GPIO PINS

#define ROTARY_CLK 26           // GPIO for rotary encoder clock
#define ROTARY_DT 25            // GPIO for rotary encoder dt
#define ROTARY_BUTTON 27        // GPIO for rotary encoder button 6
#define DEBOUNCE_TIME 250       // Milliseconds Button pressed
#define ROTARYMIN 1             // Minimal Rotary value 
#define ROTARYMAX 25            // Maximal Rotary value 

#define COOL_DOWN 2             // GPIO for cooling down Relais
#define HEAT_UP 15              // GPIO for heating up Relais

#define TEMP 32                 // GPIO for OneWire-Bus

#define I2C_SDA 12              // GPIO for OLED Display
#define I2C_SCL 14              // GPIO for OLED Display

#define TEMPERATURE_PRECISION 10 // Set Temp 12=0,0625°C(750ms) / 11=0,125°C(375ms) / 10=0,25(187,5ms) / 9=0.5°C(93,75ms)

// ======================================================================
// Setting parameters with default values
// ======================================================================

// Section for Wifi parameters
const char* WIFI_SSID = "xxx";                         // Wifi-SSID
const char* WIFI_PW = "xxx";           // Wifi-Password
int WIFI_TIMEOUT_MS = 20000;                            // Wifi Timeout in msec
String RSSI_STATUS = "";                                // Wifi RSSI Status
String HOSTNAME = "ESP-Fermentation";                   // Enter Hostname here
TaskHandle_t keepWiFiAlive_handle;                      // Handle for keeping WiFi connection alive
String EXTERNAL_URL = "http://192.168.178.12:3000/d/cLwyHTWgz/fermentation-control-system?orgId=1";              // URL of external Website

// Section for temperatures

DeviceAddress COMP_TEMP = { 0x28, 0xB4, 0xD1, 0x07, 0xD6, 0x01, 0x3C, 0xC1 };
DeviceAddress CURR_TEMP = { 0x28, 0xB1, 0x29, 0x57, 0x04, 0xE1, 0x3C, 0x49 };
DeviceAddress ROOM_TEMP = { 0x28, 0xF7, 0x93, 0x07, 0xD6, 0x01, 0x3C, 0x87 };

/*
DeviceAddress COMP_TEMP { 0x28, 0xAA, 0xFF, 0xB3, 0x55, 0x14, 0x01, 0x9A };
DeviceAddress CURR_TEMP { 0x28, 0xAA, 0xFF, 0xB3, 0x55, 0x14, 0x01, 0x9A };
DeviceAddress ROOM_TEMP { 0x28, 0xAA, 0xFF, 0xB3, 0x55, 0x14, 0x01, 0x9A };
*/

float CURR_TEMP_F = -111;
float ROOM_TEMP_F = -111;
float COMP_TEMP_F = -111;
int TARGET_TEMP = 10;                                   // Setting default target temp
int TEMP_HYSTERESIS = 1;                                // Parameter for temp hystersis 
boolean SHOW_COOL_DOWN = false;                         // Flag for cooling
boolean SHOW_HEAT_UP = false;                           // Flag for heating
static TimerHandle_t Temp_timer = NULL;                 // Handle to get temperatures

float TARGET_CO2 = 5.1;                                 // Setting default target carbonisation
float PRESSURE = 0.0;                                   // Setting default target pressure                                    

// Section for Config Mode
boolean SWITCH = true;           	                    // Enable Fermentation Control
volatile boolean CONFIG_MODE = false;                   // Config Mode
volatile boolean SHOW_FIRST = false;                    // Display when SONFIG_MODE is entered

portMUX_TYPE synch = portMUX_INITIALIZER_UNLOCKED;
volatile long lastButtonPressMS = 0;

// Section for Compressor and safety 
int COMP_RUNNING_TIME = 60;                             // Mininum Running Time of Compressor in sec
int COMP_TEMP_THRESHOLD = 50;                           // Max Temperature of compressor when switch off 
int comp_time_counter = COMP_RUNNING_TIME;              // time in seconds for cool switching
long lastCompMillis = 0; 
long lastMillis = 0;
static TimerHandle_t Compressor_timer = NULL;           // Handle for compressor switching task


// Section to set MQTT related operation
String MQTT_BROKER = "192.168.178.12";                  // MQTT-Broker
char mqtt_json_msg[128];                                // MQTT Message
static TimerHandle_t MQTT_timer = NULL;                 // Handle to send MQTT Messages

// Section to set power consumption
int POWER_HEAT = 35;						            // Power consumption heating mode
int POWER_COOL = 70;						            // Power consumption cooling mode
int POWER_IDLE = 7;							            // Power consumption idle mode

// Section to log last reboot
const char* NTP_SERVER = "de.pool.ntp.org";
const char* TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";
tm timeinfo;
char LAST_REBOOT[20];                                   // Last Reboot


// ======================================================================
// Initialize Objects
// ======================================================================


// Initialize SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Initialize WebServer
AsyncWebServer server(80);

// Initializing a oneWire instance to communicate with any OneWire devices
OneWire oneWire(TEMP);
DallasTemperature sensors(&oneWire);

// Initializing Rotary Encoder
RotaryEncoder encoder(ROTARY_CLK, ROTARY_DT, RotaryEncoder::LatchMode::TWO03);
int lastPos = -1;

// Initialize MQTT Publisher
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ======================================================================
// Interrupt Routines
// ======================================================================

void IRAM_ATTR btnRotaryISR() {
    portENTER_CRITICAL_ISR(&synch);
    if ((millis() - lastButtonPressMS) > DEBOUNCE_TIME) {
        lastButtonPressMS = millis();
        CONFIG_MODE = !CONFIG_MODE;
        SHOW_FIRST = true;
    } 
    portEXIT_CRITICAL_ISR(&synch);
}

// ======================================================================
// Functions
// ======================================================================

void initDisplay();
void initRelayBoard();
void initRotaryEncoder();
void initMQTTClient();

void loadConfig();
void saveConfig(AsyncWebServerRequest *request);
void saveConfig();

void keepWiFiAlive(void * pvparameters);
void startWebServer();
String HTML_Parser(const String& var);

void setPressure();
void initDS1820(boolean manual);
void printAddress(DeviceAddress deviceAddress);
void getTemp();
void getTemp(TimerHandle_t xTimer);
void publishMessage(TimerHandle_t xTimer);
void switchCompressor(TimerHandle_t xTimer);


//boolean switchCompressor(boolean pinStateRequest);

// ======================================================================
// Setup
// ======================================================================
void setup() {
  	// put your setup code here, to run once:
  	debug_speed(115200);
    debugln("===SETUP_BEGIN===");
  	debugf("ChipModel: %s (Rev.%i) with %i Core(s) and %i MHz\n", ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(), ESP.getCpuFreqMHz());
  	debugf("Flash: %i Byte (free: %i Byte)\n", ESP.getFlashChipSize(), ESP.getFreePsram());
    debugf("Sketch Size: %i Byte / SDK Version %s\n", ESP.getSketchSize(), ESP.getSdkVersion());
   
    initDisplay();              // Initialize Display
   
    // Mounting LittleFS
    if (!LittleFS.begin(true)) { 
        debugln("-!An error occurred during LittleFS mounting!");
        debugln_oled("-!LittlsFS ERROR");
        debug_oled_show();
        return; 
    } else {
        debugln("-LittlsFS mounted");
        debugln_oled("-LittlsFS mounted  OK");
        debug_oled_show();
    }

    loadConfig();
  
    initRelayBoard();           // Initialize Relais Board
    initRotaryEncoder();        // Intitalize RotaryEncoder
    initMQTTClient();           // Initialize MQTT Client
    initDS1820(true);           // Initialize Tempsensors
  
	xTaskCreatePinnedToCore(keepWiFiAlive, "Keep WiFi alive", 4096, NULL, 10, &keepWiFiAlive_handle, 0);
	startWebServer();

    // Start Temp timer
    Temp_timer = xTimerCreate(
            "Timer for getting Temperatures",           // Name of timer
            2000/portTICK_PERIOD_MS,                    // Period of timer (in ticks)
            pdTRUE,                                     // Auto-reload
            (void *)1,                                  // Timer ID
            getTemp);                                   // Callback function

    // Start MQTT timer
    MQTT_timer = xTimerCreate(
            "Timer for sending MQTT messages",          // Name of timer
            45000/portTICK_PERIOD_MS,                   // Period of timer (in ticks)
            pdTRUE,                                     // Auto-reload
            (void *)2,                                  // Timer ID
            publishMessage);                            // Callback function

    // Start MQTT timer
    Compressor_timer = xTimerCreate(
            "Timer for Compressor",                     // Name of timer
            COMP_RUNNING_TIME*1000/portTICK_PERIOD_MS,  // Period of timer (in ticks)
            pdTRUE,                                     // Auto-reload
            (void *)3,                                  // Timer ID
            switchCompressor);                          // Callback function

    
    if( ( Temp_timer != NULL ) && ( MQTT_timer != NULL ) && ( Compressor_timer != NULL) ) { 
        // Start the software timers, using a block time of 0 (no block time). 
        xTimerStart(Temp_timer, 500);
        xTimerStart(MQTT_timer, portMAX_DELAY);
        xTimerStart(Compressor_timer, 10000);
    }
    
    // Get current time information
    configTime(0, 0, NTP_SERVER);
    setenv("TZ", TZ_INFO, 1);
    getLocalTime(&timeinfo);
    sprintf(LAST_REBOOT, "%02d.%02d.%04d %02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    debugf("Reboot at: %s\n", LAST_REBOOT);

    debugln("===SETUP_END===");
}

// ======================================================================
// Loop
// ======================================================================
void loop() {

    // Switch Compressor of, if overheated
    if((int)round(COMP_TEMP_F) > COMP_TEMP_THRESHOLD) { 
        SWITCH = false; 
    }

    // Temp control of the fridge
    if (SWITCH) {
        if (CURR_TEMP_F  > TARGET_TEMP + TEMP_HYSTERESIS) { 
            SHOW_COOL_DOWN = true; }

        if (CURR_TEMP_F <= TARGET_TEMP - TEMP_HYSTERESIS) {
            SHOW_COOL_DOWN = false; } 

        if ((ROOM_TEMP_F <= TARGET_TEMP) && (CURR_TEMP_F < TARGET_TEMP - TEMP_HYSTERESIS)) {                    
            digitalWrite(HEAT_UP, ON);
            SHOW_HEAT_UP = true;
        } else {
            digitalWrite(HEAT_UP, OFF);
            SHOW_HEAT_UP = false;
        }
    } else {
        SHOW_COOL_DOWN = false; 
        SHOW_HEAT_UP = false;
    }

    // Show on the oled display
    if(CONFIG_MODE) {
        encoder.tick();
        int newPos = encoder.getPosition(); // get the current physical position and calc the logical position
        
        if (newPos < ROTARYMIN) {
            encoder.setPosition(ROTARYMIN);
            newPos = ROTARYMIN;
        } else if (newPos > ROTARYMAX) {
            encoder.setPosition(ROTARYMAX);
            newPos = ROTARYMAX;
        }

        // Show Config mode on OLED
        if ((lastPos != newPos) || SHOW_FIRST) { 

            SHOW_FIRST = false;
            TARGET_TEMP = newPos;

            display.clearDisplay();
            display.setFont(&FreeMonoBold12pt7b);
            display.setCursor(10, 14);
            display.print("Target:");
            display.setFont(&FreeMonoBold18pt7b);
            display.setCursor(6, 50);
            display.print(TARGET_TEMP);
            display.print(" C  ");
            display.display();

            saveConfig();
            lastPos = newPos;
        }

    } else {

        if ((millis() - lastMillis) > 500) {
            lastMillis = millis();

            String info_text = "";
    
            if(!SWITCH){
                info_text = "<OFF>";
            }

            if(SHOW_COOL_DOWN) {
                info_text = "<COOL DOWN>";
            }

            if(SHOW_HEAT_UP){
                info_text = "<HEAT UP>";
            }

            // Show Working mode on OLED
            display.clearDisplay();
            display.setRotation(0);
            display.setFont(&FreeMonoBold18pt7b);
            display.setCursor(10, 22);
            display.print(CURR_TEMP_F, 1);
            display.print("C");
            display.setFont(&FreeMono9pt7b);

            int offset = ((127 - (info_text.length()*11))/2);
            display.setCursor(offset, 41);
            display.print(info_text);

            display.setCursor(8, 60);
            display.print("Target:");
            display.print(TARGET_TEMP);
            display.print("C");
            display.display();
            
        }
    }
}

// ======================================================================
// Functions
// ======================================================================

void keepWiFiAlive(void * pvparameters){
	debugf("-Task 'keepWifiAlive' running on CPU: %i\n", xPortGetCoreID());
  	for (;;) {
    
    	if(WiFi.status() == WL_CONNECTED) {
      		String RSSI_percent = String((100 + WiFi.RSSI())*2);
      		RSSI_STATUS = "RSSI:" + String(WiFi.RSSI()) + "dBm(" + RSSI_percent + "%)";             
      		vTaskDelay(10000/portTICK_PERIOD_MS);
      		continue;
    	}

    	// try to connect Wifi
    	WiFi.setHostname(HOSTNAME.c_str());
		WiFi.mode(WIFI_STA);
	    WiFi.begin(WIFI_SSID, WIFI_PW);
	
        debugf("-Connecting to WiFi '%s' ... ", WIFI_SSID); 
    	unsigned long startAttempTime = millis();

    	while(WiFi.status() != WL_CONNECTED && millis() - startAttempTime < WIFI_TIMEOUT_MS){};

    	if(WiFi.status() != WL_CONNECTED) {     
      		debugln("failed!");
      		RSSI_STATUS = "RSSI:" + String(WiFi.RSSI()) + "dBm(fail)"; 
      		vTaskDelay(20000/portTICK_PERIOD_MS);
      		continue;
    	} 
    	
		debugf("successful (Hostname:%s & IP:%s)\n", WiFi.getHostname(), WiFi.localIP().toString().c_str());   
  	} 
  	vTaskDelete(NULL);
}

void initDisplay() {      
    
    // Start I2C Communication SDA = 5 and SCL = 4 on Wemos Lolin32 ESP32 with built-in SSD1306 OLED
    Wire.begin(I2C_SDA, I2C_SCL);

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false)) {
        debugln("-!SSD1306 allocation failed");
        for(;;);                // Don't proceed, loop forever
    }
    delay(2000);                // Pause for 2 seconds
    display.clearDisplay();     // Clear the buffer.

    display.clearDisplay();
    display.setRotation(0);
    display.setTextColor(WHITE);
    display.setTextSize(1);
	display.setCursor(0,0);

	debugln_oled("Initializing...");
    debug_oled("-Version:");
    debugln_oled(VERSION);
	debug_oled_show();
}

String HTML_Parser(const String& var){

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

    if(var == "COMP_TEMP"){  return String(COMP_TEMP_F, 1);  }
    if(var == "TARGET_TEMP"){  return String(TARGET_TEMP);  }
    if(var == "TARGET_CO2"){  return String(TARGET_CO2, 1);  }
    if(var == "FRIDGE_TEMP"){  return String(CURR_TEMP_F, 1);  }
  
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
            cool_heat_status = "   <i class='fa-solid fa-temperature-arrow-up' style='font-size:24px;color:orange'></i>";
        } 
        if(SHOW_COOL_DOWN) {
            cool_heat_status = "   <i class='fa-solid fa-temperature-arrow-down' style='font-size:24px;color:blue'></i>";
        } 
        return cool_heat_status;
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

    // Make style.css available
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/style.css","text/css");
    });

    // Enable / Disable Fermentation Control
    server.on("/switch", HTTP_GET, [](AsyncWebServerRequest *request){       
        SWITCH = !SWITCH;
        request->redirect("/");
    });

    // Make set data available
    server.on("/setdata", HTTP_GET, [](AsyncWebServerRequest *request){       
        int paramsNr = request->params();
        for(int i=0;i<paramsNr;i++){
            AsyncWebParameter* p = request->getParam(i);
            
            if(p->name() == "TARGET_TEMP") {
                TARGET_TEMP = p->value().toInt();
            }
            if(p->name() == "TARGET_CO2") {
                TARGET_CO2 = p->value().toFloat();
            }
        }
        setPressure();
        debugf("/setdata requested -> TARGET_TEMP:%d + TARGET_CO2:%1.1f Pressure:%1.1f\n", TARGET_TEMP, TARGET_CO2, PRESSURE);
       
        request->redirect("/");
        //request->send(200, "text/plain", "Data received!");
    });

    // Make get log-data available
    server.on("/getlog", HTTP_GET, [](AsyncWebServerRequest *request){             
        debug("/getlog requested!");
        request->send(LittleFS, "/logfile.log","text");
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

    // Make favicon.ico available
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        debugln("Requested favicon.ico page");
        request->send(LittleFS, "/hop-g.ico","image");
    });

    // Starting WebServer
    esp_netif_init();
    server.begin();
    ElegantOTA.begin(&server);

    debugln("-WebServer started!");
	debug_oled("-WebServer started OK");
	debug_oled_show();
}

void loadConfig(){
 
    File config = LittleFS.open("/config.json","r");

    if(config) { 
        // Deserialize the JSON document
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, config);

        if (error) { 
            debugf("-! DeserializeJson failed! -> %s/n", error.f_str());
            return; 
        }
        JsonObject obj = doc.as<JsonObject>();
                
        String hostname = obj["HOSTNAME"];
        HOSTNAME = hostname.c_str();
        String mqtt_broker = doc["MQTT_BROKER"];     
        MQTT_BROKER = mqtt_broker.c_str();
        String ext_url = doc["EXTERNAL_URL"];     
        EXTERNAL_URL = ext_url.c_str();
        TARGET_TEMP = doc["TARGET_TEMP"];
        TARGET_CO2 = doc["TARGET_CO2"];
        TEMP_HYSTERESIS = doc["TEMP_HYSTERESIS"];
        COMP_RUNNING_TIME = doc["COMP_RUNNING_TIME"];
        COMP_TEMP_THRESHOLD = doc["COMP_TEMP_THRESHOLD"];
        POWER_HEAT = doc["POWER_HEAT"];
        POWER_COOL = doc["POWER_COOL"];
        POWER_IDLE = doc["POWER_IDLE"];
        
        debugln("-Config loaded");
        debugln_oled("-Config loaded     OK");
        debug_oled_show();
    }
    config.close();
}

void saveConfig(AsyncWebServerRequest *request) {

    File config = LittleFS.open("/config.json","w");

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

            if(p->name() == "EXTERNAL_URL") {
                EXTERNAL_URL = p->value();
                doc["EXTERNAL_URL"] = EXTERNAL_URL;
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

void saveConfig() {

    File config = LittleFS.open("/config.json","w");

    if(config) { 
        // Serialize the JSON document
        DynamicJsonDocument doc(1024);
        doc["HOSTNAME"] = HOSTNAME;
        doc["MQTT_BROKER"] = MQTT_BROKER;
        doc["EXTERNAL_URL"] = EXTERNAL_URL;
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

void initRelayBoard() {
    pinMode(COOL_DOWN, OUTPUT_OPEN_DRAIN);
    digitalWrite(COOL_DOWN, HIGH);
    pinMode(HEAT_UP, OUTPUT_OPEN_DRAIN);
    digitalWrite(HEAT_UP, HIGH);
    
    debugln("-Relais initialized!");
    debugln_oled("-Relaise init      OK");
    debug_oled_show();
}

void initRotaryEncoder() {
    // Setup ESP Rotary Encoder
    pinMode(ROTARY_BUTTON, INPUT_PULLUP);
    attachInterrupt(ROTARY_BUTTON, btnRotaryISR, FALLING);


    unsigned long button_time = 0;          //variables to keep track of the timing of recent interrupts
    unsigned long last_button_time = 0; 

    encoder.setPosition(TARGET_TEMP); // start with the value of TARGET_TEMP.

    debugln("-RotaryEncoder initialized!");
    debugln_oled("-Rotary init       OK");
    debug_oled_show();
}

void initMQTTClient() {
    mqttClient.setServer(MQTT_BROKER.c_str(), 1883);
    
    debugln("-MQTT Client initialized!");
    debugln_oled("-MQTT Client       OK");
    debug_oled_show();
}

void publishMessage(TimerHandle_t xTimer) {

    display.setCursor(119, 10);
    
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
                display.print("e");
                break;
            }      
        }
        debugln("OK!");
    }
    mqttClient.loop();

    if (mqttClient.connect(HOSTNAME.c_str())) {
        StaticJsonDocument<256> doc;                            // build JSON object
        doc["sender"] = HOSTNAME;
        doc["target_temp"] = TARGET_TEMP;
        doc["room_temp"] = ROOM_TEMP_F;
        doc["comp_temp"] = COMP_TEMP_F;
        doc["fridge_temp"] = CURR_TEMP_F;

        int COOL_HEAT = 10;
        if(SHOW_COOL_DOWN) { COOL_HEAT = 0; }
        if(SHOW_HEAT_UP) { COOL_HEAT = 20; }
        doc["cool_heat"] = COOL_HEAT;

        int msg_size = serializeJson(doc, mqtt_json_msg);

        mqttClient.publish("test", mqtt_json_msg, msg_size);
     
        display.print("*");
    } 
    display.display();
    debugln("Send MQTT Message");
}

void initDS1820(boolean manual){
    sensors.begin();

    if(manual) {
        if(sensors.isConnected(COMP_TEMP)) sensors.setResolution(COMP_TEMP, TEMPERATURE_PRECISION);
        if(sensors.isConnected(CURR_TEMP)) sensors.setResolution(CURR_TEMP, TEMPERATURE_PRECISION);
        if(sensors.isConnected(ROOM_TEMP)) sensors.setResolution(ROOM_TEMP, TEMPERATURE_PRECISION);

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
    }

    getTemp();
}

void getTemp(TimerHandle_t xTimer) {
    getTemp();
    debugln("Temp Timer");
}

void getTemp() {
    sensors.requestTemperatures();
    
    float temp_comp = sensors.getTempC(COMP_TEMP);
    if (COMP_TEMP_F == -111 || temp_comp > COMP_TEMP_F-3 && temp_comp < COMP_TEMP_F+3) {
        COMP_TEMP_F = temp_comp;
    } else {
        debugln("Failure on Sensor 'COMP_TEMP'!");
    }

    float temp_fridge = sensors.getTempC(CURR_TEMP);
    if (CURR_TEMP_F == -111 || temp_fridge > CURR_TEMP_F-3 && temp_fridge < CURR_TEMP_F+3) { 
        CURR_TEMP_F = temp_fridge;
    } else {
        debugln("Failure on Sensor 'CURR_TEMP'!");
    }

    float temp_room = sensors.getTempC(ROOM_TEMP);
    if (ROOM_TEMP_F == -111 || temp_room > ROOM_TEMP_F-3 && temp_room < ROOM_TEMP_F+3) { 
        ROOM_TEMP_F = temp_room;
    } else {
        debugln("Failure on Sensor 'ROOM_TEMP'!");
    }


    if (temp_fridge == DEVICE_DISCONNECTED_C) {
        initDS1820(true);
    } else {
        CURR_TEMP_F = temp_fridge;
    }
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++)
  {
    debugf("0x%02X, ", deviceAddress[i]);
  }
}

void setPressure(){
    
    float CO2 = TARGET_CO2 / 10;
    PRESSURE = CO2 / exp(-10.73797 + (2617.25 / (TARGET_TEMP + 273.15))) - 1.013;
    PRESSURE = round(PRESSURE*10)/10;
}

void switchCompressor(TimerHandle_t xTimer) {

    if(SHOW_COOL_DOWN) {
        digitalWrite(COOL_DOWN, ON);
    } else {
        digitalWrite(COOL_DOWN, OFF);
    }
    debugf("Switch Compressor: %d\n", SHOW_COOL_DOWN);
}
