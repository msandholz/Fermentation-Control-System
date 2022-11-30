# Fermentation-Control-System

## ToDo: Next Release (0.8):
- calculate align position of current temperature (18pt) : int offset = ((120 - ((CURR_TEMP_F.length()+1)*18))/2);
- Fehler korrigieren: Zeile 
        String ext_url = doc["EXTERNAL_URL"];     
        EXTERNAL_URL = mqtt_broker.c_str(); to EXTERNAL_URL = ext_url.c_str();
