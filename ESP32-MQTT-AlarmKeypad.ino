/** ESP32-MQTT-AlarmKeypad
*/

#include <EspMQTTClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <Keypad.h>

/****************** CONFIGURATIONS TO CHANGE *******************/

/********** REQUIRED SETTINGS TO CHANGE **********/

/* Wifi Settings */
static const char* friendly_name = "[ESP32] Home Alarm Keypad";         //  Friendly name for ESP32 device.
static const char* host = "home_alarm_keypad";                  //  Unique name for ESP32. The name detected by your router and MQTT. Be sure to use unique hostnames. Host is the MQTT Client name and is used in MQTT topics
static const char* ssid = "wifi_ssid";                           //  WIFI SSID
static const char* password = "wifi_password";                   //  WIFI Password

/* MQTT Settings */
/* MQTT Client name is set to WIFI host from Wifi Settings*/
static const char* mqtt_host = "ip_address";                       //  MQTT Broker server ip
static const char* mqtt_user = "username";                         //  MQTT Broker username. If empty or NULL, no authentication will be used
static const char* mqtt_pass = "password";                         //  MQTT Broker password
static const int mqtt_port = 1883;                                 //  MQTT Port
static const std::string mqtt_main_topic = "esp32_alarm_keypad";   //  MQTT main topic


/********** ADVANCED SETTINGS - ONLY NEED TO CHANGE IF YOU WANT TO TWEAK SETTINGS **********/

#define ROW_NUM     4     // 4x4 keypad has four rows
#define COLUMN_NUM  4     // 4x4 keypad has four columns

static byte pin_rows[ROW_NUM]      = {26, 25, 33, 32};   // GPIO 26, GPIO 25, GPIO 33, GPIO 32 connect to the row pins
static byte pin_column[COLUMN_NUM] = {13, 12, 14, 27};   // GPIO 13, GPIO 12, GPIO 14, GPIO 27 connect to the column pins

static const bool allowZeroLengthDisarm = false;       // Allow submitting disarm service without any code entered.
static const bool allowZeroLengthArm = true;           // Allow submitting arm service without any code entered.

static const char submit_key = '#';                    // Key to press to ARM the alarm.
static const char submit_key_alt = 'A';                // Key to press to ARM the alarm. (Alternate)
static const char disarm_key = 'D';                    // Key to press to DISARM the alarm.
static const char clear_key = 'C';                     // Key to press to clear the entered code.
static const char clear_key_alt = 'B';                 // Key to press to clear the entered code.

/* ESP32 LED Settings */
#define LED_SUCCESS_PIN 15                           // GPIO 15 connects to the SUCCESS LED.
#define LED_FAILURE_PIN 4                            // GPIO 4 connects to the FAILURE LED.
#define LED_WARNING_PIN 18                           // GPIO 18 connects to the WARNING LED.
#define LED_STATUS_PIN 19                            // GPIO 19 connects to the STATUS LED.
#define LED_KEYPRESS_PIN 4                           // GPIO 4 connects to the KEYPRESS LED. (same as FAILURE)
//#define LED_BUILTIN 2                              // If your board doesn't have a defined LED_BUILTIN, uncomment this line and replace 2 with the LED pin value
#define LED_PIN LED_BUILTIN                          // If your board doesn't have a defined LED_BUILTIN (You will get a compile error), uncomment the line above

#define USE_EXTRA_FUNCTION_KEY 0                        // Change this to 1 to enable extra function key
static const char extra_function_key = '_';             // Key to press to send extra function event

static const char ignoredKeySet[6] = { '_' };          // Add any keys to be ignored.

