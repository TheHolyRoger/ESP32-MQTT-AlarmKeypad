# ESP32-MQTT-AlarmKeypad

Home Assistant Alarm keypad using ESP32 & MQTT.

https://github.com/TheHolyRoger/ESP32-MQTT-AlarmKeypad

More info & discussion: https://community.home-assistant.io/t/esp32-alarm-keypad/422952/2

v0.05

  
  Author:
   TheHolyRoger
  
  <a href="https://www.buymeacoffee.com/TheHoliestRoger" target="_blank"><img src="https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png" alt="Buy Me A Coffee" style="height: 41px !important;width: 174px !important;box-shadow: 0px 3px 2px 0px rgba(190, 190, 190, 0.5) !important;-webkit-box-shadow: 0px 3px 2px 0px rgba(190, 190, 190, 0.5) !important;" ></a>
  
  
  Contributions from:
  - devWaves

![ESP32-MQTT-AlarmKeypad](/doc_resources/alarm_keypad.png?raw=true "ESP32-MQTT-AlarmKeypad")
	
Code can be installed using Arduino IDE.

Designed for the standard "4x4 Matrix Keypad" you can find online. Tested with the hard-button version but should work with the membrane version too.


Based off of the work from https://github.com/devWaves/SwitchBot-MQTT-BLE-ESP32

Default Button config:
  - **#**: Broadcast Alarm **ARM** code
  - **D**: Broadcast Alarm **DISARM** code
  - **B, C**: Clear inputted code
  - **A**: Ignored (Both my keypads have issues with `B`/`C` being confused with `A`)

Example LED config:
  - **SUCCESS**: First (green) LED on pin 15
  - **FAILURE**: Second (red) LED on pin 4
  - **WARNING**: Third (yellow) LED on pin 18
  - **STATUS**: Fourth (blue) LED on pin 19
  - **KEYPRESS**: Second (red) LED on pin 4

Notes:
  - Supports Home Assistant MQTT Discovery
  - OTA update included. Go to ESP32 IP address in browser. In Arduino IDE menu - Sketch / Export compile Binary . Upload the .bin file
  - Designed for 4x4 keypad but should work with 3x3 keypad by modifying `keys` in the advanced section.
  - Also works if no code is set for arm/disarm, simply press the arm/disarm key without entering a code.

**ESPMQTTTopic**

\<ESPMQTTTopic\> = \<mqtt_main_topic\>/\<host\>
	
  - Default = esp32_alarm_keypad/home_alarm_keypad

**ESP32 will SUBSCRIBE to MQTT 'set' topic for 2 LED pins**

  - \<ESPMQTTTopic\>/status_light/set
  - \<ESPMQTTTopic\>/warning_light/set

Send a payload to the 'set' topic.
Strings:
  - "ON"
  - "OFF"
	
**ESP32 will RESPOND with MQTT on 'state' topic for 2 LED pins**

  - \<ESPMQTTTopic\>/status_light/state
  - \<ESPMQTTTopic\>/warning_light/state

Example payload:
  - "ON"
  - "OFF"

**ESP32 will BROADCAST to MQTT 'code_exit' topic upon press of the Arm key with the entered code**

  - \<ESPMQTTTopic\>/code_exit

Use a Home Assistant automation to listen to this topic and then call the alarm arm service with the received code.
Strings:
  - \<ALARM_CODE\>

**ESP32 will BROADCAST to MQTT 'code_entry' topic upon press of the Disarm key with the entered code**

  - \<ESPMQTTTopic\>/code_entry

Use a Home Assistant automation to listen to this topic and then call the alarm disarm service with the received code.
Strings:
  - \<ALARM_CODE\>

**ESP32 will SUBSCRIBE to MQTT 'code_success' topic which flashes the LEDs, used for successful/failed alarm code entry**

  - \<ESPMQTTTopic\>/code_success

Use a Home Assistant automation to detect success/failure when arming/disarming the alarm, then send a payload to the 'code_success' MQTT topic. 
Strings:
  - { "success": true }
  - { "success": false }

**ESP32 will RESPOND with MQTT on ESPMQTTTopic with ESP32 status**

  - \<ESPMQTTTopic\>

example payloads:
  - {"status":"waiting"}
  - {"status":"idle"}
  - {"status":"boot"}
  - {"status":"errorParsingJSON"}
  - {"status":"codeSuccess"}
  - {"status":"codeSuccessFail"}
      
<br>
<br>

## Steps to Install on ESP32 - Using Arduino IDE ##

1. Install Arduino IDE
2. Setup IDE for proper ESP32 type
     https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/
3. Install EspMQTTClient library
4. Install ArduinoJson library
5. Modify code for your Wifi and MQTT configurations

	Configurations to change can be found in the code under these line...
	```
	/****************** CONFIGURATIONS TO CHANGE *******************/

	/********** REQUIRED SETTINGS TO CHANGE **********/
	```

	Stop when you see this line...
	```
	/********** ADVANCED SETTINGS - ONLY NEED TO CHANGE IF YOU WANT TO TWEAK SETTINGS **********/
	```
6. Compile and upload to ESP32 (I am using a generic unbranded ESP32 with the "DOIT ESP32 DEVKIT v1" board setup in Arduino)
7. Reboot ESP32 plug it in with 5v usb (no data needed)

<br>
<br>

## Example HASS Automations

### Listen for ARM / Exit code
```yaml

alias: '[Alarm] Keypad Exit'
description: ''
trigger:
  - platform: mqtt
    topic: esp32_alarm_keypad/home_alarm_keypad/code_exit
condition: []
action:
  - service: alarm_control_panel.alarm_arm_away
    data:
      code: '{{ trigger.payload | string }}'
    target:
      entity_id: alarm_control_panel.alarmo  # Adjust for your alarm entity
  - delay:
      hours: 0
      minutes: 0
      seconds: 1
      milliseconds: 0
  - choose:
      - conditions:
          - condition: not
            conditions:
              - condition: state
                entity_id: alarm_control_panel.alarmo  # Adjust for your alarm entity
                state: arming
        sequence:
          - service: mqtt.publish
            data:
              topic: esp32_alarm_keypad/home_alarm_keypad/code_success
              payload: '{ "success": false }'
    default: []
mode: single

```

### Listen for DISARM / Entry code
```yaml

alias: '[Alarm] Keypad Entry'
description: ''
trigger:
  - platform: mqtt
    topic: esp32_alarm_keypad/home_alarm_keypad/code_entry
condition: []
action:
  - service: alarm_control_panel.alarm_disarm
    data:
      code: '{{ trigger.payload | string }}'
    target:
      entity_id: alarm_control_panel.alarmo  # Adjust for your alarm entity
  - delay:
      hours: 0
      minutes: 0
      seconds: 1
      milliseconds: 0
  - choose:
      - conditions:
          - condition: not
            conditions:
              - condition: state
                entity_id: alarm_control_panel.alarmo  # Adjust for your alarm entity
                state: disarmed
        sequence:
          - service: mqtt.publish
            data:
              topic: esp32_alarm_keypad/home_alarm_keypad/code_success
              payload: '{ "success": false }'
    default: []
mode: single

```

### Activating Warning/Status Lights
Simply add triggers for your alarm entity changing state to turn the corresponding `LIGHT` entities on/off.

E.g.
  - **ARMING**: Turn `WARNING` light `ON`.
  - **ARMED**: Turn `STATUS` light `ON`.
  - **DISARMED**: Turn `STATUS` and `WARNING` lights `OFF`.
  - **ENTRY**: Turn `STATUS` and `WARNING` lights `ON`.



