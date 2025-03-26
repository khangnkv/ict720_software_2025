//Set your WiFi SSID and password
const char* ssid = "BY_DOM_2.4G";
const char* password = "0961873889";

//Set your Telegram Bot Token and Chat ID
// const char* botToken = "8091802131:AAFXAfn0aJEIVAV1NnTDHh4v8WpVcY8wuC4";
// const char* chatID = "-4646258185";
const char* botToken = "8014819411:AAGoPCj--tjormpguatqeDhNwC6NwwU4ikM";
const char* chatID = "5854488746";

#define baud_rate 115200

// Mqtt settings
#define MQTT_BROKER       "192.168.1.26"
#define MQTT_PORT         1883
#define MQTT_CLIENT_ID    "babyMonitor_M5&Yunhat"
#define MQTT_TOPIC_ALERT_FLAG "ict720/babyMonitor/alertflag"
#define MQTT_TOPIC_STATUS "ict720/babyMonitor/status"

//Sensor variables
float accX, accY, accZ;
float gyroX, gyroY, gyroZ;
float tmp, hum;
float pressure;

//Cpu speed
#define cpu_speed 80

//Display 
#define show_display false

//Yun Hat LED
#define LED_DEFAULT_COLOR 0x000000
int yunBrightness = 1;

//Sensor values threshold
float tempThreshold = 32;
float humThreshold = 70;
float pressureThreshold = 101325;
float axisXThreshold = 0.7;
float axisYThreshold = 0.7;
float axisZThreshold = 1.5;

//update delay
unsigned long sleepTime = 30; // in seconds 