static const char keypad_map[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

static const bool ledHighEqualsON = true;            // ESP32 board LED ON=HIGH (Default). If your ESP32 LED is mostly ON instead of OFF, then set this value to false
static const bool ledOnBootScan = true;              // Turn on LED during initial boot scan
static const bool ledOnKeypress = true;               // Blink LED on keypresses.

static const int minimumAlarmCodeLength = 4;
static const int maximumAlarmCodeLength = 6;

/* Webserver Settings */
static const bool useLoginScreen = false;            //  use a basic login popup to avoid unwanted access
static const String otaUserId = "admin";             //  user Id for OTA update. Ignore if useLoginScreen = false
static const String otaPass = "admin";               //  password for OTA update. Ignore if useLoginScreen = false
static WebServer server(80);                         //  default port 80

/* Home Assistant Settings */
static const bool home_assistant_mqtt_discovery = true;                    // Enable to publish Home Assistant MQTT Discovery config
static const std::string home_assistant_mqtt_prefix = "homeassistant";     // MQTT Home Assistant prefix

/* Gateway General Settings */
static const int waitForMQTTRetainMessages = 10;      // Only for bots in simulated ON/OFF: On boot ESP32 will look for retained MQTT state messages for X secs, otherwise default state is used

static const bool printSerialOutputForDebugging = false;  // Only set to true when you want to debug an issue from Arduino IDE. Lots of Serial output from scanning can crash the ESP32

/*************************************************************/

/* ANYTHING CHANGED BELOW THIS COMMENT MAY RESULT IN ISSUES - ALL SETTINGS TO CONFIGURE ARE ABOVE THIS LINE */

static const String versionNum = "v0.06";

/*
   Server Index Page
*/

static const String serverIndex =
  "<link rel='stylesheet' href='https://code.jquery.com/ui/1.12.1/themes/base/jquery-ui.css'>"
  "<style> .ui-progressbar { position: relative; }"
  ".progress-label { position: absolute; width: 100%; text-align: center; top: 6px; font-weight: bold; text-shadow: 1px 1px 0 #fff; }</style>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<script src='https://code.jquery.com/ui/1.12.1/jquery-ui.js'></script>"
  "<script>"
  "$( function() { var progressbar = $( \"#progressbar\" ), progressLabel = $( '.progress-label' );"
  "progressbar.progressbar({ value: '0', change: function() {"
  "progressLabel.text( progressbar.progressbar( 'value' ) + '%' );},"
  "complete: function() { progressLabel.text( 'Uploaded!' ); } });"
  "progressbar.find( '.ui-progressbar-value' ).css({'background': '#8fdbb2'});"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "progressbar.progressbar( \"value\", Math.round(per*100) );"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
  "},"
  "error: function (a, b, c) {"
  "}"
  "});"
  "});"
  "});"
  "</script>"
  "<table bgcolor='A09F9F' align='center' style='top: 250px;position: relative;width: 30%;'>"
  "<tr>"
  "<td colspan=2>"
  "<center><font size=5><b>Alarm Keypad ESP32 MQTT version: " + versionNum + "</b></font> <font size=1><b>(Unofficial)</b></font></center>"
  "<center><font size=3>Hostname/MQTT Client Name: " + std::string(host).c_str() + "</font></center>"
  "<center><font size=3>MQTT Main Topic: " + std::string(mqtt_main_topic).c_str() + "</font></center>"
  "<br>"
  "</td>"
  "<br>"
  "<br>"
  "</tr>"
  "<tr>"
  "<td>Upload .bin file:</td>"
  "<td><input type='file' name='update'></td>"
  "</tr>"
  "<tr>"
  "<td colspan='2'><div id='progressbar'><div class='progress-label'>0%</div></div></td>"
  "</tr>"
  "<tr>"
  "<td colspan='2' align='center'><input type='submit' value='Update'></td>"
  "</tr>"
  "</table>"
  "</form>";

static EspMQTTClient client(
  ssid,
  password,
  mqtt_host,
  (mqtt_user == NULL || strlen(mqtt_user) < 1) ? NULL : mqtt_user,
  (mqtt_user == NULL || strlen(mqtt_user) < 1) ? NULL : mqtt_pass,
  host,
  mqtt_port
);

static const uint16_t mqtt_packet_size = 1024;
static const std::string manufacturer = "Roger Alarm Co";

static int ledONValue = HIGH;
static int ledOFFValue = LOW;
static bool deviceHasBooted = false;
static bool deviceFirstAdvertisementsSent = false;
static char alarmCode[6];
//static bool waitForDeviceCreation = false;
static char aBuffer[200];
static const std::string ON_STRING = "ON";
static const char* ON_STR = ON_STRING.c_str();
static const std::string OFF_STRING = "OFF";
static const char* OFF_STR = OFF_STRING.c_str();
static const std::string ESPMQTTTopic = mqtt_main_topic + "/" + std::string(host);
static const std::string esp32Topic = ESPMQTTTopic + "/esp32";
static const std::string rssiStdStr = esp32Topic + "/rssi";
static const std::string lastWillStr = ESPMQTTTopic + "/lastwill";
static const char* lastWill = lastWillStr.c_str();
static const std::string alarmLightStatusTopic = ESPMQTTTopic + "/status_light/";
static const std::string alarmLightWarningTopic = ESPMQTTTopic + "/warning_light/";
static const std::string codeSuccessStdStr = ESPMQTTTopic + "/code_success";

Keypad keypad = Keypad( makeKeymap(keypad_map), pin_rows, pin_column, ROW_NUM, COLUMN_NUM );

static long lastOnlinePublished = 0;
#if USE_EXTRA_FUNCTION_KEY == 1
  static const char specialKeySet[6] = {submit_key, submit_key_alt, clear_key, clear_key_alt, disarm_key, extra_function_key};
  static const bool useExtraFunctionKey = true;
#else
  static const char specialKeySet[6] = {submit_key, submit_key_alt, clear_key, clear_key_alt, disarm_key};
  static const bool useExtraFunctionKey = false;
#endif
static const int specialKeyLength = sizeof(specialKeySet) / sizeof(specialKeySet[0]);
static const int ignoredKeyLength = sizeof(ignoredKeySet) / sizeof(ignoredKeySet[0]);

void publishLastwillOnline() {
  if ((millis() - lastOnlinePublished) > 30000) {
    if (client.isConnected()) {
      client.publish(lastWill, "online", true);
      lastOnlinePublished = millis();
      String rssi = String(WiFi.RSSI());
      client.publish(rssiStdStr.c_str(), rssi.c_str());
    }
  }
}

void publishHomeAssistantDiscoveryESPConfig() {
  String wifiMAC = String(WiFi.macAddress());
  client.publish((home_assistant_mqtt_prefix + "/sensor/" + host + "/linkquality/config").c_str(), ("{\"~\":\"" + esp32Topic + "\"," +
                 + "\"name\":\"" + friendly_name + " Linkquality\"," +
                 + "\"device\": {\"identifiers\":[\"espmqtt_" + host + "_" + wifiMAC.c_str() + "\"],\"manufacturer\":\"" + manufacturer + "\",\"model\":\"" + "ESP32" + "\",\"name\": \"" + friendly_name + "\" }," +
                 + "\"avty_t\": \"" + lastWill + "\"," +
                 + "\"uniq_id\":\"espmqtt_" + host + "_" + wifiMAC.c_str() + "_linkquality\"," +
                 + "\"stat_t\":\"~/rssi\"," +
                 + "\"icon\":\"mdi:signal\"," +
                 + "\"unit_of_meas\": \"rssi\"}").c_str(), true);
  client.publish((home_assistant_mqtt_prefix + "/light/" + host + "/status_light/config").c_str(), ("{\"~\":\"" + (alarmLightStatusTopic) + "\"," +
                 + "\"name\":\"" + friendly_name + " Status Light\"," +
                 + "\"device\": {\"identifiers\":[\"espmqtt_" + host + "_" + wifiMAC.c_str() + "\"],\"manufacturer\":\"" + manufacturer + "\",\"model\":\"" + "ESP32" + "\",\"name\": \"" + friendly_name + "\" }," +
                 + "\"avty_t\": \"" + lastWill + "\"," +
                 + "\"uniq_id\":\"espmqtt_" + host + "_" + wifiMAC.c_str() + "_status_light\"," +
                 + "\"stat_t\":\"~/state\"," +
                 + "\"cmd_t\": \"~/set\" }").c_str(), true);
  client.publish((home_assistant_mqtt_prefix + "/light/" + host + "/warning_light/config").c_str(), ("{\"~\":\"" + (alarmLightWarningTopic) + "\"," +
                 + "\"name\":\"" + friendly_name + " Warning Light\"," +
                 + "\"device\": {\"identifiers\":[\"espmqtt_" + host + "_" + wifiMAC.c_str() + "\"],\"manufacturer\":\"" + manufacturer + "\",\"model\":\"" + "ESP32" + "\",\"name\": \"" + friendly_name + "\" }," +
                 + "\"avty_t\": \"" + lastWill + "\"," +
                 + "\"uniq_id\":\"espmqtt_" + host + "_" + wifiMAC.c_str() + "_warning_light\"," +
                 + "\"stat_t\":\"~/state\"," +
                 + "\"cmd_t\": \"~/set\" }").c_str(), true);
}

void publishHomeAssistantAdvertisements() {
  
  client.publish((alarmLightStatusTopic + "/state").c_str(), OFF_STR, true);
  client.publish((alarmLightWarningTopic + "/state").c_str(), OFF_STR, true);
}

void setup () {
  if (ledHighEqualsON) {
    ledONValue = HIGH;
    ledOFFValue = LOW;
  }
  else {
    ledONValue = LOW;
    ledOFFValue = HIGH;
  }
  pinMode (LED_PIN, OUTPUT);
  pinMode (LED_SUCCESS_PIN, OUTPUT);
  pinMode (LED_FAILURE_PIN, OUTPUT);
  pinMode (LED_STATUS_PIN, OUTPUT);
  pinMode (LED_WARNING_PIN, OUTPUT);
  pinMode (LED_KEYPRESS_PIN, OUTPUT);

  Serial.begin(115200);

  // Connect to WiFi network
  WiFi.begin(ssid, password);
  if (printSerialOutputForDebugging) {
    Serial.println("");
  }

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (printSerialOutputForDebugging) {
      Serial.print(".");
    }
  }
  if (printSerialOutputForDebugging) {
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    if (printSerialOutputForDebugging) {
      Serial.println("Error setting up MDNS responder!");
    }
    while (1) {
      delay(1000);
    }
  }
  if (printSerialOutputForDebugging) {
    Serial.println("mDNS responder started");
  }
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    if (useLoginScreen) {
      if (!server.authenticate(otaUserId.c_str(), otaPass.c_str())) {
        return server.requestAuthentication();
      }
    }
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    if (useLoginScreen) {
      if (!server.authenticate(otaUserId.c_str(), otaPass.c_str())) {
        return server.requestAuthentication();
      }
    }
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      if (printSerialOutputForDebugging) {
        Serial.printf("Update: %s\n", upload.filename.c_str());
      }
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        if (printSerialOutputForDebugging) {
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        }
        blinkESP32LED();
        blinkColouredLEDBootSequence();
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();

  client.setMqttReconnectionAttemptDelay(100);
  client.enableLastWillMessage(lastWill, "offline");
  client.setKeepAlive(60);
  client.setMaxPacketSize(mqtt_packet_size);

  Serial.println("Alarm Keypad ESP32 starting...");
  if (!printSerialOutputForDebugging) {
    Serial.println("Set printSerialOutputForDebugging = true to see more Serial output");
  }
}

