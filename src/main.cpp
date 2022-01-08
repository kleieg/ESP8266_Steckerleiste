// ---------------------->>>>>>>>
// Serial IO kann nicht genutzt werden!!!
// GPIO01=Tx und GPIO03=Rx werden für Realis benutzt!!!
//
// based on https://randomnerdtutorials.com/esp8266-nodemcu-ota-over-the-air-vs-code/
//


// Import required libraries
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include <AsyncElegantOTA.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>

#include "WLAN_Credentials.h"

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<   Anpassungen !!!!
// set hostname used for MQTT tag and WiFi 
#define HOSTNAME "Steckerleiste"
#define VERSION "v 2.0.0"


// variables to connects to  MQTT broker
const char* mqtt_server = "192.168.178.15";
const char* willTopic = "tele/Steckerleiste/LWT";       // muss mit HOSTNAME passen !!!  tele/HOSTNAME/LWT    !!!

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<   Anpassungen Ende !!!!

int WiFi_reconnect = 0;

// for MQTT
byte willQoS = 0;
const char* willMessage = "Offline";
boolean willRetain = true;
std::string mqtt_tag;
int Mqtt_sendInterval = 120000;   // in milliseconds = 2 minutes
long Mqtt_lastScan = 0;
long lastReconnectAttempt = 0;
int Mqtt_reconnect = 0;

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
long My_time = 0;

// Initializes the espClient. 
WiFiClient myClient;
PubSubClient client(myClient);
// name used as Mqtt tag
std::string gateway = HOSTNAME ;  

// Timers auxiliar variables
long now = millis();
char strtime[8];
int LEDblink = 0;
bool led = 1;
int gpioLed = 2;
int LedBlinkTime = 500;
int RelayResetTime = 5000;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

// Set number of outputs
#define NUM_OUTPUTS  8

// Assign each GPIO to an output
int outputGPIOs[NUM_OUTPUTS] =  {16, 5, 4, 14, 3, 1, 12, 13};
//int outputGPIOs[NUM_OUTPUTS] =  {16, 5, 4, 14, 12, 12, 12, 13};  // if Serial log enabled!!
// Assign relay details
String relayReset[NUM_OUTPUTS] = {"N", "N", "N", "Y", "Y", "Y", "Y", "Y"};
int relayResetStatus[NUM_OUTPUTS] = {0,0,0,0,0,0,0,0};
int relayResetTimer[NUM_OUTPUTS] = {0,0,0,0,0,0,0,0};

// end of definitions -----------------------------------------------------


// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin()) {
    // Serial.println("An error has occurred while mounting LittleFS");
  }
  // Serial.println("LittleFS mounted successfully");
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  WiFi.begin(ssid, password);
  // Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    // Serial.print('.');
    delay(1000);
  }
  // Serial.println(WiFi.localIP());
}

String getOutputStates(){
  JSONVar myArray;

  myArray["cards"][0]["c_text"] = String(HOSTNAME) + "   /   " + String(VERSION);
  myArray["cards"][1]["c_text"] = willTopic;
  myArray["cards"][2]["c_text"] = String(WiFi.RSSI());
  myArray["cards"][3]["c_text"] = String(Mqtt_sendInterval) + "ms";
  myArray["cards"][4]["c_text"] = String(My_time);
  myArray["cards"][5]["c_text"] = "WiFi = " + String(WiFi_reconnect) + "   MQTT = " + String(Mqtt_reconnect);
  myArray["cards"][6]["c_text"] = String(RelayResetTime);
  myArray["cards"][7]["c_text"] = " to reboot click ok";

  for (int i =0; i<NUM_OUTPUTS; i++){
    myArray["gpios"][i]["output"] = String(i);

    if (relayReset[i] == "Y") {
      myArray["gpios"][i]["state"] = String(digitalRead(outputGPIOs[i]));
    }
      else {
      myArray["gpios"][i]["state"] = String(!digitalRead(outputGPIOs[i]));
    }
  }
  String jsonString = JSON.stringify(myArray);
  return jsonString;
}

