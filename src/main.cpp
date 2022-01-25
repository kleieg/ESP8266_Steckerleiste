
#include <Arduino.h>

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include <AsyncElegantOTA.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include "WLAN_Credentials.h"
#include "config.h"
#include "wifi_mqtt.h"


// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
long My_time = 0;


// Timers auxiliar variables
long now = millis();
char strtime[8];
int LEDblink = 0;
bool led = 1;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");


// Assign each GPIO to an output  --> see config.h
SetGPIO

// Assign relay details
String relayReset[NUM_OUTPUTS] = {"N", "N", "N", "Y", "Y", "Y", "Y", "Y"};
int relayResetStatus[NUM_OUTPUTS] = {0,0,0,0,0,0,0,0};
int relayResetTimer[NUM_OUTPUTS] = {0,0,0,0,0,0,0,0};

// end of definitions -----------------------------------------------------


// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin()) {
    Logyes Serial.println("An error has occurred while mounting LittleFS");
  }
  Logyes Serial.println("LittleFS mounted successfully");
}


String getOutputStates(){
  JSONVar myArray;

  myArray["cards"][0]["c_text"] = Hostname;
  myArray["cards"][1]["c_text"] = WiFi.dnsIP().toString() + "   /   " + String(VERSION);
  myArray["cards"][2]["c_text"] = String(WiFi.RSSI());
  myArray["cards"][3]["c_text"] = String(MQTT_INTERVAL) + "ms";
  myArray["cards"][4]["c_text"] = String(My_time);
  myArray["cards"][5]["c_text"] = "WiFi = " + String(WiFi_reconnect) + "   MQTT = " + String(Mqtt_reconnect);
  myArray["cards"][6]["c_text"] = String(RELAY_RESET_INTERVAL );
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

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    // according to AsyncWebServer documentation this is ok
    data[len] = 0;

    Logyes Serial.println("Data received: ");
    Logyes Serial.printf("%s\n", data);

    JSONVar json = JSON.parse((const char *)data);
    if (json == nullptr)
    {
      Logyes Serial.println("Request is not valid json, ignoring");
      return;
    }
    if (!json.hasOwnProperty("action"))
    {
      Logyes Serial.println("Request is not valid json, ignoring");
      return;
    }
    if (!strcmp(json["action"], "states"))
    {
      notifyClients(getOutputStates());
    }
    else if (!strcmp(json["action"], "reboot"))
    {
      Logyes Serial.println("Reset..");
      ESP.restart();
    }
    else if (!strcmp(json["action"], "relais"))
    {
      if (!json.hasOwnProperty("data"))
      {
        Logyes Serial.println("Relais request is missing data, ignoring");
        return;
      }
      if (!json["data"].hasOwnProperty("relais"))
      {
        Logyes Serial.println("Relais request is missing relais number, ignoring");
        return;
      }
      if (JSONVar::typeof_(json["data"]["relais"]) != "number")
      {
        Logyes Serial.println("Relais request contains invali relais number, ignoring");
        return;
      }
      int relais = json["data"]["relais"];
      if (relais < 0 || relais >= NUM_OUTPUTS)
      {
        Logyes Serial.println("Relais request contains invali relais number, ignoring");
        return;
      }
      
      digitalWrite(outputGPIOs[relais], !digitalRead(outputGPIOs[relais]));
      notifyClients(getOutputStates());
      Logyes Serial.println("switch Relais");

      if (relayReset[relais] == "Y")
      {
        relayResetStatus[relais] = 1;
      }
    }
  }

  Mqtt_lastSend = now - MQTT_INTERVAL - 10; // --> MQTT send !!
}


void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Logyes Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Logyes Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}


