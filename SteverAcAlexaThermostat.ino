/*
  Version 0.3 - March 06 2018
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsClient.h> //  https://github.com/kakopappa/sinric/wiki/How-to-add-dependency-libraries
#include <ArduinoJson.h> // https://github.com/kakopappa/sinric/wiki/How-to-add-dependency-libraries
#include <StreamString.h>

// temp sensor stuff
#include "DHT.h"        // including the library of DHT11 temperature and humidity sensor
#define DHTTYPE DHT11   // DHT 11
#define dht_dpin 0
DHT dht(dht_dpin, DHTTYPE);

//*********************
int targetTemp = 23;
bool relayState = false;
float tempBuffer = 0.0;
//********************

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

#define MyApiKey "892a0a56-d860-4f7a-a391-95e08252e044" // TODO: Change to your sinric API Key. Your API Key is displayed on sinric.com dashboard
#define MySSID "CMU-DEVICE" // TODO: Change to your Wifi network SSID
#define MyWifiPassword "" // TODO: Change to your Wifi network password

#define HEARTBEAT_INTERVAL 300000 // 5 Minutes 

uint64_t heartbeatTimestamp = 0;
bool isConnected = false;

void setPowerStateOnServer(String deviceId, String value);
void setSetTemperatureSettingOnServer(String deviceId, float setPoint, String scale, float ambientTemperature, float ambientHumidity);
void setThermostatModeOnServer(String deviceId, String thermostatMode);

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      isConnected = false;
      Serial.printf("[WSc] Webservice disconnected from sinric.com!\n");
      break;
    case WStype_CONNECTED: {
        isConnected = true;
        Serial.printf("[WSc] Service connected to sinric.com at url: %s\n", payload);
        Serial.printf("Waiting for commands from sinric.com ...\n");
      }
      break;
    case WStype_TEXT: {
        Serial.printf("[WSc] get text: %s\n", payload);
        // Example payloads

        // For Thermostat
        // {"deviceId": xxxx, "action": "setPowerState", value: "ON"} // https://developer.amazon.com/docs/device-apis/alexa-thermostatcontroller.html
        // {"deviceId": xxxx, "action": "SetTargetTemperature", value: "targetSetpoint": { "value": 20.0, "scale": "CELSIUS"}} // https://developer.amazon.com/docs/device-apis/alexa-thermostatcontroller.html#settargettemperature
        // {"deviceId": xxxx, "action": "AdjustTargetTemperature", value: "targetSetpointDelta": { "value": 2.0, "scale": "FAHRENHEIT" }} // https://developer.amazon.com/docs/device-apis/alexa-thermostatcontroller.html#adjusttargettemperature
        // {"deviceId": xxxx, "action": "SetThermostatMode", value: "thermostatMode" : { "value": "COOL" }} // https://developer.amazon.com/docs/device-apis/alexa-thermostatcontroller.html#setthermostatmode

        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject((char*)payload);
        String deviceId = json ["deviceId"];
        Serial.print("DEVICE ID:::::::");
        Serial.println(deviceId);
        String action = json ["action"];

        /*if (deviceId == "5axxxxxxxxxxxxxxxxxxx") // Device ID of first device
          {
          // Check device id if you have multiple devices.
          } */

        if (action == "setPowerState") { // On or Off
          String value = json ["value"];
          Serial.println("[WSc] setPowerState" + value);
        }
        else if (action == "SetTargetTemperature") {
          // Alexa, set thermostat to 20
          //String value = json ["value"];
          String value = json["value"]["targetSetpoint"]["value"];
          targetTemp = value.toInt();
          String scale = json["value"]["targetSetpoint"]["scale"];

          Serial.println("[WSc] SetTargetTemperature value: " + value);
          Serial.println("[WSc] SetTargetTemperature scale: " + scale);
        }
        else if (action == "AdjustTargetTemperature") {
          //Alexa, make it warmer in here
          //Alexa, make it cooler in here
          String value = json["value"]["targetSetpointDelta"]["value"];
          String scale = json["value"]["targetSetpointDelta"]["scale"];

          Serial.println("[WSc] AdjustTargetTemperature value: " + value);
          Serial.println("[WSc] AdjustTargetTemperature scale: " + scale);
        }
        else if (action == "SetThermostatMode") {
          //Alexa, set thermostat name to mode
          //Alexa, set thermostat to automatic
          //Alexa, set kitchen to off
          String value = json["value"]["thermostatMode"]["value"];

          Serial.println("[WSc] SetThermostatMode value: " + value);
        }
        else if (action == "test") {
          Serial.println("[WSc] received test command from sinric.com");
        }
      }

      break;
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(13,OUTPUT);

  WiFiMulti.addAP(MySSID);
  Serial.println();
  Serial.print("Connecting to Wifi: ");
  Serial.println(MySSID);

  // Waiting for Wifi connect
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  if (WiFiMulti.run() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("WiFi connected. ");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  // server address, port and URL
  webSocket.begin("iot.sinric.com", 80, "/");

  // event handler
  webSocket.onEvent(webSocketEvent);
  webSocket.setAuthorization("apikey", MyApiKey);

  // try again every 5000ms if connection has failed
  webSocket.setReconnectInterval(5000);   // If you see 'class WebSocketsClient' has no member named 'setReconnectInterval' error update arduinoWebSockets

  //init the temp sensor
  dht.begin();
}

