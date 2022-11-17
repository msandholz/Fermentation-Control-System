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


void saveConfig() {
   
    if(!SPIFFS.begin(true)){
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
     // Open file for writing
   File config_file = SPIFFS.open("/config.json", FILE_WRITE);

  if (!config_file) {
    Serial.println(F("Failed to create file"));
    return;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<256> doc;

doc["HOSTNAME"] = "John";
doc["WIFI_SSID"] = "WLAN";
doc["WIFI_PW"] = "xyz";
doc["MQTT_SERVER"] = "";
doc["TARGET_TEMP"] = 10;
doc["TEMP_HYSTERESIS"] = 1;
doc["POWER_HEAT"] = 120;
doc["POWER_COOL"] = 240;
doc["POWER_IDLE"] = 10;

  // Serialize JSON to file
  if (serializeJson(doc, config_file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  config_file.close();
}
