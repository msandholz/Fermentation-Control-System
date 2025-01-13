#include <credentials.h>
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <time.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <UrlEncode.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_BME280.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <EPD_On.h>
#include <EPD_Off.h>
#include <EPD_Cool.h>
#include <EPD_Heat.h>

#define VERSION "2.1.0"

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

// Defining GPIO PINS
/* 
  PIN   |   Mode        |   Funtion             ||  PIN |   Mode        |   Function
  ------+---------------+-----------------------++------+---------------+-----------------------
  13    |   -           |   -                   ||  G   |   GND         |   Ground
  15    |   output      |   Heat up             ||  12  |   -           |      
  2     |   output      |   Cool down           ||  14  |   Input       |   Button Temp plus
  0     |   I2C SDA     |   BME 280 Data        ||  27  |   Input       |   Button Temp minus 
  4     |   SPI BUSY    |   EPD_BUSY            ||  26  |   I2C CLK     |   BME 280 Clock
  16    |   SPI RST     |   EPD RST             ||  25  |   I2C SDA     |   BME 280 Data 
  17    |   SPI DC      |   EPD DC              ||  33  |   -           |   
  5     |   SPI CS      |   EPD CS              ||  32  |   One Wire    |   DS1820 Temp sendor
  18    |   SPI CLK     |   EPD CLK             ||  35  |   -           |   
  23    |   SPI DIN     |   EPD DIN (MOSI)      ||  34  |   Input       |   Bubble Counter
  19    |   -           |                       ||  EN  |   -           |
  22    |   output      |   LED on board        ||  VN  |   -           |
  3V    |   VCC         |   Power               ||  VP  |   -           |
*/

#define COOL_DOWN 2                                     // GPIO for cooling down Relais
#define HEAT_UP 15                                      // GPIO for heating up Relais

#define BUBBLE_COUNT 34                                 // GPIO for Bubble Counter
#define BTN_TEMP_MINUS 27                               // GPIO for TEMP -1
#define BTN_TEMP_PLUS 14                                // GPIO for TEMP +1
#define BTN_PRESS_TIME 250                              // Milliseconds Button pressed
#define BTN_BOTH_PRESS_TIME 5000                        // Milliseconds both Buttons pressed

#define SDA_PIN 25                                      // GPIO for I2C SDA
#define SCL_PIN 26                                      // GPIO for I2C SCL

#define TEMP 32                                         // GPIO for OneWire-Bus
#define TEMPERATURE_PRECISION 10                        // Set Temp 12=0,0625°C(750ms) / 11=0,125°C(375ms) / 10=0,25(187,5ms) / 9=0.5°C(93,75ms)

#define ENABLE_GxEPD2_GFX 0                             // 2.9 inch e-Paper Module
#define SCREEN_WIDTH   296                              // 296
#define SCREEN_HEIGHT  128                              // 128
#define EPD_BUSY  4                                     // EPD BUSY
#define EPD_RST   16                                    // EPD RST
#define EPD_DC    17                                    // EPD DC
#define EPD_CS    5                                     // EPD CS
#define EPD_SCK   18                                    // EPD CLK
#define EPD_MOSI  23                                    // EPD DIN (MOSI)
//define EPD_MISO  19                                   // EPD MISO (Master-In Slave-Out not used, as no data from display)
#define EPD_IDLE_TIME 780                               // Milliseconds the EPD need to refresh

#define ON false
#define OFF true 

// ======================================================================
// Setting parameters with default values
// ======================================================================

// ESP32 Parameters
char CHIP_MODEL[50];                                    // Chip model
String SDK_VERSION = "";                                // SDK Version
int FLASH_USED = 0;                                     // Flash size used
int FLASH_TOTAL = 0;                                    // Flash size total
int FILESYSTEM_USED = 0;                                // Filesystem size used
int FILESYSTEM_TOTAL = 0;                               // Filesystem size total


// Section for Wifi parameters
int WIFI_TIMEOUT_MS = 20000;                            // Wifi Timeout in msec
String RSSI_STATUS = "";                                // Wifi RSSI Status
String HOSTNAME = "ESP-Fermentation";                   // Enter Hostname here
TaskHandle_t keepWiFiAlive_handle;                      // Handle for keeping WiFi connection alive
String EXTERNAL_URL = "http://192.168.178.12:3000/d/cLwyHTWgz/fermentation-control-system?orgId=1"; // URL of external Website


// Section for temperature
float FRIDGE_TEMP_F = -111;
float BEER_TEMP_F = -111;
float COMP_TEMP_F = -111;
float ROOM_TEMP_F = -111;
float ROOM_HUMIDITY_F = -111;
float ROOM_PRESSURE_F = -111;
int TARGET_TEMP = 10;                                   // Setting default target temp
int TEMP_HYSTERESIS = 1;                                // Parameter for temp hystersis
int CHART_VALUES [201];                                 // Array for chart
boolean SHOW_COOL_DOWN = false;                         // Flag for cooling
boolean SHOW_HEAT_UP = false;                           // Flag for heating
static TimerHandle_t Temp_timer = NULL;                 // Handle to get temperatures
static TimerHandle_t Display_timer = NULL;              // Timer for refreshing the EPD display
boolean SWITCH = true;           	                    // Enable Fermentation Control
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;       // Mutex for disabling interface handling
Adafruit_BME280 bme280;                                 // I2C