// receive MQTT messages
void MQTT_callback(char* topic, byte* message, unsigned int length) {
  
  Logyes Serial.printf("%s","Message arrived on topic: ");
  Logyes Serial.printf("%s\n",topic);
  Logyes Serial.printf("%s","Data : ");

  String MQTT_message;
  for (unsigned int i = 0; i < length; i++) {
    MQTT_message += (char)message[i];
  }
  Logyes Serial.println(MQTT_message);

  String relaisTopic = Hostname + "/CMD/Relais";
  String strTopic = String(topic);

  if (!strTopic.startsWith(relaisTopic) || strTopic.length() != relaisTopic.length() + 1)
  {
    Logyes Serial.printf("Invalid topic %d, %d", strTopic.length(), relaisTopic.length());
    return;
  }
  int relais = strTopic[strTopic.length() - 1] - '0';
  if(relais < 0 || relais >= NUM_OUTPUTS) {
    Logyes Serial.printf("Invalid relais %d", relais);
    return;
  }

  if (MQTT_message == "true")
  {
    digitalWrite(outputGPIOs[relais], LOW);
  }
  else if (MQTT_message == "false")
  {
    digitalWrite(outputGPIOs[relais], HIGH);
  }

  notifyClients(getOutputStates());

  Mqtt_lastSend  = now - MQTT_INTERVAL - 10; // --> MQTT send !!
}


void MQTTsend () {
 
  JSONVar mqtt_data, actuators;

  String mqtt_tag = Hostname + "/STATUS";
  Logyes Serial.printf("%s\n", mqtt_tag.c_str());

  char property[8];
  strcpy(property, "Relais0");

  for (size_t relais = 0; relais <= 6; relais++)
  {
    property[6] = '0' + relais;
    actuators[(const char*)property] = !digitalRead(outputGPIOs[relais]) ? true : false;
  }
  
  mqtt_data["Time"] = My_time;
  mqtt_data["RSSI"] = WiFi.RSSI();
  mqtt_data["Actuators"] = actuators;

  String mqtt_string = JSON.stringify(mqtt_data);

  Logyes Serial.printf("%s\n", mqtt_string.c_str());

  client.publish(mqtt_tag.c_str(), mqtt_string.c_str());

  notifyClients(getOutputStates());
}

void setup(){
  // Serial port for debugging purposes
  SERIALINIT
  delay (1000);                    // wait for serial log to be reday

  Logyes Serial.printf("init GPIOs\n");
  pinMode(GPIO_LED, OUTPUT);
  digitalWrite(GPIO_LED, LOW);

  // Set GPIOs as outputs. Set all relays to off when the program starts -  the relay is off when you set the relay to HIGH
  for (int i =0; i<NUM_OUTPUTS; i++){
    pinMode(outputGPIOs[i], OUTPUT);
    digitalWrite(outputGPIOs[i], HIGH);
  }

  initLittleFS();
  initWiFi();

  // init Websocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  Logyes Serial.printf("setup MQTT\n");
  client.setServer(MQTT_BROKER, 1883);
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

    if (now - LEDblink > LED_BLINK_INTERVAL) {
      LEDblink = now;
      if(led == 0) {
       digitalWrite(GPIO_LED, 1);
       led = 1;
      }else{
       digitalWrite(GPIO_LED, 0);
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
        if (now - relayResetTimer[i] > RELAY_RESET_INTERVAL  ){
          relayResetStatus[i] = 0;
          digitalWrite(outputGPIOs[i], HIGH);
          notifyClients(getOutputStates());
          Mqtt_lastSend  = now - MQTT_INTERVAL - 10;  // --> MQTT send !!
        }
      }
    }  

    // check WiFi
    if (WiFi.status() != WL_CONNECTED  ) {
      // try reconnect every 5 seconds
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;              // prevents mqtt reconnect running also
        // Attempt to reconnect
        Logyes Serial.printf("WiFi reconnect"); 
        reconnect_wifi();
      }
    }

  // check if MQTT broker is still connected
    if (!client.connected()) {
      // try reconnect every 5 seconds
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        // Attempt to reconnect
        Logyes Serial.printf("MQTT reconnect"); 
        reconnect_mqtt();
      }
    } else {
      // Client connected

      client.loop();

      // send data to MQTT broker
      if (now - Mqtt_lastSend  > MQTT_INTERVAL) {
      Mqtt_lastSend  = now;
      MQTTsend();
      } 
    }
}