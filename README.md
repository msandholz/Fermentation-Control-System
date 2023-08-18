# Fermentation-Control-System: Change Log


## ToDo: Next Release (0.86):

- Send Float-Values to Grafana:
  Line 490: `doc["room_temp"] = int(ROOM_TEMP_F);` --> `doc["room_temp"] = ROOM_TEMP_F;`
  Line 491: `doc["fridge_temp"] = CURR_TEMP;` --> `doc["fridge_temp"] = CURR_TEMP_F;`

- Kompressor laufzeit konfigurierbar machen
- config.json:
  Line 7: `"COMP_RUNNING_TIME":60,`

`long lastCompMillis = 0;
long COMP_RUNNING_TIME = 60*1000;
boolean pinstate = false;

void switchCompressor(boolean isOn) {

	if(isON=!pinstate){
		lastCompMillis = millis();
		if ((millis() - lastMillis) < COMP_RUNNING_TIME) {
			digitalWrite(COOL_DOWN, !isON); 
		} else{
			digitalWrite(COOL_DOWN, ON); 
			pinstate = isOn;
		}
	} 
}`

  

- config.html:
  Lin 48: `<tr><td class="right">Compressor running time:</td><td><input type="number" name="COMP_RUNNING_TIME" value="%COMP_RUNNING_TIME%"></td></tr>`
  
- main.ccp:
  Line 55:   `int COMP_RUNNING_TIME = 60;`
  Line 145:  `COMP_RUNNING_TIME = doc["COMP_RUNNING_TIME"];`
  Line 189:  `if(p->name() == "COMP_RUNNING_TIME") {
                COMP_RUNNING_TIME = p->value().toInt();
                doc["COMP_RUNNING_TIME"] = COMP_RUNNING_TIME;
            }`
  Line 231: `doc["COMP_RUNNING_TIME"] = COMP_RUNNING_TIME;`
  Line 351: `if(var == "COMP_RUNNING_TIME"){  return String(COMP_RUNNING_TIME);  }`
-  
  



- Heatprotection Compressor:
    Line  53: `float COMP_TEMP_F = -22;`
    Line  54: `int COMP_TEMP_SWITCHOFF = 50;`
    Line 460: `float temp_comp = sensors.getTempCByIndex(2);`
    Line 461: `if (COMP_TEMP_F == -22 || temp_comp > COMP_TEMP_F+3 || temp_comp < COMP_TEMP_F-3)
                  { COMP_TEMP_F = temp_comp;}`
    Line 604: `if (int COMP_TEMP_F > COMP_TEMP_SWITCHOFF)
                  { digitalWrite(COOL_DOWN, OFF);
                    info_text = "<ALERT>";
                    SHOW_COOL_DOWN = false;}`


## Release (0.85):
- Line 656: `CURR_TEMP = (int)CURR_TEMP_F;` --> Line 594: `CURR_TEMP = (round)CURR_TEMP_F;`
- Error correction temperature sensor:
  Change Line 456: `if (temp_fridge > 0) { CURR_TEMP_F = temp_fridge; }` to

  Line  51: `float CURR_TEMP_F = -22;`
  Line 455: `float temp_fridge= sensors.getTempCByIndex(0);`
  Line 456: `if (CURR_TEMP_F == -22 || temp_fridge > CURR_TEMP_F+3 && temp_fridge < CURR_TEMP_F-3)`
                  `{ CURR_TEMP_F = temp_fridge;}`

- Change Hysterese algorythm:
  `if (!SHOW_COOL_DOWN) {
        if (CURR_TEMP  > TARGET_TEMP + TEMP_HYSTERESIS) {
            digitalWrite(COOL_DOWN, ON);
            info_text = "<COOL DOWN>";
            SHOW_COOL_DOWN = true;
        }
    } else {
        if (CURR_TEMP < TARGET_TEMP - TEMP_HYSTERESIS) {
             digitalWrite(COOL_DOWN, OFF);
             SHOW_COOL_DOWN = false;
        }
    }`

## Release 0.84:
- Check calculation of character positioning in line 938
- Implement delay to switch off the compressor (configurable in seconds)

## Release 0.83
- Solve bug in line 140

String ext_url = doc["EXTERNAL_URL"];     
EXTERNAL_URL = mqtt_broker.c_str()

## Release 0.82:
- Change of steering logic (line 592-618)
- Solve error in setting variable COOL_HEAT = 20
