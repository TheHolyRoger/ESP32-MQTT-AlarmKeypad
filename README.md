# ESP32-MQTT-AlarmKeypad

Home Assistant Alarm keypad using ESP32 & MQTT.

https://github.com/TheHolyRoger/ESP32-MQTT-AlarmKeypad

More info & discussion: https://community.home-assistant.io/t/esp32-alarm-keypad/422952/2

v0.04

  
  Author:
   TheHolyRoger
  
  <a href="https://www.buymeacoffee.com/TheHoliestRoger" target="_blank"><img src="https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png" alt="Buy Me A Coffee" style="height: 41px !important;width: 174px !important;box-shadow: 0px 3px 2px 0px rgba(190, 190, 190, 0.5) !important;-webkit-box-shadow: 0px 3px 2px 0px rgba(190, 190, 190, 0.5) !important;" ></a>
  
  
  Contributions from:
  - devWaves

![ESP32-MQTT-AlarmKeypad](/doc_resources/alarm_keypad.png?raw=true "ESP32-MQTT-AlarmKeypad")
	
Code can be installed using Arduino IDE.

Designed for the standard "4x4 Matrix Keypad" you can find online. Tested with the hard-button version but should work with the membrane version too.


Based off of the work from https://github.com/devWaves/SwitchBot-MQTT-BLE-ESP32

Notes:
  - Supports Home Assistant MQTT Discovery
  - OTA update included. Go to ESP32 IP address in browser. In Arduino IDE menu - Sketch / Export compile Binary . Upload the .bin file
  - Designed for 4x4 keypad but should work with 3x3 keypad by modifying `keys` in the advanced section.

**ESPMQTTTopic**

\<ESPMQTTTopic\> = \<mqtt_main_topic\>/\<host\>
	
  - Default = esp32_alarm_keypad/home_alarm_keypad

**ESP32 will subscribe to MQTT 'set' topic for 2 LED pins**

  - \<ESPMQTTTopic\>/status_light/set
  - \<ESPMQTTTopic\>/warning_light/set

Send a payload to the 'set' topic.
Strings:
  - "ON"
  - "OFF"
	
**ESP32 will respond with MQTT on 'state' topic for 2 LED pins**

  - \<ESPMQTTTopic\>/status_light/state
  - \<ESPMQTTTopic\>/warning_light/state

Example payload:
  - "ON"
  - "OFF"

**ESP32 will subscribe to MQTT 'code_success' topic which flashes the LEDs, used for successful/failed alarm code entry**

  - \<ESPMQTTTopic\>/code_success

Use a Home Assistant automation to detect success/failure then send a payload to the 'code_success' MQTT topic. 
Strings:
  - { "success": true }
  - { "success": false }

**ESP32 will respond with MQTT on ESPMQTTTopic with ESP32 status**

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

