#include <MQTT.h>
#include <ESP8266WiFi.h>

// will be computed as "<HOSTNAME>_<MAC-ADDRESS>"
String Hostname;

int WiFi_reconnect = 0;

// for MQTT
long Mqtt_lastSend = 0;
long lastReconnectAttempt = 0;
int Mqtt_reconnect = -1;


// Initializes the espClient. 
WiFiClient ethClient;
MQTTClient mqttClient(256);

// Initialize WiFi
void initWiFi() {
    // dynamically determine hostname
  Hostname = HOSTNAME;
  Hostname += "_";
  Hostname += WiFi.macAddress();
  Hostname.replace(":", "");

  WiFi.hostname(Hostname);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  LogPrintln("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    LogPrintf(".");
    delay(1000);
  }
  LogPrintln(WiFi.localIP());
}

// reconnect to WiFi 
void reconnect_wifi() {
  LogPrintf2("%s\n","WiFi try reconnect"); 
  WiFi_reconnect = WiFi_reconnect + 1;
  WiFi.disconnect();
  WiFi.reconnect();
  delay(500);
  if (WiFi.status() == WL_CONNECTED) {
    // Once connected, publish an announcement...
    LogPrintf2("%s\n","WiFi reconnected"); 
  }
}

  void initMQTT() {
  String willTopic = Hostname + "/LWT";
  
  LogPrintln("setup MQTT\n");
  
  mqttClient.begin(ethClient);
  mqttClient.setHost(MQTT_BROKER, 1883);
  mqttClient.setWill(willTopic.c_str(), "Offline", true, 0);
}


void reconnect_mqtt()
{
  String willTopic = Hostname + "/LWT";
  String cmdTopic = Hostname + "/CMD/+";

  LogPrintf2("%s\n", "MQTT try reconnect");

  Mqtt_reconnect = Mqtt_reconnect + 1;

  if (mqttClient.connect(Hostname.c_str()))
  {
    LogPrintf2("%s\n", "MQTT connected");

    mqttClient.publish(willTopic.c_str(), "Online", true, 0);
  
    mqttClient.subscribe(cmdTopic.c_str());
  } else {
    LogPrintf2("Failed to connect to broker; error: %d\n", mqttClient.lastError());
  }
}