// Section for OneWire sensors 
DeviceAddress COMP_TEMP = { 0x28, 0xB4, 0x92, 0x45, 0xD4, 0xE7, 0x5E, 0xC8 };
DeviceAddress CURR_TEMP = { 0x28, 0x66, 0x02, 0x45, 0xD4, 0xF2, 0x15, 0xA0 };
DeviceAddress BEER_TEMP = { 0x28, 0x9F, 0xB4, 0x45, 0xD4, 0x69, 0x58, 0xCC };


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
char mqtt_json_msg[256];                                // MQTT Message
static TimerHandle_t MQTT_timer = NULL;                 // Handle to send MQTT Messages


// Section for Button and Bubble Counter 
portMUX_TYPE synch = portMUX_INITIALIZER_UNLOCKED;
boolean BUTTON_PRESSED = false;                         // Flag, if one button pressed
volatile long lastBtnPressPlus = 0;                     // Starting time for pressing button plus  
volatile long lastBtnPressMinus = 0;                    // Starting time for pressing button minus  
boolean BUTTONS_BOTH_PRESSED = false;                   // Flag, if both buttons pressed simultaneously
volatile long lastBtnPressedBoth = 0;                   // Starting time for pressing both buttons simultaneously                    
int bubbleCount = 0;                                    // Bubble Counter
int BUBBLES_PER_MINUTE = 0;                             // Bubbles per Minute
static TimerHandle_t BubbleCount_timer = NULL;          // Timer for Bubble Counter

// Section for ePaper Timer
volatile long last_EPD_refresh_time = 0;                 // Timestamp of last display refresh

// Section für WhatApp notification
boolean WHATSAPP_NOTIFICATION = false;


// Section to log last reboot
const char* NTP_SERVER = "de.pool.ntp.org";
const char* TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";
tm timeinfo;
char LAST_REBOOT[20];                                   // Last Reboot


// ======================================================================
// Interrupt Routines
// ======================================================================

// Interrupt service routine for Bubble Counter
void IRAM_ATTR bubbleCountISR() {
    portENTER_CRITICAL_ISR(&synch);
    bubbleCount++;
    portEXIT_CRITICAL_ISR(&synch);
}

// Interrupt service routine for button "Temp minus"
void IRAM_ATTR btnTempMinusISR() {
    portENTER_CRITICAL_ISR(&synch);
    int currentState = gpio_get_level((gpio_num_t)BTN_TEMP_MINUS);

    if(currentState == 1) {
        lastBtnPressMinus = millis();
    } else {
        if (((millis() - lastBtnPressMinus) > BTN_PRESS_TIME) && !BUTTONS_BOTH_PRESSED) {
            TARGET_TEMP--;
            BUTTON_PRESSED = true;
            if(TARGET_TEMP < 0) { TARGET_TEMP = 0; }
        }
    }
    portEXIT_CRITICAL_ISR(&synch);
}