void flashColouredLEDWithDelay(int PinLED, int onDelay, int PinLEDSecondary = 0) {
  digitalWrite(LED_PIN, ledONValue);
  digitalWrite(PinLED, ledONValue);
  if (PinLEDSecondary > 0) {
    digitalWrite(PinLEDSecondary, ledONValue);
  }
  delay(onDelay);
  digitalWrite(LED_PIN, ledOFFValue);
  digitalWrite(PinLED, ledOFFValue);
  if (PinLEDSecondary > 0) {
    digitalWrite(PinLEDSecondary, ledOFFValue);
  }
}

void blinkESP32LED() {
  if (client.isConnected()) {
    if (printSerialOutputForDebugging) {
      Serial.println("Blinking ESP LED...");
    }

    digitalWrite(LED_PIN, ledONValue);
    delay(200);
    digitalWrite(LED_PIN, ledOFFValue);
  }
}

void blinkColouredLEDSuccess() {
  if (printSerialOutputForDebugging) {
    Serial.println("Blinking LED for Success...");
  }
  for (int i=0; i<3; ++i) {
    flashColouredLEDWithDelay(LED_SUCCESS_PIN, 200);
    if (i < 2) {
      delay(40);
    }
  }
}

void blinkColouredLEDBootSequence() {
  if (printSerialOutputForDebugging) {
    Serial.println("Blinking LEDs for Boot...");
  }
  flashColouredLEDWithDelay(LED_SUCCESS_PIN, 100);
  delay(1);
  flashColouredLEDWithDelay(LED_FAILURE_PIN, 100);
  delay(1);
  flashColouredLEDWithDelay(LED_WARNING_PIN, 100);
  delay(1);
  flashColouredLEDWithDelay(LED_STATUS_PIN, 100);
  delay(10);
  for (int i=0; i<3; ++i) {
    flashColouredLEDWithDelay(LED_SUCCESS_PIN, 200);
    if (i < 2) {
      delay(40);
    }
  }
}