void notifyClients(String state) {
  ws.textAll(state);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    
    data[len] = 0;
    char help[30];
    
    for (int i = 0; i <= len; i++){
      help[i] = data[i];
    }

    // Serial.println("Data received: ");
    // Serial.printf("%s\n",help);

    if (strcmp((char*)data, "states") == 0) {
      notifyClients(getOutputStates());
    }
    else{
      if (strcmp((char*)data, "Reboot") == 0) {
        // Serial.println("Reset..");
        ESP.restart();
      }
      else {
        int gpio = atoi((char*)data);
        digitalWrite(outputGPIOs[gpio], !digitalRead(outputGPIOs[gpio]));
        notifyClients(getOutputStates());
        // Serial.println("switch Relais");

        if (relayReset[gpio] == "Y") {
          relayResetStatus[gpio] = 1;
        }
      }
    }
  }

  Mqtt_lastScan = now - Mqtt_sendInterval - 10;  // --> MQTT send !!
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      // Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      // Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

// reconnect to WiFi 
void reconnect_wifi() {
  // Serial.printf("%s\n","WiFi try reconnect"); 
  WiFi.begin();
  delay(500);
  if (WiFi.status() == WL_CONNECTED) {
    WiFi_reconnect = WiFi_reconnect + 1;
    // Once connected, publish an announcement...
    // Serial.printf("%s\n","WiFi reconnected"); 
  }
}

// This functions reconnects your ESP32 to your MQTT broker

void reconnect_mqtt() {
  if (client.connect(gateway.c_str(), willTopic, willQoS, willRetain, willMessage)) {
    // Once connected, publish an announcement...
    // Serial.printf("%s\n","Mqtt connected"); 
    mqtt_tag = gateway + "/connect";
    client.publish(mqtt_tag.c_str(),"connected");
    // Serial.printf("%s",mqtt_tag.c_str());
    // Serial.printf("%s\n","connected");
    mqtt_tag = "tele/" + gateway  + "/LWT";
    client.publish(mqtt_tag.c_str(),"Online",willRetain);
    // Serial.printf("%s",mqtt_tag.c_str());
    // Serial.printf("%s\n","Online");

    mqtt_tag = "cmnd/" + gateway + "/#";
    client.subscribe(mqtt_tag.c_str());

    Mqtt_reconnect = Mqtt_reconnect + 1;
  }
}

// receive MQTT messages
void MQTT_callback(char* topic, byte* message, unsigned int length) {
  
  // Serial.printf("%s","Message arrived on topic: ");
  // Serial.printf("%s\n",topic);
  // Serial.printf("%s","Data : ");

  String MQTT_message;
  for (int i = 0; i < length; i++) {
    MQTT_message += (char)message[i];
  }
  // Serial.println(MQTT_message);

  String Topic_Relais1 = String("cmnd/"); 
  Topic_Relais1 = String(Topic_Relais1 + gateway.c_str() + "/Relais1");
  String Topic_Relais2 = String("cmnd/"); 
  Topic_Relais2 = String(Topic_Relais2 + gateway.c_str() + "/Relais2");
  String Topic_Relais3 = String("cmnd/"); 
  Topic_Relais3 = String(Topic_Relais3 + gateway.c_str() + "/Relais3");

  if (String(topic) == Topic_Relais1 ){
    if(MQTT_message == "on"){
      digitalWrite(outputGPIOs[0], LOW);
    }
    else if(MQTT_message == "off"){
      digitalWrite(outputGPIOs[0], HIGH);
    }
  }

    if (String(topic) == Topic_Relais2 ){
    if(MQTT_message == "on"){
      digitalWrite(outputGPIOs[1], LOW);
    }
    else if(MQTT_message == "off"){
      digitalWrite(outputGPIOs[1], HIGH);
    }
  }

    if (String(topic) == Topic_Relais3 ){
    if(MQTT_message == "on"){
      digitalWrite(outputGPIOs[2], LOW);
    }
    else if(MQTT_message == "off"){
      digitalWrite(outputGPIOs[2], HIGH);
    }
  }

  notifyClients(getOutputStates());

}