// Interrupt service routine for button "Temp plus"
void IRAM_ATTR btnTempPlusISR() {
    portENTER_CRITICAL_ISR(&synch);

    int currentState = gpio_get_level((gpio_num_t)BTN_TEMP_PLUS);

    if(currentState == 1) {
        lastBtnPressPlus = millis();
    } else {
        if (((millis() - lastBtnPressPlus) > BTN_PRESS_TIME) && !BUTTONS_BOTH_PRESSED) {
            TARGET_TEMP++;
            BUTTON_PRESSED = true;
            if(TARGET_TEMP > 25) { TARGET_TEMP = 25; }
        }
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

// Connections for e.g. LOLIN D32
enum alignmentType {LEFT, RIGHT, CENTER};
GxEPD2_BW<GxEPD2_290_T94_V2, GxEPD2_290_T94_V2::HEIGHT> ePaper(GxEPD2_290_T94_V2(/*CS=D8*/ EPD_CS, /*DC=D3*/ EPD_DC, /*RST=D4*/ EPD_RST, /*BUSY=D2*/ EPD_BUSY));
//GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=D8*/ EPD_CS, /*DC=D3*/ EPD_DC, /*RST=D4*/ EPD_RST, /*BUSY=D2*/ EPD_BUSY));

SPIClass hspi(HSPI);


// ======================================================================
// Functions
// ======================================================================

void loadConfig();
void saveConfig(AsyncWebServerRequest *request);
void saveConfig();

void initESP(boolean filesystem);
void initOTA();


void printAddress(DeviceAddress deviceAddress);
void initDS1820(boolean manual);
void initBME280();
void initRelayBoard();
void getTemp();
void getTemp(TimerHandle_t xTimer);

void switchCompressor(TimerHandle_t xTimer);
void calcBubbles(TimerHandle_t xTimer);

void setPressure();

void keepWiFiAlive();

void publishMessage(TimerHandle_t xTimer);
void initMQTTClient();

void sendWhatsAppNotification(String Nofication);

void startWebServer();
void requestData(AsyncWebServerRequest *request);
String responseData();
void requestConfig(AsyncWebServerRequest *request);
String responseConfig();
String HTML_Parser(const String& var);

// Display related
void initEPD();
void refresh_EPD(TimerHandle_t xTimer);
void refresh_EPD();
void show_EPD_UpperPart();
void show_EPD_Graph();
void show_EPD_Status();
void convertEndianness(String name, unsigned char EPD_Cool_bits[], int height, int width);

static TimerHandle_t refresh_display_timer = NULL;


// ======================================================================
// Setup
// ======================================================================

void setup() {
    debug_speed(115200);
    debugln("===SETUP_BEGIN===");

    initESP(true);                                      // Initialize ESP
    loadConfig();                                       // Load Config
    keepWiFiAlive();                                    // Connect to Wifi 

    //convertEndianness("EPD", EPD_Off_bits, EPD_Cool_height, EPD_Off_width);
    
    // Initialize Over The Air Updates
    initOTA();

    //initDS1820(false);                                // Initialize Tempsensors - get sensor adresses
    initDS1820(true);                                   // Initialize Tempsensors - set precision
    initBME280();                                       // Initialize BME280 Sensor
    initRelayBoard();                                   // Initialize Relay board
    initMQTTClient();                                   // Initialize MQTT Client

    // GPIO for TEMP -1
    pinMode(BTN_TEMP_MINUS, INPUT_PULLUP);         
    attachInterrupt(BTN_TEMP_MINUS, btnTempMinusISR, CHANGE);
    
    // GPIO for TEMP +1
    pinMode(BTN_TEMP_PLUS, INPUT_PULLUP);
    attachInterrupt(BTN_TEMP_PLUS, btnTempPlusISR, CHANGE);         

    // GPIO for Bubble Counter
    pinMode(BUBBLE_COUNT, INPUT_PULLUP);         
    attachInterrupt(BUBBLE_COUNT, bubbleCountISR, FALLING);

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
        "Timer for sending MQTT messages",              // Name of timer
        45000/portTICK_PERIOD_MS,                       // Period of timer (in ticks)
        pdTRUE,                                         // Auto-reload
        (void *)3,                                      // Timer ID
        publishMessage);                                // Callback function

    // Start Bubble Count timer
    BubbleCount_timer = xTimerCreate(
        "Timer for bubbles per minute",                 // Name of timer
        60000/portTICK_PERIOD_MS,                       // Period of timer (in ticks)
        pdTRUE,                                         // Auto-reload
        (void *)4,                                      // Timer ID
        calcBubbles);                                   // Callback function

    // Start update display timer
    Display_timer = xTimerCreate(
        "Refresh Temp on Display timer",                // Name of timer
        10000 / portTICK_PERIOD_MS,                     // Period of timer (in ticks)
        pdTRUE,                                         // Auto-reload
        (void *)5,                                      // Timer ID
        refresh_EPD);                                   // Callback function


    if( ( Temp_timer != NULL ) && ( MQTT_timer != NULL ) && ( Compressor_timer != NULL) && ( BubbleCount_timer != NULL) ) { 
        // Start the software timers, using a block time of 0 (no block time). 
        xTimerStart(Temp_timer, 500);
        xTimerStart(MQTT_timer, portMAX_DELAY);
        xTimerStart(Compressor_timer, 10000);
        xTimerStart(BubbleCount_timer, 20000);
        xTimerStart(Display_timer, 6000);
    }

    // Initialize CHART_VALUES array
    for (int i = 0; i < 200; i++){ CHART_VALUES[i] = 0; }

    startWebServer();                                   // Start WebServer
    initEPD();                                          // Initialize ePaper Display

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

    ArduinoOTA.handle(); 
    
    // check, if both buttons presses simultaneously
    if(digitalRead(BTN_TEMP_MINUS) == HIGH && digitalRead(BTN_TEMP_PLUS) == HIGH) {
        if(!BUTTONS_BOTH_PRESSED) {
            BUTTONS_BOTH_PRESSED = true;
            lastBtnPressedBoth = millis();
        }

        if(millis() - lastBtnPressedBoth > BTN_BOTH_PRESS_TIME) {
            SWITCH=!SWITCH;
            refresh_EPD();
            lastBtnPressedBoth = millis();
        }        
    } else {
        BUTTONS_BOTH_PRESSED = false;        
    }

    // check, if on button was pressed and refresh display
    if(BUTTON_PRESSED) {
        refresh_EPD();
        BUTTON_PRESSED = false;
    }

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

// initialize ESP + gather system parameters
void initESP(boolean filesystem) {

    debugln("+--------------------------------------------------------------------------");
    sprintf(CHIP_MODEL, "%s (Rev.%i) with %i Core(s) and %i MHz", ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(), ESP.getCpuFreqMHz());   
    debugf("| ChipModel:   %s\n", CHIP_MODEL);

    SDK_VERSION = ESP.getSdkVersion();
    debugf("| SDK Version: %s\n", SDK_VERSION);

    // calculate RAM
    int heap_used = ESP.getHeapSize() - ESP.getFreeHeap();
    int heap_total = ESP.getHeapSize();
    float heap_percentage = (float)heap_used / heap_total  * 100;
    debugf("| RAM:         %02.1f%% (used %i bytes from %i bytes)\n", heap_percentage, heap_used, heap_total); 

    // calculate FLASH
    FLASH_USED = ESP.getSketchSize();
    FLASH_TOTAL = ESP.getFlashChipSize();
    float flash_percentage = (float)FLASH_USED / FLASH_TOTAL * 100;
    debugf("| FLASH:       %02.1f%% (used %i bytes from %i bytes)\n", flash_percentage, FLASH_USED, FLASH_TOTAL);          

    if(filesystem) { // Mounting LittleFS
        if (!LittleFS.begin(true)) { 
            debugln("| LittlsFS:   An error occurred during LittleFS mounting!");
            return; 
        } else {
            FILESYSTEM_USED = LittleFS.usedBytes();
            FILESYSTEM_TOTAL = LittleFS.totalBytes();
            float filesystem_percentage = (float)FILESYSTEM_USED / FILESYSTEM_TOTAL * 100;
            debugf("| LittlsFS:    %02.1f%% (used %i bytes from %i bytes)\n", filesystem_percentage, FILESYSTEM_USED, FILESYSTEM_TOTAL);
        } 
    } 
    debugln("+--------------------------------------------------------------------------");
}

// function for loading configuration from file system
void loadConfig(){
 
  File configFile = LittleFS.open("/config.json","r");

  if(configFile) { 
    // Deserialize the JSON document
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, configFile);

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
  configFile.close();
}

// function to persist save config parameters on file system
void saveConfig() {

    File configFile = LittleFS.open("/config.json","w");
    if(configFile) { 
        // Serialize the JSON document
        JsonDocument doc;
        String configString;
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
        serializeJsonPretty(doc, configString);
        configFile.print(configString);
    }   
    configFile.close();
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++)
  {
    debugf("0x%02X, ", deviceAddress[i]);
  }
}

// function to initialize OTA
void initOTA(){

    //ArduinoOTA.setPort(3232);                           // Port defaults to 3232
    ArduinoOTA.setHostname(HOSTNAME.c_str());           // Hostname defaults to esp3232-[MAC]
    //ArduinoOTA.setPassword("Msha1309g*");               // No authentication by default

    ArduinoOTA
        .onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
                type = "sketch";
            else // U_SPIFFS
                type = "filesystem";

            // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
            debugln("Start OTA updating " + type);
        })
        
        .onEnd([]() {
            debugln("\nEnd");
        })

        .onProgress([](unsigned int progress, unsigned int total) {
            debugf("Progress: %u%%\r", (progress / (total / 100)));
        })
        
        .onError([](ota_error_t error) {
            debugf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) debugln("Auth Failed");
            else if (error == OTA_BEGIN_ERROR) debugln("Begin Failed");
            else if (error == OTA_CONNECT_ERROR) debugln("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) debugln("Receive Failed");
            else if (error == OTA_END_ERROR) debugln("End Failed");
        });

    ArduinoOTA.begin();
}