void blinkLEDClear() {
  if (printSerialOutputForDebugging) {
    Serial.println("Blinking LED for Clear...");
  }

  for (int i=0; i<2; ++i) {
    flashColouredLEDWithDelay(LED_SUCCESS_PIN, 50, LED_FAILURE_PIN);
    if (i < 1) {
      delay(50);
    }
  }
}

void blinkLEDFail() {
  if (printSerialOutputForDebugging) {
    Serial.println("Blinking LED for Failure...");
  }
  for (int i=0; i<3; ++i) {
    int onDelay;
    if (i < 2) {
      onDelay = 200;
    } else {
      onDelay = 600;
    }
    flashColouredLEDWithDelay(LED_FAILURE_PIN, onDelay);
    if (i < 2) {
      delay(50);
    }
  }
}

long retainStartTime = 0;
bool waitForRetained = true;

boolean isSpecialKey(char key)
{
  for (int x = 0; x < specialKeyLength; x++)
  {
    if (specialKeySet[x] == key)
    {
      return true;
    }    
  } 
  return false;
}

boolean isIgnoredKey(char key)
{
  for (int x = 0; x < ignoredKeyLength; x++)
  {
    if (ignoredKeySet[x] == key)
    {
      return true;
    }    
  } 
  return false;
}

void check_keypad () {
  char key = keypad.getKey();
  if (key) {
    if (!isIgnoredKey(key) && ledOnKeypress) {
      digitalWrite(LED_PIN, ledONValue);
      digitalWrite(LED_KEYPRESS_PIN, ledONValue);
    }
    if (printSerialOutputForDebugging) {
      Serial.print("Received keypress: ");
      Serial.println(String(key));
    }
    if (!isIgnoredKey(key) && !isSpecialKey(key)) {
      strncat(alarmCode, &key, 1);
    }
    if (printSerialOutputForDebugging) {
      Serial.print("Current Alarm Code: ");
      Serial.println(String(alarmCode));
    }

    if ((allowZeroLengthDisarm || strlen(alarmCode) >= minimumAlarmCodeLength) && (key == disarm_key || keypad.isPressed(disarm_key))) {
      if (printSerialOutputForDebugging) {
        Serial.print("Submitting Disarm Alarm Code: ");
        Serial.println(String(alarmCode));
      }
      client.publish((ESPMQTTTopic + "/code_entry").c_str(), String(alarmCode));
      strcpy(alarmCode, "");
    }
    else if (key == clear_key || key == clear_key_alt || keypad.isPressed(clear_key) || keypad.isPressed(clear_key_alt)) {
      strcpy(alarmCode, "");
      if (printSerialOutputForDebugging) {
        Serial.println("Clearing alarmCode.");
      }
      blinkLEDClear();
    }
    else if (useExtraFunctionKey && (key == extra_function_key || keypad.isPressed(extra_function_key))) {
      if (printSerialOutputForDebugging) {
        Serial.println("Submitting extra function event.");
      }
      client.publish((ESPMQTTTopic + "/extra_function").c_str(), "{\"extra_function\":\"called\"}");
    }
    else if ((allowZeroLengthArm || strlen(alarmCode) >= minimumAlarmCodeLength) && key == submit_key || keypad.isPressed(submit_key) || key == submit_key_alt) {
      if (printSerialOutputForDebugging) {
        Serial.print("Submitting Arm Alarm Code: ");
        Serial.println(String(alarmCode));
      }
      client.publish((ESPMQTTTopic + "/code_exit").c_str(), String(alarmCode));
      strcpy(alarmCode, "");
    }
    else if (strlen(alarmCode) > maximumAlarmCodeLength) {
      strcpy(alarmCode, "");
    }
    delay(100);
    if (!isIgnoredKey(key) && ledOnKeypress) {
      digitalWrite(LED_PIN, ledOFFValue);
      digitalWrite(LED_KEYPRESS_PIN, ledOFFValue);
    }
  }
}

