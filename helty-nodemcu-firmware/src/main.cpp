////////////////////////////////////////////////////////////////////////
//                                                                    //
//    ModBus gateway to Helty VMC via NodeMCU and WIFI/MQTT support   //
//    Version: 2.0.0                                                  //
//                                                                    //
//    MQTT telemetry:                                                 //
//      vmcs/vmc_sala/state (speed status)    
//      vmcs/vmc_sala/fan_speed (fan speed)                          //
//      vmcs/vmc_sala/info  (json format)                             //
//                    IntTemperature (internal temperature)           //
//                    ExtTemperature (external temperature)           //
//                    Alarm (Alarm flag)                              //
//                                                                    //
//    MQTT Settings telemetry:                                        //
//      vmcs/vmc_sala/teleperiod (MQTT update period)                 //
//                                                                    //
//    MQTT Commands:                                                  //
//      vmcs/vmc_sala/cmnd/teleperiod (MQTT update period)            //
//      vmcs/vmc_sala/cmnd/speed (0 to 7, speed setpoint)             //
//      vmcs/vmc_sala/LWT (Last Will Testament)                       //
//                                                                    //

////////////////////////////////////////////////////////////////////////
  
// include libraries
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <stdio.h>
#include <stdlib.h>
#include <ArduinoJson.h>

// required for OTA updates
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>

// required for modbus communication
#include <ModbusRTU.h>
#include <SoftwareSerial.h>

// Include your secrets file
#include "secrets.h"

//
//     END OF CONFIGURATION SECTION
//


// RS485 setup with NodeMCU
#define RE_DE D2  // Connect RE&DE terminal to pin GPIO4
#define RX D6     // pin GPIO12
#define TX D1     // pin GPIO5

#define SLAVE_ID 2        // slave ID ( default is 2)
#define REG_COUNT 1       // registries count (does not support multiple readings)
#define SPEED_HREG 1000   // holding register 1000 - speed
#define INTTEMP_IREG 1000 // input register 1000 - internal temperature
#define EXTTEMP_IREG 1001 // input register 1001 - external temperature
#define ALARM_IREG 1006   // input register 1006 - alarm flags

// speeds settings
#define SPEED_ZERO 0x0000   // speed zero
#define SPEED_ONE 0x0001    // speed one
#define SPEED_TWO 0x0002    // speed two
#define SPEED_THREE 0x0003  // speed three
#define SPEED_FOUR 0x0004   // speed four
#define SPEED_HYPER 0x0005  // hyper speed
#define SPEED_NIGHT 0x0006  // night speed
#define SPEED_COOL 0x0007   // free cooling speed

// firmware version (publishes to vmcs/<device_id>/version on startup)
#define FW_VERSION "2.0.0"

// variables
uint16_t res;
uint16_t value;
String Msg = "";            // message for debug to MQTT
long period = 2000;         // period between two consecutive mqtt updates (msec)
unsigned long time_now = 0; // timestamp for mqtt refresh
int speed = 0;              // speed set
int speedO = 0;             // speed set old value
char buffer [10];
char output [256];

// MQTT topics
String LWT_TOPIC;
String CMD_TOPIC;
String CMD1_TOPIC;
String CMD2_TOPIC;
String TELE_TOPIC;
String TELE1_TOPIC;
String TELE2_TOPIC;
String FAN_SPEED_TOPIC;
String VERSION_TOPIC;

// Helper function to build MQTT topics
String buildMqttTopic(const char* suffix) {
  return String("vmcs/") + String(ESP_DEVICE_NAME) + String("/") + String(suffix);
}


// initialize libraries for modbus
SoftwareSerial S(RX, TX);
ModbusRTU mb;

// initialize web server for OTA
AsyncWebServer server(80);
// initialize wi-fi client
WiFiClient espClient;
// initialize mqtt client
PubSubClient client(espClient);
// prepare for json encoding
JsonDocument doc;



//
// callback function to monitor modbus communication errors
//
bool cb(Modbus::ResultCode event, uint16_t transactionId, void* data) { 
  if (event != Modbus::EX_SUCCESS) {
      Serial.printf_P("Request result: 0x%02X, Mem: %d\n", event, ESP.getFreeHeap());
  }
  return true;
}