void MQTTsend () {
  JSONVar mqtt_data; 
  
  mqtt_tag = "tele/" + gateway + "/SENSOR";
  // Serial.printf("%s\n",mqtt_tag.c_str());

  mqtt_data["Time"] = My_time;
  mqtt_data["RSSI"] = WiFi.RSSI();

  mqtt_data["Relais1"] = digitalRead(outputGPIOs[0]);
  mqtt_data["Relais2"] = digitalRead(outputGPIOs[1]);
  mqtt_data["Relais3"] = digitalRead(outputGPIOs[2]);
  mqtt_data["Relais4"] = digitalRead(outputGPIOs[3]);
  mqtt_data["Relais5"] = digitalRead(outputGPIOs[4]);
  mqtt_data["Relais6"] = digitalRead(outputGPIOs[5]);
  mqtt_data["Relais7"] = digitalRead(outputGPIOs[6]);
  mqtt_data["Relais8"] = digitalRead(outputGPIOs[7]);


  String mqtt_string = JSON.stringify(mqtt_data);

  // Serial.printf("%s\n",mqtt_string.c_str()); 

  client.publish(mqtt_tag.c_str(), mqtt_string.c_str());

  notifyClients(getOutputStates());
}

void setup(){
  // Serial port for debugging purposes
  //Serial.begin(115200);
  delay (1000);                    // wait for serial log to be reday

  // Serial.printf("init GPIOs\n");
  pinMode(gpioLed, OUTPUT);
  digitalWrite(gpioLed,led);

  // Set GPIOs as outputs. Set all relays to off when the program starts -  the relay is off when you set the relay to HIGH
  for (int i =0; i<NUM_OUTPUTS; i++){
    pinMode(outputGPIOs[i], OUTPUT);
    digitalWrite(outputGPIOs[i], HIGH);
  }

  initLittleFS();
  initWiFi();
  initWebSocket();

  // Serial.printf("setup MQTT\n");
  client.setServer(mqtt_server, 1883);
  client.setCallback(MQTT_callback);

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html",false);
  });

  server.serveStatic("/", LittleFS, "/");

  timeClient.begin();
  timeClient.setTimeOffset(0);

  // Start ElegantOTA
  AsyncElegantOTA.begin(&server);
  
  // Start server
  server.begin();
}

void loop() {
    AsyncElegantOTA.loop();
    ws.cleanupClients();

  // update UPCtime
    timeClient.update();
    My_time = timeClient.getEpochTime();

  // LED blinken
    now = millis();

    if (now - LEDblink > LedBlinkTime) {
      LEDblink = now;
      if(led == 0) {
       digitalWrite(gpioLed, 1);
       led = 1;
      }else{
       digitalWrite(gpioLed, 0);
       led = 0;
      }
    }
  // auf Reset prüfen
  // falls nötig Timer setzten
    for(int i=0; i<NUM_OUTPUTS; i++){
      if (relayResetStatus[i] == 1) {
        relayResetStatus[i] = 2;
        relayResetTimer[i] = now;
      }
    }

  // prüfen ob Timer abgelaufen; wenn ja Relais ausschalten
    for(int i=0; i<NUM_OUTPUTS; i++){
      if (relayResetStatus[i] == 2 ){
        if (now - relayResetTimer[i] > RelayResetTime ){
          relayResetStatus[i] = 0;
          digitalWrite(outputGPIOs[i], HIGH);
          notifyClients(getOutputStates());
          Mqtt_lastScan = now - Mqtt_sendInterval - 10;  // --> MQTT send !!
        }
      }
    }  

    // check WiFi
    if (WiFi.status() != WL_CONNECTED  ) {
      // try reconnect every 5 seconds
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;              // prevents mqtt reconnect running also
        // Attempt to reconnect
        // Serial.printf("WiFi reconnect"); 
        reconnect_wifi();
      }
    }

  // check if MQTT broker is still connected
    if (!client.connected()) {
      // try reconnect every 5 seconds
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        // Attempt to reconnect
        // Serial.printf("MQTT reconnect"); 
        reconnect_mqtt();
      }
    } else {
      // Client connected

      client.loop();

      // send data to MQTT broker
      if (now - Mqtt_lastScan > Mqtt_sendInterval) {
      Mqtt_lastScan = now;
      MQTTsend();
      } 
    }
}