void loop () {
  vTaskDelay(10 / portTICK_PERIOD_MS);
  server.handleClient();
  client.loop();
  publishLastwillOnline();

  if (waitForRetained && client.isConnected()) {
    if (retainStartTime == 0) {
      client.publish(ESPMQTTTopic.c_str(), "{\"status\":\"waiting\"}");
      retainStartTime = millis();
    }
    else if ((millis() - retainStartTime) > (waitForMQTTRetainMessages * 1000)) {
      waitForRetained = false;
    }
  }

  if (client.isConnected()) {
    if (digitalRead(0) == LOW) {
      blinkESP32LED();
    }

    check_keypad();
  }
}

void codeSuccess(std::string payload) {
  if (printSerialOutputForDebugging) {
    Serial.println("Processing codeSuccess MQTT event...");
  }
  StaticJsonDocument<100> docIn;
  deserializeJson(docIn, payload);

  if (docIn == nullptr) { //Check for errors in parsing
    if (printSerialOutputForDebugging) {
      Serial.println("JSON Parsing failed");
    }
    //char aBuffer[100];
    StaticJsonDocument<100> docOut;
    docOut["status"] = "errorParsingJSON";
    serializeJson(docOut, aBuffer);
    client.publish(ESPMQTTTopic.c_str(), aBuffer);
  }
  else {
    const bool successValid = docIn["success"];
    if (printSerialOutputForDebugging) {
      Serial.print("Success is: ");
      Serial.println(successValid);
      Serial.print("Full Event JSON: ");
      Serial.println(docIn.as<String>());
    }

    if (successValid == true) {
      StaticJsonDocument<100> docOut;
      docOut["status"] = "codeSuccess";
      serializeJson(docOut, aBuffer);
      if (printSerialOutputForDebugging) {
        Serial.println("Code was successful.");
      }
      client.publish(ESPMQTTTopic.c_str(), aBuffer);
      blinkColouredLEDSuccess();
    } else {
      StaticJsonDocument<100> docOut;
      docOut["status"] = "codeSuccessFail";
      serializeJson(docOut, aBuffer);
      if (printSerialOutputForDebugging) {
        Serial.println("Code was unsuccessful.");
      }
      client.publish(ESPMQTTTopic.c_str(), aBuffer);
      blinkLEDFail();
    }
  }
}

