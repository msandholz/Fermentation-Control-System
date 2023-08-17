# Fermentation-Control-System

## ToDo: Next Release (0.85):
- Line 656: `CURR_TEMP = (int)CURR_TEMP_F;` --> Line 594: `CURR_TEMP = (round)CURR_TEMP_F;`
- Error correction temperature sensor:
  Change Line 456: `if (temp_fridge > 0) { CURR_TEMP_F = temp_fridge; }` to

  Line  51: `float CURR_TEMP_F = -22;`
  Line 455: `float temp_fridge= sensors.getTempCByIndex(0);`
  Line 456: `if (CURR_TEMP_F == -22 || temp_fridge > CURR_TEMP_F+3 && temp_fridge < CURR_TEMP_F-3)`
                  `{ CURR_TEMP_F = temp_fridge;}`

- Change Hysterese algorythm:
  `if (!SHOW_COOL_DOWN) {
        if (CURR_TEMP  >= TARGET_TEMP + TEMP_HYSTERESIS) {
            digitalWrite(COOL_DOWN, ON);
            info_text = "<COOL DOWN>";
            SHOW_COOL_DOWN = true;
        }
    } else {
        if (CURR_TEMP <= TARGET_TEMP - TEMP_HYSTERESIS) {
             digitalWrite(COOL_DOWN, OFF);
             SHOW_COOL_DOWN = false;
        }
    }`

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
