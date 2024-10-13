// set hostname used for MQTT tag and WiFi
#define HOSTNAME "ESPsteckerleiste"
#define MQTT_BROKER "192.168.178.15"
#define VERSION "v 5.2.0"

#define MQTT_INTERVAL 120000
#define RECONNECT_INTERVAL 5000
#define LED_BLINK_INTERVAL 500
#define RELAY_RESET_INTERVAL 10000

#define GPIO_LED 2
// Set number of outputs
#define NUM_OUTPUTS  8

// ---------------------->>>>>>>>
// Serial IO kann nicht genutzt werden!!!
// GPIO01=Tx und GPIO03=Rx werden für Realis benutzt!!!
// d.h. wenn Logging eingeschaltet GPIOs umdefinieren
//
//Beim Update schalten die Relais 7 und 8 !!! Stecker entfernen!! Keine Last !!!
//
#define LOGno
#ifdef LOG 
#define SERIALINIT Serial.begin(115200);
#define LogPrintf(x) Serial.printf(x)
#define LogPrintln(x) Serial.println(x)
#define LogPrintf2(x,y) Serial.printf(x,y)
#define LogPrintln2(x,y) Serial.println(x,y)
#define LogPrintf3(x,y,z) Serial.printf(x,y,z)
#define SetGPIO int outputGPIOs[NUM_OUTPUTS] =  {16, 5, 4, 14, 12, 13, 12, 13}; 
#else
#define SERIALINIT
#define LogPrintf(x)
#define LogPrintln(x)
#define LogPrintf2(x,y)
#define LogPrintln2(x,y)
#define LogPrintf3(x,y,z)
#define SetGPIO int outputGPIOs[NUM_OUTPUTS] =  {16, 5, 4, 14, 12, 13, 3, 1};
#endif