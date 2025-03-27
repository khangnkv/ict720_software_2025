//Set your WiFi SSID and password
const char* ssid = -
const char* password = -

//Set your Telegram Bot Token and Chat ID
const char* botToken = -
const char* chatID = -

#define baud_rate 115200

// Mqtt settings
#define MQTT_BROKER                 -
#define MQTT_PORT                   1883
#define MQTT_CLIENT_ID              "babyCare_M5&Yunhat"
#define MQTT_TOPIC_ALERT_FLAG       "ict720/babyCare/alertflag"
#define MQTT_TOPIC_ALERT_FLAG_COUNT "ict720/babyCare/count"
#define MQTT_TOPIC_STATUS           "ict720/babyCare/status"

//Sensor variables
float accX, accY, accZ;
float gyroX, gyroY, gyroZ;
float tmp, hum;
float pressure;

//Cpu speed
#define cpu_speed 100

//Display 
#define show_display false

//Yun Hat LED
#define LED_DEFAULT_COLOR 0x000000
int yunBrightness = 1;

//Sensor values threshold
float tempHighThreshold = 32;
float tempLowThreshold = 22;
float humHighThreshold = 70;
float humLowThreshold = 40;
float pressureHighThreshold = 101600;
float pressureLowThreshold = 101300;
float axisXThreshold = 0.7;
float axisYThreshold = 0.7;
float axisZThreshold = 1.5;

//update delay
unsigned long sleepTime = 30; // in seconds 