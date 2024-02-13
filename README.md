# Fermentation-Control-System: Change Log
- Board: ESP32 D1 Mini NodeMCU 
- Platform.ini: https://docs.platformio.org/en/latest/boards/espressif32/wemos_d1_mini32.html
- Pinout: https://cdn.shopify.com/s/files/1/1509/1638/files/D1_Mini_ESP32_-_pinout.pdf?v=1604068668

- Board: AZDelivery LOLIN32 Lite V1.0.0
- Platform.ini: https://docs.platformio.org/en/latest/boards/espressif32/lolin32_lite.html
- Pinout: https://cdn.shopify.com/s/files/1/1509/1638/files/ESP32_Lolin32_Pinout.pdf?v=1608455156

## ToDo: Next Release (1.5):



## Release (1.6 - 1.9):
- Bugfixing


## Release (1.5):

- Start Timers immediately
```c
void setup() { ...
if( ( xOneShotTimer != NULL ) && ( xAutoReloadTimer != NULL ) ) { 
     // Start the software timers, using a block time of 0 (no block time). 
     xTimer1Started = xTimerStart( xOneShotTimer, 0 ); 
     xTimer2Started = xTimerStart( xAutoReloadTimer, 0 ); }
```
- Commpressor overheat deactivation
```C
void loop() { ...
if( (int)round(COMP_TEMP_F) > COMP_TEMP_THRESHOLD ) { 
     SWITCH = false; }
```
- Insert +/- Button on index.html to config target time
```CSS
.big-button {
  font-size: 24px;
  width: 50px;
  height: 50px;
  background-color: #04AA6D;
  border: 3px solid #04AA6D;
  border-radius: 6px;
  color: white;
  text-decoration: none;
  margin: 2px 2px;
  cursor: pointer;
}

.big-button:active {
  color: #04AA6D;
  background-color: white;
  border: 3px solid #04AA6D;
}
```

```html
<script>
     function callRestAPI() {          
          const xhttp = new XMLHttpRequest();
          const url = "/getdata?TARGET_TEMP=" + document.getElementById("TARGET_TEMP").value + "&TARGET_TIME=" + document.getElementById("TARGET_TIME").value;
          xhttp.onreadystatechange = function() { if (xhttp.readyState == 4 && xhttp.readyState == 200) { callback(xhttp.responseText); } }
          
          xhttp.open("GET", url, true);
          xhttp.send();
     }
     function TargetCount(updown) {
	       var value = document.getElementById("TARGET_TEMP").value;
            if (updown == "+"){
                if(value < 25) { value++; }
            } else {
                if(value > 1) { value--; }
            }
 	       document.getElementById("TARGET_TEMP").value = value;
            callRestAPI();
        }
</script>
...
<tr>
     <td class="txt_center"><b>Target Temp [&deg;C]:</b></td>
</tr>
<tr>
     <td>
          <input class="big-button" type="submit" value="-" onclick="javascript:TempCount('-')">
          <input class="big-font" type="number" min="20" max="99" size="4" name="TARGET_TEMP" id="TARGET_TEMP" value="%TARGET_TEMP%" onchange="javascript:callRestAPI()">
          <input class="big-button" type="submit" value="+" onclick="javascript:TempCount('+')">
      </td>
</tr>
```

```c
int COMP_TEMP = 0;
void loop() { ...
if( COMP_TEMP > COMP_TEMP_THRESHOLD ) { 
     SWITCH = false; }
```



## Next Release (1.0):
- Re-Organize the code
- Use LittleFS insteat of SPIFFS
- Use new Version of ElegantOTA
- Implement favicon 

## Release (0.89):
- Add calc.hmtl to do some brew related calculations 

## Release (0.88):
- Add new heating algorythm
```C
Line 51: int ROOM_TEMP = 0;
Line 657: ROOM_TEMP = (int)round(ROOM_TEMP_F);
Line 684:  if ((ROOM_TEMP <= TARGET_TEMP) && (CURR_TEMP < TARGET_TEMP - TEMP_HYSTERESIS)) {...
```

## Release (0.87): 
- Heatprotection Compressor:
    Line  53: `float COMP_TEMP_F = -22;`
    Line 460: `float temp_comp = sensors.getTempCByIndex(2);`
    Line 461: `if (COMP_TEMP_F == -22 || temp_comp > COMP_TEMP_F+3 || temp_comp < COMP_TEMP_F-3)
                  { COMP_TEMP_F = temp_comp;}`
    Line 604: `if (int COMP_TEMP_F > COMP_TEMP_SWITCHOFF)
                  { digitalWrite(COOL_DOWN, OFF);
                    info_text = "<ALERT>";
                    SHOW_COOL_DOWN = false;}`
  
- Temperaturschwellwert konfigurierbar machen
- config.json:
  Line 7: `"COMP_TEMP_THRESHOLD":50,`
  
- config.html:
  Lin 48: `<tr><td class="right">Compressor temp threshold [%deg;C]:</td><td><input type="number" name="COMP_TEMP_THRESHOLD" value="%COMP_TEMP_THRESHOLD%"></td></tr>`

- index.html:
  Line 29: `<tr><td class="right">Compressor Temp:</td><td><strong>%COMP_TEMP_THRESHOLD% &#x2103;</strong></td></tr>`

- main.ccp:
  Line  54: `int COMP_TEMP_THRESHOLD = 50;`
  Line 145:  `COMP_TEMP_THRESHOLD = doc["COMP_TEMP_THRESHOLD"];`
  Line 189:  `if(p->name() == "COMP_TEMP_THRESHOLD") {
                COMP_TEMP_THRESHOLD = p->value().toInt();
                doc["COMP_TEMP_THRESHOLD"] = COMP_TEMP_THRESHOLD;
            }`
  Line 231: `doc["COMP_TEMP_THRESHOLD"] = COMP_TEMP_THRESHOLD;`
  Line 351: `if(var == "COMP_TEMP_THRESHOLD"){  return String(COMP_TEMP_THRESHOLD);  }`


## Release (0.86):

- Send Float-Values to Grafana:
  Line 490: `doc["room_temp"] = int(ROOM_TEMP_F);` --> `doc["room_temp"] = ROOM_TEMP_F;`
  Line 491: `doc["fridge_temp"] = CURR_TEMP;` --> `doc["fridge_temp"] = CURR_TEMP_F;`

- Kompressor laufzeit konfigurierbar machen
- config.json:
  Line 7: `"COMP_RUNNING_TIME":60,`
  
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