void onConnectionEstablished() {
  if (!MDNS.begin(host)) {
    Serial.println("Error starting mDNS");
  }
  server.close();
  server.begin();

  if (!deviceHasBooted) {
    deviceHasBooted = true;
    if (ledOnBootScan) {
      digitalWrite(LED_PIN, ledONValue);
    }
    client.publish(ESPMQTTTopic.c_str(), "{\"status\":\"boot\"}");
    delay(100);

    if (ledOnBootScan) {
      digitalWrite(LED_PIN, ledOFFValue);
      delay(10);
      blinkColouredLEDBootSequence();
    }
    if (printSerialOutputForDebugging) {
      Serial.println("Boot Ended");
    }
    client.publish(ESPMQTTTopic.c_str(), "{\"status\":\"idle\"}");
  }
  client.publish(lastWill, "online", true);

  client.subscribe((alarmLightStatusTopic + "/set").c_str(), [alarmLightStatusTopic] (const String & payload)  {
    if (printSerialOutputForDebugging) {
      Serial.println("Status Light Control MQTT Received...");
    }

    std::string deviceStateTopic = alarmLightStatusTopic + "/state";

    if ((strcmp(payload.c_str(), OFF_STR) == 0)) {
      digitalWrite(LED_STATUS_PIN, ledOFFValue);
      client.publish(deviceStateTopic.c_str(), OFF_STR, true);
    } else if ((strcmp(payload.c_str(), ON_STR) == 0)) {
      digitalWrite(LED_STATUS_PIN, ledONValue);
      client.publish(deviceStateTopic.c_str(), ON_STR, true);
    }
  });

  client.subscribe((alarmLightWarningTopic + "/set").c_str(), [alarmLightWarningTopic] (const String & payload)  {
    if (printSerialOutputForDebugging) {
      Serial.println("Status Light Control MQTT Received...");
    }

    std::string deviceStateTopic = alarmLightWarningTopic + "/state";

    if ((strcmp(payload.c_str(), OFF_STR) == 0)) {
      digitalWrite(LED_WARNING_PIN, ledOFFValue);
      client.publish(deviceStateTopic.c_str(), OFF_STR, true);
    } else if ((strcmp(payload.c_str(), ON_STR) == 0)) {
      digitalWrite(LED_WARNING_PIN, ledONValue);
      client.publish(deviceStateTopic.c_str(), ON_STR, true);
    }
  });

  client.subscribe(codeSuccessStdStr.c_str(), [] (const String & payload)  {
    if (printSerialOutputForDebugging) {
      Serial.println("Code Success MQTT Received...");
    }
    codeSuccess(payload.c_str());
  });

  if (home_assistant_mqtt_discovery) {
    publishHomeAssistantDiscoveryESPConfig();
  }
  
  if (!deviceFirstAdvertisementsSent) {
    deviceFirstAdvertisementsSent = true;
    publishHomeAssistantAdvertisements();
  }
}