// function to initialize DS1820 sensors
void initDS1820(boolean manual){
    sensors.begin();

    if(manual) {
        if(sensors.isConnected(COMP_TEMP)) sensors.setResolution(COMP_TEMP, TEMPERATURE_PRECISION);
        if(sensors.isConnected(CURR_TEMP)) sensors.setResolution(CURR_TEMP, TEMPERATURE_PRECISION);
       // if(sensors.isConnected(ROOM_TEMP)) sensors.setResolution(ROOM_TEMP, TEMPERATURE_PRECISION);
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
/*
        if (sensors.getAddress(ROOM_TEMP, 2)) {
            debug("Device 2  has address: ");
            printAddress(ROOM_TEMP);
            debugln();
            sensors.setResolution(ROOM_TEMP, TEMPERATURE_PRECISION);
        } else {
            debugln("Unable to find address for Device 2");
        }
*/

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

// function to initialize BME 280 sensor
void initBME280(){
    debug("-BME 280           ->");
    Wire.begin(SDA_PIN, SCL_PIN);
    unsigned bme_status = bme280.begin(0x76, &Wire);  

    if (!bme_status) {
        debugf("Could not find a valid BME280 sensor [Sensor ID: 0x%i]", bme280.sensorID());
    } else {
        debugln(" initialized!");
    }
}

// function to initialize Relay board
void initRelayBoard() {
    pinMode(COOL_DOWN, OUTPUT_OPEN_DRAIN);
    digitalWrite(COOL_DOWN, HIGH);
    pinMode(HEAT_UP, OUTPUT_OPEN_DRAIN);
    digitalWrite(HEAT_UP, HIGH);
    
    debugln("-Relay board       -> initialized!");
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

// function to get BME280 data
void getBME280Data(){
    ROOM_TEMP_F = bme280.readTemperature();
    ROOM_HUMIDITY_F = bme280.readHumidity();
    ROOM_PRESSURE_F = (bme280.readPressure() / 100.0F);
}

// function to get temperature with timer handle
void getTemp(TimerHandle_t xTimer) {
  getTemp();
  getBME280Data();
  debugf("TIMER for Temp-> Target: %d°C|", TARGET_TEMP);
  debugf("Fridge: %.1f°C|", FRIDGE_TEMP_F);
  debugf("Beer: %.1f°C|",BEER_TEMP_F);
  debugf("Comp: %.1f°C|", COMP_TEMP_F);
  debugf("Room: %.1f°C|", ROOM_TEMP_F);
  debugf("Humidity: %.1f %|", ROOM_HUMIDITY_F);
  debugf("Pressure: %.1f hPa\n", ROOM_PRESSURE_F);
}

// function to calulate bubbles per minute
void calcBubbles(TimerHandle_t xTimer){  
    BUBBLES_PER_MINUTE = bubbleCount;
    bubbleCount = 0;

    if (CHART_VALUES[199] > 0) {
        for (int i = 0; i < 200; i++) {
            CHART_VALUES[i] = CHART_VALUES[i + 1];
        }
        CHART_VALUES[200] = int(BEER_TEMP_F);
    } else {
        for (int i = 0; i < 200; i++) {
            if(CHART_VALUES[i] == 0) {
                CHART_VALUES[i] = int(BEER_TEMP_F);
                debugf("---> calcBubbles LOOP -> Array[%i] | VALUE[%i]\n",i, CHART_VALUES[i]);
                break;
            }
        }
    }

    refresh_EPD();
    debugf("TIMER for Bubbles-> %i bubbles per minute\n", BUBBLES_PER_MINUTE);
}

// function to switch compressor on/off with timer handle
void switchCompressor(TimerHandle_t xTimer) {

    //taskENTER_CRITICAL(&lock); 
    taskDISABLE_INTERRUPTS(); // disable all interrupts
    if(SHOW_COOL_DOWN) {
        digitalWrite(COOL_DOWN, ON);
    } else {
        digitalWrite(COOL_DOWN, OFF);
    }
    delay(250);
    taskENABLE_INTERRUPTS(); // enable all interrupts
    //taskEXIT_CRITICAL(&lock); 

    refresh_EPD();

    debugf("SWITCH Compressor: %d\n", SHOW_COOL_DOWN);
}

// function to keep wifi alive 
void keepWiFiAlive(){
	 
    wl_status_t status = WiFi.status();
    String wifi_stat = "---";
    
    switch (status) {
        case WL_IDLE_STATUS:
            wifi_stat = "IDLE";
            break;
        case WL_NO_SSID_AVAIL:
            wifi_stat = "NO_SSID_AVAILABLE";
            break;
        case WL_CONNECTED:
            wifi_stat = "CONNECTED";
            break;
        case WL_CONNECT_FAILED:
            wifi_stat = "CONNECT_FAILD";
            break;
        case WL_CONNECTION_LOST:
            wifi_stat = "CONNECTION_LOST";
            break;
        case WL_DISCONNECTED:
            wifi_stat = "DISCONNECTED";
            break;
        default:
            wifi_stat = "UNKOWN";
    }


    debugf("WIFI [%s]\n", wifi_stat);
     
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
            debugf("-Access at http://%s.local\n", String(HOSTNAME));
        }
    } 

    String RSSI_percent = String((100 + WiFi.RSSI())*2);
    RSSI_STATUS = "RSSI:" + String(WiFi.RSSI()) + "dBm(" + RSSI_percent + "%)";
}

// function to publish MQTT message 
void publishMessage(TimerHandle_t xTimer) {

    keepWiFiAlive();
    
    if(!mqttClient.connected()) {
        int b = 0;
        debug("-Connecting to MQTT Broker");
  
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
        doc["room_humidity"] = ROOM_HUMIDITY_F;
        doc["room_pressure"] = ROOM_PRESSURE_F;
        doc["comp_temp"] = COMP_TEMP_F;
        doc["fridge_temp"] = FRIDGE_TEMP_F;
        doc["beer_temp"] = BEER_TEMP_F;
        doc["bubble_per_minute"] = BUBBLES_PER_MINUTE;

        int COOL_HEAT = 10;
        if(SHOW_COOL_DOWN) { COOL_HEAT = 0; }
        if(SHOW_HEAT_UP) { COOL_HEAT = 20; }
        doc["cool_heat"] = COOL_HEAT;

        int msg_size = serializeJson(doc, mqtt_json_msg);

        mqttClient.publish("test", mqtt_json_msg, msg_size);

        debugf("TIMER for MQTT-> Message sent:%s\n", mqtt_json_msg);

    } else {
        debugln("TIMER for MQTT-> Sending Message failed!");
    }
   
}

// function to initialize the MQTT client
void initMQTTClient() {
    mqttClient.setServer(MQTT_BROKER.c_str(), 1883);
    
    debugln("-MQTT Client       -> initialized!");
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
        String url = "https://api.callmebot.com/whatsapp.php?phone=" + String(PHONE_NUMBER) + "&apikey=" + String(API_KEY) + "&text="+urlEncode(message);    
        HTTPClient http;
        http.begin(url);

        // Specify content-type header
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
        // Send HTTP POST request
        int httpResponseCode = http.POST(url);
        if (httpResponseCode == 200){
            debugln("WHATSAPP Notification -> sent successfully!");
        } else {
            debug("WHATSAPP Notification -> ERROR: HTTP response code: ");
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

    // Make style.css available
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/style.css","text/css");
    });

    // Make favicon.ico available
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        debugln("Requested favicon.ico page");
        request->send(LittleFS, "/hop-g.ico","image");
    });

    // Make index.html available
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        debugln("Requested index.html page");
        request->send(LittleFS, "/index.html", String(), false, HTML_Parser);
    });

    // Make calc.html available
    server.on("/calc", HTTP_GET, [](AsyncWebServerRequest *request){
        debugln("Requested calc.html page");
        request->send(LittleFS, "/calc.html", String(), false, HTML_Parser);
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

    // Make get data available
    server.on("/getdata", HTTP_GET, [](AsyncWebServerRequest *request){    
        
        requestData(request);         
        String output = responseData();
        //debugln("/data response -> "+ output);
        request->send(200, "application/json", output);
    });

    // Make get config available
    server.on("/getconfig", HTTP_GET, [](AsyncWebServerRequest *request){

        requestConfig(request);
        String output = responseConfig();
        //debugln("/data response -> "+ output);
        request->send(200, "application/json", output);
    });

    // Reboot ESP
    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){       
        debugln("!!!Rebooting ESP!!!");
        request->send(200, "text/plain", "Rebooting ESP!");
        ESP.restart();
    });

    //handling uploading firmware file
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {  
        if(Update.hasError()) {
            request->send(500, "text/plain", "Update Failed!");
            debugf("-Over-The-Air Update Failed!");
        } else {
            request->send(200, "text/plain", "Update Successfull! Rebooting...");
            debugf("-Over-The-Air Update Successfull! Rebooting...");
            ESP.restart();
        }
    },
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
        
        
        if (!index) {
            debugf("-Over-The-Air Update started: %s ", filename.c_str());
            
            if ((filename.c_str()=="firmware.bin") && (!Update.begin(UPDATE_SIZE_UNKNOWN))) { // OTA for firmware.bin
                debugln("(firmware mode)");
                Update.printError(Serial);
            }

            if ((filename.c_str()=="littlefs.bin") && (!Update.begin(0x400000, U_SPIFFS))) { // OTA for littlefs.bin started
                debugln("(littlefs mode)");
                Update.printError(Serial);
            }
        }
        if (!Update.hasError()) {
            if (Update.write(data, len) != len) { // Writing data
                Update.printError(Serial);
            }
        }
        if (final) {
            if (Update.end(true)) { // OTA ended
                debugf("-Update successfull: %u Bytes\n", index + len);
            } else {
                Update.printError(Serial);
            }
        }
    });

    // Starting WebServer
    esp_netif_init();
    server.begin();
    //ElegantOTA.begin(&server);

    debugln("-WebServer         -> started!");
}