//
// read holding register
// returns the register value
//
uint16_t rdHreg(uint16_t ADDRESS) {
  if (!mb.slave()) {    // Check if no transaction in progress
    digitalWrite(RE_DE, HIGH);
    mb.readHreg(SLAVE_ID, ADDRESS, &res, 1, cb); // Send Read Hreg from Modbus Server
    delayMicroseconds(120);
    digitalWrite(RE_DE,LOW);
    while(mb.slave()) { // Check if transaction is active
      mb.task();
      yield();
    }
    Serial.println(res);
  }
  return res;
}

//
// write holding register
//
void wtHreg(uint16_t ADDRESS, uint16_t VALUE) {
  if (!mb.slave()) {    // Check if no transaction in progress
    digitalWrite(RE_DE, HIGH);
    mb.writeHreg(SLAVE_ID, ADDRESS, VALUE, cb); // Send write Hreg to Modbus Server
    delayMicroseconds(120);
    digitalWrite(RE_DE,LOW);
    while(mb.slave()) { // Check if transaction is active
      mb.task();
      yield();
    }
  }
}

//
// read input register
// returns the register value
//
uint16_t rdIreg(uint16_t ADDRESS) {
  if (!mb.slave()) {    // Check if no transaction in progress
    digitalWrite(RE_DE, HIGH);
    mb.readIreg(SLAVE_ID, ADDRESS, &res, 1, cb); // Send Read Ireg from Modbus Server
    delayMicroseconds(120);
    digitalWrite(RE_DE,LOW);
    while(mb.slave()) { // Check if transaction is active
      mb.task();
      yield();
    }
    Serial.println(res);
  }
  return res;
}

//
// Helper function to map internal speed (0-7) to percentage (0-100)
//
int mapStateToFanSpeed(int value) {
  if (value == 0) {
    return 0;
  } else if (value == 1) {
    return 1;
  } else if (value == 2) {
    return 2;
  } else if (value == 3) {
    return 3;
  } else if (value == 4) {
    return 4;
  } else if (value == 5) { // Hyper Speed
    return 4;
  } else if (value == 6) { // Night Mode
    return 1;
  } else if (value == 7) { // Free Cooling
    return 4;
  } else {
    return 0; // Default for unexpected values
  }
}

//
// MQTT connection function
//
void mqtt_connect() {
  // Connect to MQTT broker
  if (client.connect(ESP_DEVICE_NAME, mqttUser, mqttPassword, LWT_TOPIC.c_str(), 1, true, "Offline")) {
    // Connection to MQTT successful
    Serial.println("Connected!");
    // Subscribe to settings topics with QoS 1 for reliable delivery
    client.subscribe(CMD_TOPIC.c_str(), 1);    // subscribe to all command topics
    client.publish(TELE1_TOPIC.c_str(), itoa((int)period, buffer, 10), true); // teleperiod
    client.publish(LWT_TOPIC.c_str(), "Online", true);  // last will testament
    client.publish(VERSION_TOPIC.c_str(), FW_VERSION, true); // firmware version

  }
}

//
// WIFI connection function
//
void wifi_connect() {
  // Connect to WIFI
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi..");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);

  }
}

//
// Callback function for MQTT subscriptions
//
void mqtt_callback(char* topic, byte* payload, unsigned int length) {

  String message = (char*)payload;
  String thetopic = (char*)topic;
  message = message.substring(0, length);
  char buffer [5];

  // prepare MQTT message for debug
  Msg = "Message arrived in topic: " + thetopic + " -> " + message;
  Serial.println(Msg);

  // decode subscribed messages and return confirmation
  // telegram period
  if (thetopic == CMD1_TOPIC) {
    Serial.print("TelePeriod=");
    period = message.toInt();
    Serial.println(message);
    client.publish(TELE1_TOPIC.c_str(), itoa(period, buffer, 10), true);
  }

  // speed
  if (thetopic == CMD2_TOPIC) {
    Serial.print("Speed=");
    speed = message.toInt();
    Serial.println(message);
  }

  Serial.println();
  Serial.println("-----------------------");

}


