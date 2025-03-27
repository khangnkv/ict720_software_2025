//set the ssid and password of the wifi
const char* ssid = -
const char* password = -

//set the token and chat id of the telegram bot
const char* botToken = -
const char* chatID = -

// Mqtt settings
#define MQTT_BROKER       -
#define MQTT_PORT         1883
#define MQTT_CLIENT_ID    "babyCare_Tsimcam"
#define MQTT_TOPIC_ALERT  "ict720/babyCare/alertcount"
#define MQTT_TOPIC_NOISE  "ict720/babyCare/noise"
#define MQTT_TOPIC_ALERT_FLAG "ict720/babyCare/alertflag"

// Set the baud rate for serial communication
#define baud_rate 115200

// Set camera pins
#define SD_MISO_PIN      40
#define SD_MOSI_PIN      38
#define SD_SCLK_PIN      39
#define SD_CS_PIN        47
#define PCIE_PWR_PIN     48
#define PCIE_TX_PIN      45
#define PCIE_RX_PIN      46
#define PCIE_LED_PIN     21
#define MIC_IIS_WS_PIN   42
#define MIC_IIS_SCK_PIN  41
#define MIC_IIS_DATA_PIN 2
#define CAM_PWDN_PIN     -1
#define CAM_RESET_PIN    -1
#define CAM_XCLK_PIN     14
#define CAM_SIOD_PIN     4
#define CAM_SIOC_PIN     5
#define CAM_Y9_PIN       15
#define CAM_Y8_PIN       16
#define CAM_Y7_PIN       17
#define CAM_Y6_PIN       12
#define CAM_Y5_PIN       10
#define CAM_Y4_PIN       8
#define CAM_Y3_PIN       9
#define CAM_Y2_PIN       11
#define CAM_VSYNC_PIN    6
#define CAM_HREF_PIN     7
#define CAM_PCLK_PIN     13
#define BUTTON_PIN       0
#define PWR_ON_PIN       1
#define SERIAL_RX_PIN    44
#define SERIAL_TX_PIN    43
#define BAT_VOLT_PIN     -1

// Set the noise threshold in dB 
#define MIC_SAMPLE_COUNT 1600
#define MIC_SAMPLE_RATE 16000
const int noiseThreshold = 50; // Noise threshold in dB

// Define the maximum number of images to capture
#define MAX_IMAGES 10

//loop delay
int loopDelay = 500;

//Image capture delay
int imageCaptureDelay = 500;

// Cooldown period in milliseconds (e.g., 60 seconds)
const unsigned long alertCooldown = 60000; 

// Reconnect interval in milliseconds
const unsigned long mqttReconnectInterval = 5000;