// function for processing requested data
void requestData(AsyncWebServerRequest *request) {
    debug("REQUEST /getdata->" ); 
    int paramsNr = request->params();

    for(int i=0;i<paramsNr;i++){
        const AsyncWebParameter* p = request->getParam(i);
  
        if(p->name() == "TARGET_TEMP") {
            TARGET_TEMP = p->value().toInt();
            //refreshEPD_TargetTemp();
            debugf(" TARGET_TEMP:%d", TARGET_TEMP);
        }
        if(p->name() == "TARGET_CO2") {
            TARGET_CO2 = p->value().toFloat();
            debugf(" + TARGET_CO2:%1.1f", TARGET_CO2);
        }
        if(p->name() == "SWITCH") {
            if (p->value() == "true"){
                SWITCH = true;
            } else {
                SWITCH = false;
            }
            debugf(" + Switch:%i", SWITCH);
        }
    }

    if(paramsNr > 0){
        refresh_EPD();
        saveConfig();
    }

    debugln("");
}

// function to resonpse JSON data
String responseData() {

    // Allocate a temporary JsonDocument
    JsonDocument data;

    data["HOSTNAME"] = HOSTNAME;
    data["VERSION"] = VERSION;
    data["EXTERNAL_URL"] = EXTERNAL_URL;
    data["SWITCH"] = SWITCH;
    data["TARGET_TEMP"] = TARGET_TEMP;
    data["TARGET_CO2"] = TARGET_CO2;
    data["POWER_CONSUMPTION"] = POWER_CONSUMPTION;
    data["ROOM_TEMP"] = String(ROOM_TEMP_F, 1);
    data["ROOM_HUMIDITY"] = String(ROOM_HUMIDITY_F, 1);
    data["ROOM_PRESSURE"] = String(ROOM_PRESSURE_F, 1);
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
    data["BUBBLES_PER_MINUTE"] = BUBBLES_PER_MINUTE;
    data["LAST_REBOOT"] = LAST_REBOOT;
    data["RSSI_DBM"] = WiFi.RSSI();
    data["CHIP_MODEL"] = CHIP_MODEL;
    data["SDK_VERSION"] = SDK_VERSION;
    data["FLASH_USED"] = FLASH_USED;
    data["FLASH_TOTAL"] = FLASH_TOTAL;  
    data["RAM_USED"] = ESP.getHeapSize() - ESP.getFreeHeap(); 
    data["RAM_TOTAL"] = ESP.getHeapSize();
    data["FILESYSTEM_USED"] = FILESYSTEM_USED;
    data["FILESYSTEM_TOTAL"] = FILESYSTEM_TOTAL;  
    data["IP_ADDRESS"] = WiFi.localIP();

    // Serialize JSON document to String
    String output;
    serializeJsonPretty(data, output);
    saveConfig();
    return output;
}