//
// setup routine
//
void setup() {
  pinMode(RE_DE, OUTPUT);  // direction pin
  Serial.begin(115200);   // start serial port
  S.begin(19200, SWSERIAL_8N1); // setup software serial
  mb.begin(&S, RE_DE); // start software serial for Modbus with RE_DE pin
  mb.master();  // start Master modbus processing

  // Initialize dynamic MQTT topics
  LWT_TOPIC = buildMqttTopic("LWT");
  CMD_TOPIC = buildMqttTopic("cmnd/#");
  CMD1_TOPIC = buildMqttTopic("cmnd/teleperiod");
  CMD2_TOPIC = buildMqttTopic("cmnd/speed");
  TELE_TOPIC = buildMqttTopic("state");
  FAN_SPEED_TOPIC = buildMqttTopic("fan_speed");
  TELE1_TOPIC = buildMqttTopic("teleperiod");
  TELE2_TOPIC = buildMqttTopic("info");
  VERSION_TOPIC = buildMqttTopic("version");

  // Connnect to local wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for wifi connection
  Serial.println("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // activate http server for OTA updates
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String msg = "Hi! I am ";
    msg += ESP_DEVICE_NAME;
    msg += ".";
    request->send(200, "text/plain", msg);
  });

  // Start ElegantOTA
  ElegantOTA.begin(&server);
  server.begin();
  Serial.println("HTTP server started");

  // Set MQTT broker
  client.setServer(mqttServer, mqttPort);

  // Set the callback function for MQTT subscriptions
  client.setCallback(mqtt_callback);

  // Connect to MQTT broker
  Serial.println("Connecting to MQTT...");
  while (!client.connected()) {
    Serial.print(".");

    // Attempt connection to MQTT
    if (client.connect(ESP_DEVICE_NAME, mqttUser, mqttPassword, LWT_TOPIC.c_str(), 1, true, "Offline")) {
      // Connection to MQTT successful
      Serial.println("Connected!");
      // Subscribe to settings topics with QoS 1 for reliable delivery
      client.subscribe(CMD_TOPIC.c_str(), 1);    // subscribe to all command topics
      client.publish(TELE1_TOPIC.c_str(), itoa((int)period, buffer, 10), true); // teleperiod
      client.publish(LWT_TOPIC.c_str(), "Online", true); // last will testament
      client.publish(VERSION_TOPIC.c_str(), FW_VERSION, true); // firmware version
    }
  }

  value = rdHreg(SPEED_HREG);
  client.publish(CMD2_TOPIC.c_str(), itoa(value, buffer, 10)); // update speed command
  client.publish(FAN_SPEED_TOPIC.c_str(), itoa(mapStateToFanSpeed(value), buffer, 10));
}


//
// main loop
//
void loop() {

  // manage millis reset
  if ((millis() - time_now) < 0) {
    time_now = millis();
  }

  //Update MQTT every period (msec)
  if ((millis() - time_now) >= period){

    // check MQTT connection and reconnect
    if (!client.connected()) {
      mqtt_connect();
    }

    value = rdHreg(SPEED_HREG);
    client.publish(TELE_TOPIC.c_str(), itoa(value, buffer, 10)); // update speed topic
    client.publish(FAN_SPEED_TOPIC.c_str(), itoa(mapStateToFanSpeed(value), buffer, 10));

    // Add values in the document
    //
    value = rdIreg(INTTEMP_IREG);  // internal temp x 0.1C
    doc["IntTemperature"] = (float)value*0.1;

    value = rdIreg(EXTTEMP_IREG);  // external temp x 0.1C
    doc["ExtTemperature"] = (float)value*0.1;

    value = rdIreg(ALARM_IREG);    // alarms
    doc["Alarm"] = value;


    serializeJson(doc, output);
    client.publish(TELE2_TOPIC.c_str(), output); // update alarm topic

    // set next update
    time_now += period;

  }

  // set speed of vmc
  if (speed != speedO) {
    switch (speed) {
      case 0:
        wtHreg(SPEED_HREG, SPEED_ZERO);
        break;
      case 1:
        wtHreg(SPEED_HREG, SPEED_ONE);
        break;
      case 2:
        wtHreg(SPEED_HREG, SPEED_TWO);
        break;
      case 3:
        wtHreg(SPEED_HREG, SPEED_THREE);
        break;
      case 4:
        wtHreg(SPEED_HREG, SPEED_FOUR);
        break;
      case 5:
        wtHreg(SPEED_HREG, SPEED_HYPER);
        break;
      case 6:
        wtHreg(SPEED_HREG, SPEED_NIGHT);
        break;
      case 7:
        wtHreg(SPEED_HREG, SPEED_COOL);
        break;
    }

    client.publish(TELE_TOPIC.c_str(), itoa(speed, buffer, 10), true);
    speedO = speed;
  }


  // MQTT client loop
  client.loop();
  // elegant ota loop
  ElegantOTA.loop();

}
