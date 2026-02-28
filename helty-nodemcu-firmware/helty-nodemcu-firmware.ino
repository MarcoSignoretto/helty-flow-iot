////////////////////////////////////////////////////////////////////////
//                                                                    //
//    ModBus gateway to Helty VMC via NodeMCU and WIFI/MQTT support   //
//                                                                    //
//    MQTT telemetry:                                                 //
//      vmcs/vmc_sala/state (speed status)                            //
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
//    Version History:                                                //
//      1.0 - Initial Tests                                           //
//      2.0 - First working build                                     //
//      3.0 - Moved info to json                                      //
////////////////////////////////////////////////////////////////////////
  
// include libraries
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <stdio.h>
#include <stdlib.h>
#include <ArduinoJson.h>

// required for OTA updates
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

// required for modbus communication
#include <ModbusRTU.h>
#include <SoftwareSerial.h>

//
//        CONFIGURATION SECTION
// wi-fi and mqtt connection parameters
//
const char* ssid = "<YOUR WIFI SSID>";        // Enter your WiFi name
const char* password =  "<YOUR WIFI PWD>";    // Enter WiFi password
const char* mqttServer = "<MQTT IP>"; 		// MQTT host IP
const int mqttPort = <MQTT PORT>;             // MQTT TCP port
const char* mqttUser = "<MQTT USER>";         // MQTT username
const char* mqttPassword = "<MQTT PWD>";     	// MQTT password
//
//     END OF CONFIGURATION SECTION
//


// RS485 setup with NodeMCU
#define RE_DE D2  // Connect RE&DE terminal to pin GPIO4
#define RX D6     // pin GPIO12
#define TX D7     // pin GPIO13

#define SLAVE_ID 2        // slave ID
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
const char* TOPIC_LWT =  "vmcs/vmc_letto/LWT";             // last will testament
const char* TOPIC_CMD =  "vmcs/vmc_letto/cmnd/#";          // all commands
const char* TOPIC_CMD1 = "vmcs/vmc_letto/cmnd/teleperiod"; // setup TelePeriod command
const char* TOPIC_CMD2 = "vmcs/vmc_letto/cmnd/speed";      // set speed command

// answers to command
const char* TOPIC_TELE =  "vmcs/vmc_letto/state";          // speed status
const char* TOPIC_TELE1 = "vmcs/vmc_letto/teleperiod";     // MQTT update period
const char* TOPIC_TELE2 = "vmcs/vmc_letto/info";           // info


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
StaticJsonDocument<200> doc;



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
// MQTT connection function
//
void mqtt_connect() {
  // Connect to MQTT broker
  if (client.connect("VMC_Letto", mqttUser, mqttPassword,TOPIC_LWT,1,true,"Offline" )) {
    // Connection to MQTT successful
    Serial.println("Connected!");  
    // Subscribe to settings topics
    client.subscribe(TOPIC_CMD);    // subscribe to all command topics
    client.publish(TOPIC_TELE1, itoa((int)period,buffer,10), true); // teleperiod
    client.publish(TOPIC_LWT, "Online");  // last will testament

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
  message = message.substring(0,length);
  char buffer [5];

  // prepare MQTT message for debug
  Msg = "Message arrived in topic: " + thetopic + " -> " + message;
  Serial.println(Msg);
  
  // decode subscribed messages and return confirmation
  // telegram period
  if (thetopic == TOPIC_CMD1) {
    Serial.print("TelePeriod=");
    period = message.toInt();
    Serial.println(message);
    client.publish(TOPIC_TELE1, itoa(period,buffer,10), true);
  }

  // speed
  if (thetopic == TOPIC_CMD2) {
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
  pinMode(RE_DE,OUTPUT);  // direction pin
  Serial.begin(115200);   // start serial port
  S.begin(19200, SWSERIAL_8N1); // setup software serial
  mb.begin(&S); // start software serial
  mb.master();  // start Master modbus processing

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
    request->send(200, "text/plain", "Hi! I am VMC Letto.");
  });

  // Start ElegantOTA
  AsyncElegantOTA.begin(&server);
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
    if (client.connect("VMC_Letto", mqttUser, mqttPassword,TOPIC_LWT,1,true,"Offline" )) {
      // Connection to MQTT successful
      Serial.println("Connected!");  
      // Subscribe to settings topics
      client.subscribe(TOPIC_CMD);    // subscribe to all command topics
      client.publish(TOPIC_TELE1, itoa((int)period,buffer,10), true); // teleperiod
      client.publish(TOPIC_LWT, "Online"); // last will testament

    }
  }

  value = rdHreg(SPEED_HREG);
  client.publish(TOPIC_CMD2, itoa(value,buffer,10)); // update speed command

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
    client.publish(TOPIC_TELE, itoa(value,buffer,10)); // update speed topic

    // Add values in the document
    //
    value = rdIreg(INTTEMP_IREG);  // internal temp x 0.1C
    doc["IntTemperature"] = (float)value*0.1;

    value = rdIreg(EXTTEMP_IREG);  // external temp x 0.1C
    doc["ExtTemperature"] = (float)value*0.1;

    value = rdIreg(ALARM_IREG);    // alarms
    doc["Alarm"] = value;
    
    
    serializeJson(doc, output);
    client.publish(TOPIC_TELE2, output); // update alarm topic

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
    
    client.publish(TOPIC_TELE, itoa(speed,buffer,10), true);
    speedO = speed;
  }


  // MQTT client loop
  client.loop();

}
