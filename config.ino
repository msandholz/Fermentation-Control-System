void loadConfig() {
   if(!SPIFFS.begin(true)){
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
  
    File config_file = SPIFFS.open("/config.json");
 
    if(!config_file){
        Serial.println("Failed to open file for reading");
        return;
    }
  
    StaticJsonDocument<512> doc;

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, config_file);
  
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }
  
  const char* HOSTNAME = doc["HOSTNAME"]; // "John"
  const char* WIFI_SSID = doc["WIFI_SSID"]; // "WLAN"
  const char* WIFI_PW = doc["WIFI_PW"]; // "xyz"
  const char* MQTT_SERVER = doc["MQTT_SERVER"]; // nullptr
  int TARGET_TEMP = doc["TARGET_TEMP"]; // 10
  int TEMP_HYSTERESIS = doc["TEMP_HYSTERESIS"]; // 1
  int POWER_HEAT = doc["POWER_HEAT"]; // 120
  int POWER_COOL = doc["POWER_COOL"]; // 240
  int POWER_IDLE = doc["POWER_IDLE"]; // 10

    config_file.close();
}