void adjustTemp(){
  float currTemp = readTempature();
  if(currTemp < (targetTemp - tempBuffer)){
    relayState = false;
    Serial.println("CURRENT TEMP IS LOWER");
  }
  if(currTemp > (targetTemp + tempBuffer)){
    relayState = true;
    Serial.println("CURRENT TEMP IS higher");
  }

  if (relayState == true) digitalWrite(13,HIGH);
  if (relayState == false) digitalWrite(13,LOW);
}


void loop() {
  webSocket.loop();
  Serial.print("target temp is: ");
  Serial.println(targetTemp);
  adjustTemp();
  //readTempature();
  //float currentTemp = getCurrentTemp();
//  Serial.print("temperature = ");
//  Serial.print(currentTemp);
//  Serial.println("F ");
  

  if (isConnected) {
    uint64_t now = millis();

    // Send heartbeat in order to avoid disconnections during ISP resetting IPs over night. Thanks @MacSass
    if ((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL) {
      heartbeatTimestamp = now;
      webSocket.sendTXT("H");
    }
  }
}


float readTempature() { 
  //delay(dht.getMinimumSamplingPeriod());

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  float temperaturefh = dht.readTemperature(true);
  
  Serial.print(humidity, 1);
  Serial.print("\t\t");
  Serial.print(temperature, 1);
  Serial.print("\t\t");
  Serial.print(temperaturefh, 1);
  Serial.print("\t\t");
  Serial.print(dht.computeHeatIndex(temperature, humidity, false), 1);
  Serial.println("\t\t");
    setSetTemperatureSettingOnServer("5de60ac70d788452d70b7141", (targetTemp*1.8 +32), "FAHRENHEIT", temperaturefh, humidity);  //Convert target temp to f.*******
  //setSetTemperatureSettingOnServer("5de60ac70d788452d70b7141", temperaturefh, "FAHRENHEIT", temperaturefh, humidity);  
  delay(2000);
  return temperature;
}


// If you are going to use a push button to on/off the switch manually, use this function to update the status on the server
// so it will reflect on Alexa app.
// eg: setPowerStateOnServer("deviceid", "ON")

// Call ONLY If status changed. DO NOT CALL THIS IN loop() and overload the server.

void setPowerStateOnServer(String deviceId, String value) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["deviceId"] = deviceId;
  root["action"] = "setPowerState";
  root["value"] = value;
  StreamString databuf;
  root.printTo(databuf);

  webSocket.sendTXT(databuf);
}

// Call ONLY If status changed. DO NOT CALL THIS IN loop() and overload the server.

//eg: setSetTemperatureSettingOnServer("deviceid", 25.0, "CELSIUS" or "FAHRENHEIT", 23.0, 45.3)
// setPoint: Indicates the target temperature to set on the termostat.
void setSetTemperatureSettingOnServer(String deviceId, float setPoint, String scale, float ambientTemperature, float ambientHumidity) {
//  Serial.print("\t\t");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["action"] = "SetTemperatureSetting";
  root["deviceId"] = deviceId;

  JsonObject& valueObj = root.createNestedObject("value");
  JsonObject& temperatureSetting = valueObj.createNestedObject("temperatureSetting");
  temperatureSetting["setPoint"] = setPoint;
  temperatureSetting["scale"] = scale;
  temperatureSetting["ambientTemperature"] = ambientTemperature;
  temperatureSetting["ambientHumidity"] = ambientHumidity;

  StreamString databuf;
  root.printTo(databuf);

  webSocket.sendTXT(databuf);
}
// Call ONLY If status changed. DO NOT CALL THIS IN loop() and overload the server.

void setThermostatModeOnServer(String deviceId, String thermostatMode) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["deviceId"] = deviceId;
  root["action"] = "SetThermostatMode";
  root["value"] = thermostatMode;
  StreamString databuf;
  root.printTo(databuf);

  webSocket.sendTXT(databuf);
}