// function for processing requested data
void requestConfig(AsyncWebServerRequest *request){
    debug("REQUEST /getconfig->" ); 
    int paramsNr = request->params();
        
    for(int i=0;i<paramsNr;i++){
        const AsyncWebParameter* p = request->getParam(i);
  
        if(p->name() == "HOSTNAME") {
            HOSTNAME = p->value();
            debugf(" HOSTNAME:%s", HOSTNAME);
        }

        if(p->name() == "MQTT_BROKER") {
            MQTT_BROKER = p->value();
            debugf(" + MQTT_BROKER:%s", MQTT_BROKER);
        }

        if(p->name() == "EXTERNAL_URL") {
            EXTERNAL_URL = p->value();
            debugf(" + EXTERNAL_URL:%s", EXTERNAL_URL);
        }

        if(p->name() == "WHATSAPP_NOTIFICATION") {
            WHATSAPP_NOTIFICATION = p->value();
            debugf(" + WHATSAPP_NOTIFICATION:%i\n", p->value());
        }

        if(p->name() == "TEMP_HYSTERESIS") {
            TEMP_HYSTERESIS = p->value().toInt();
            debugf(" + TEMP_HYSTERESIS:%d", TEMP_HYSTERESIS);
        }

        if(p->name() == "COMP_RUNNING_TIME") { 
            COMP_RUNNING_TIME = p->value().toInt(); 
            debugf(" + COMP_RUNNING_TIME:%i", COMP_RUNNING_TIME);
        }

        if(p->name() == "COMP_TEMP_THRESHOLD") { 
            COMP_TEMP_THRESHOLD = p->value().toInt(); 
            debugf(" + COMP_TEMP_THRESHOLD:%i", COMP_TEMP_THRESHOLD); 
        }

        if(p->name() == "TARGET_TEMP") {
            TARGET_TEMP = p->value().toInt();
            debugf(" + TARGET_TEMP:%i", TARGET_TEMP); 
        }

        if(p->name() == "TARGET_CO2") {
            TARGET_CO2 = p->value().toFloat();
            debugf(" + TARGET_CO2:%1.1f", TARGET_CO2);
        }

        if(p->name() == "POWER_HEAT") {
            POWER_HEAT = p->value().toInt();
            debugf(" + POWER_HEAT:%i", POWER_HEAT);
        }

        if(p->name() == "POWER_COOL") {
            POWER_COOL = p->value().toInt();
            debugf(" + POWER_COOL:%i", POWER_COOL);
        }

        if(p->name() == "POWER_IDLE") {
            POWER_IDLE = p->value().toInt();
            debugf(" + POWER_IDLE:%i", POWER_IDLE);                
        }
    }
    debugln("");
    saveConfig();
}

// function to resonpse JSON config 
String responseConfig() {
    // Allocate a temporary JsonDocument
    JsonDocument config;

    config["HOSTNAME"] = HOSTNAME;
    config["VERSION"] = VERSION;
    config["WIFI_SSID"] = WIFI_SSID;
    config["WIFI_PW"] = WIFI_PW;
    config["MQTT_BROKER"] = MQTT_BROKER;
    config["EXTERNAL_URL"] = EXTERNAL_URL;
    config["WHATSAPP_NOTIFICATION"] = WHATSAPP_NOTIFICATION;
    config["TEMP_HYSTERESIS"] = TEMP_HYSTERESIS;
    config["TARGET_TEMP"] = TARGET_TEMP;
    config["TARGET_CO2"] = TARGET_CO2;
    config["COMP_RUNNING_TIME"] = COMP_RUNNING_TIME;
    config["COMP_TEMP_THRESHOLD"] = COMP_TEMP_THRESHOLD;

    config["POWER_HEAT"] = POWER_HEAT;
    config["POWER_COOL"] = POWER_COOL;
    config["POWER_IDLE"] = POWER_IDLE;

    // Serialize JSON document to String
    String output;
    serializeJsonPretty(config, output);
    saveConfig();
    return output;
}

// function to calculate the pressure
void setPressure(){    
    float CO2 = TARGET_CO2 / 10;
    PRESSURE = CO2 / exp(-10.73797 + (2617.25 / (TARGET_TEMP + 273.15))) - 1.013;
    PRESSURE = round(PRESSURE*10)/10;
}

// function to initialize the display
void initEPD() {
    debugln("-Display           -> initialized!");
    if (DEBUG_SERIAL) { 
        ePaper.init(115200, true, 2, false); 
    } else {
        ePaper.init();
    } 
   
    char value_text[] = "-";
    int16_t tbx, tby; uint16_t tbw, tbh;
    ePaper.setRotation(1);                    // Use 1 or 3 for landscape modes
    ePaper.setPartialWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);   
    ePaper.fillScreen(GxEPD_WHITE);
    ePaper.setTextColor(GxEPD_BLACK);
    ePaper.firstPage();

    do {
        show_EPD_UpperPart();
        show_EPD_Graph();
        show_EPD_Status();
    } while (ePaper.nextPage());
    
    //ePaper.display(false);
}

// function for EPD to Refresh upper Part with timer
void refresh_EPD(TimerHandle_t xTime){
    refresh_EPD();
}

// function for EPD to Refresh upper Part 
void refresh_EPD(){

    debugf("REFRESH EPD: millis[%i] timediff[%i]\n", millis(), millis() - last_EPD_refresh_time);

    if ((millis() - last_EPD_refresh_time) > EPD_IDLE_TIME) {
        last_EPD_refresh_time = millis();

        ePaper.setPartialWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);   
        ePaper.fillScreen(GxEPD_WHITE);

        show_EPD_UpperPart();
        show_EPD_Graph();
        show_EPD_Status();

        ePaper.display(true); // Inhalt anzeigen und Puffer leeren
    }
}

// function for EPD to show upper part
void show_EPD_UpperPart(){

    int16_t tbx, tby; uint16_t tbw, tbh;

    // Area for BEER TEMP
    char beer_temp[] = "-";
    ePaper.setFont(&FreeSans9pt7b);
    ePaper.setCursor(0, 20);
    ePaper.print("Beer:");
    //ePaper.drawRect(52, 0, 70, 35, GxEPD_BLACK);
    
    ePaper.setFont(&FreeSansBold18pt7b);
    if(BEER_TEMP_F != -111){
        dtostrf(BEER_TEMP_F, 1, 0, beer_temp);
    }
    ePaper.getTextBounds(beer_temp, 0, 0, &tbx, &tby, &tbw, &tbh);
    ePaper.setCursor(119-tbw, 25);
    ePaper.print(beer_temp);

    ePaper.drawCircle(125, 7, 2, GxEPD_BLACK);
    ePaper.setFont(&FreeSansBold12pt7b);
    ePaper.setCursor(125, 25);
    ePaper.print("C");


    // Area for FRIDGE TEMP
    char fridge_temp[] = "-";
    ePaper.setFont(&FreeSans9pt7b);
    ePaper.setCursor(148, 20);
    ePaper.print("Fridge:");
    //ePaper.drawRect(205, 0, 70, 35, GxEPD_BLACK);

    ePaper.setFont(&FreeSansBold18pt7b);
    if(FRIDGE_TEMP_F != -111){
        dtostrf(FRIDGE_TEMP_F, 1, 0, fridge_temp); 
    }
    ePaper.getTextBounds(fridge_temp, 0, 0, &tbx, &tby, &tbw, &tbh);
    ePaper.setCursor(271-tbw, 25);
    ePaper.print(fridge_temp);

    ePaper.drawCircle(278, 7, 2, GxEPD_BLACK);
    ePaper.setFont(&FreeSansBold12pt7b);
    ePaper.setCursor(278, 25);
    ePaper.print("C");


    // Area for BUBBLES PER MINUTE
    char bubbles[] = "-";
    ePaper.setFont(&FreeSans9pt7b);
    ePaper.setCursor(0, 55);
    ePaper.print("Bubbles:");       
    //display.drawRect(75, 37, 70, 27, GxEPD_BLACK);

    ePaper.setFont(&FreeSansBold12pt7b);
    dtostrf(BUBBLES_PER_MINUTE, 1, 0, bubbles); 
    ePaper.getTextBounds(bubbles, 0, 0, &tbx, &tby, &tbw, &tbh);
    ePaper.setCursor(142-tbw, 58);
    ePaper.print(bubbles);


    // Area for TARGET TEMP
    char target_temp[] = "-";
    ePaper.setFont(&FreeSans9pt7b);
    ePaper.setCursor(148, 55);
    ePaper.print("Target:");
    //display.drawRect(205, 37, 70, 27, GxEPD_BLACK);

    dtostrf(TARGET_TEMP, 1, 0, target_temp);
    ePaper.setFont(&FreeSansBold18pt7b); 
    ePaper.getTextBounds(target_temp, 0, 0, &tbx, &tby, &tbw, &tbh);
    ePaper.setCursor(270-tbw, 62);
    ePaper.print(target_temp);

    ePaper.setFont(&FreeSansBold12pt7b);
    ePaper.setCursor(278, 62);
    ePaper.print("C");
    ePaper.drawCircle(278, 45, 2, GxEPD_BLACK);

    //ePaper.display(true);

    //debug("TIMER for Display-> ");
    //debugf("Fridge: %.1f°C|", FRIDGE_TEMP_F);
    //debugf("Beer: %.1f°C\n",BEER_TEMP_F);
}

// function for EPD to show graph
void show_EPD_Graph(){

    // Area for Chart
    ePaper.drawRect(0,64,236,64,GxEPD_BLACK);

    ePaper.drawFastVLine(10, 70, 50, GxEPD_BLACK);
    ePaper.drawLine(10,70,5,75, GxEPD_BLACK);
    ePaper.drawLine(10,70,15,75, GxEPD_BLACK);

    ePaper.drawFastHLine(10, 120, 215, GxEPD_BLACK);
    ePaper.drawLine(225,120,215,115, GxEPD_BLACK);
    ePaper.drawLine(225,120,215,125, GxEPD_BLACK);

    for(int i = 0 ; i < 200; i = i + 2){
        ePaper.drawPixel(i+11,120-TARGET_TEMP,GxEPD_BLACK);
    }


    for(int i = 0; i < 200; i++){
        ePaper.drawPixel(i+11,120-CHART_VALUES[i],GxEPD_BLACK);
    }

}

// function for EPD to show status
void show_EPD_Status(){
    debugf("STATUS -> SWITCH[%d] | HEAT_UP[%d] | COOL_DOWN[%d]\n", SWITCH, SHOW_HEAT_UP, SHOW_COOL_DOWN);

    if(SWITCH) {
        if (SHOW_HEAT_UP) { 
            ePaper.drawBitmap(232, 64, EPD_Heat_bits, EPD_Heat_width, EPD_Heat_height, GxEPD_BLACK);
        } else if (SHOW_COOL_DOWN) {
            ePaper.drawBitmap(232, 64, EPD_Cool_bits, EPD_Cool_width, EPD_Cool_height, GxEPD_BLACK);
        } else {
            ePaper.drawBitmap(232, 64, EPD_On_bits, EPD_On_width, EPD_On_height, GxEPD_BLACK);
        }
    } else {
        ePaper.drawBitmap(232, 64, EPD_Off_bits, EPD_Off_width, EPD_Off_height, GxEPD_BLACK);
    }
}

// function for converting XBM-Files to Big Endian
/*
void convertEndianness(String name, unsigned char data[], int height, int width){

    int array_num = sizeof(data[]);
    int element = 0;
    uint8_t converted = 0;

    Serial.printf("=== Coverting Endianness [%i] ===\n");
    Serial.printf("#define %s_width %i\n",name, width);
    Serial.printf("#define %s_height %i\n",name, height);
    Serial.printf("static unsigned char %s_bits[] = {\n", name);

    for (int i = 0; i <array_num-1; i++ ) {
   
        for (int ii = 0; ii < 8; ii++) {
            if (data[i] & (1 << ii)) {
                converted |= (1 << (7 - ii));
            }
        }

        if(element == 0) { Serial.print("  "); }

        element++;    

        if(element<12) {
            Serial.printf("0x%02x, ", converted);
        } else {
            element = 0;
            Serial.printf("0x%02x,\n", converted);
        }
    }

    Serial.printf("0x%02x };\n", converted);
    Serial.println("====== Coverting End ======");
}
*/