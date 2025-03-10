#include <Arduino.h>
#include <task.h>
#include <queue.h>
#include "hw_mic.h"
#include <WiFiClient.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "esp_camera.h"

//eap32 camera define value
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    10
#define SIOD_GPIO_NUM    40
#define SIOC_GPIO_NUM    39
#define Y9_GPIO_NUM      48
#define Y8_GPIO_NUM      11
#define Y7_GPIO_NUM      12
#define Y6_GPIO_NUM      14
#define Y5_GPIO_NUM      16
#define Y4_GPIO_NUM      18
#define Y3_GPIO_NUM      17
#define Y2_GPIO_NUM      15
#define VSYNC_GPIO_NUM   6
#define HREF_GPIO_NUM    7
#define PCLK_GPIO_NUM    13

// As T-Simcam has 2 mics (Left and Right), so use multitasking to choose which one to read

// Variables Declaration
SemaphoreHandle_t xSemaphore = NULL;
int mem_idx = 0;
int32_t mic_samples[2 * 1600];      // 2 Memmory index -> Allocate = 2X
WiFiClient espClient;   //can change to EthernetClient or 4GClient or 5GClient or WiFiClient or WiFiMultiClient or other client
PubSubClient mqtt_client(espClient);
JsonDocument doc;
float avg_val = 0.0;

// Function Prototypes
void read_mic_init(void);
void read_mic_task(void *pvParam);
void process_mic_init(void);
void process_mic_task(void *pvParam);
void on_message(char* topic, byte* payload, unsigned int length);


void setup(void) {
  // 0. Initialize Serial
  delay(5000);
  Serial.begin(115200);
  // Camera Init
  // Enable more power for the camera
  camera_config_t config;
  Serial.println("Camera init");
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CAM_Y2_PIN;
    config.pin_d1 = CAM_Y3_PIN;
    config.pin_d2 = CAM_Y4_PIN;
    config.pin_d3 = CAM_Y5_PIN;
    config.pin_d4 = CAM_Y6_PIN;
    config.pin_d5 = CAM_Y7_PIN;
    config.pin_d6 = CAM_Y8_PIN;
    config.pin_d7 = CAM_Y9_PIN;
    config.pin_xclk = CAM_XCLK_PIN;
    config.pin_pclk = CAM_PCLK_PIN;
    config.pin_vsync = CAM_VSYNC_PIN;
    config.pin_href = CAM_HREF_PIN;
    config.pin_sccb_sda = CAM_SIOD_PIN;
    config.pin_sccb_scl = CAM_SIOC_PIN;
    config.pin_pwdn = CAM_PWDN_PIN;
    config.pin_reset = CAM_RESET_PIN;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG; // for streaming
    // config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition

    // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
    //                      for larger pre-allocated frame buffer.
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera initialization failed");
    return;
  }

  Serial.println("Camera ready!");
  //----------
  // Serial.println("Serial Initialized");
  // WiFi.begin("BY_DOM_2.4G","0961873889");
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.println("Connecting to WiFi...");
  // Serial.println("Connected to the WiFi network");
  // Serial.printf( "IP Address: %s\n", WiFi.localIP().toString().c_str());
  // Serial.printf("RSSI: %d\n", WiFi.RSSI());}


  // 1. Prepare Semaphore
  xSemaphore = xSemaphoreCreateBinary();

  // 2. Create Task to Read Mic
  xTaskCreate(read_mic_task, "READ_MIC_TASK", 4096, NULL, 3, NULL);

  // 3. Create Task to Process Mic Data
  xTaskCreate(process_mic_task, "PROCESS_MIC_TASK", 4096, NULL, 3, NULL);

  // 4. Connect to MQTT Broker
  // mqtt_client.setServer("broker.emqx.io", 1883);
  // mqtt_client.setCallback(on_message);
  // mqtt_client.connect("chad_mosthandsomemanonthisplanet");
  // mqtt_client.subscribe("ict720/potatogamer/esp32/cmd");
  // Serial.println("Connected to MQTT Broker");
}

void loop(void) {
  // mqtt_client.publish("ict720/chad/esp32/beat", "ok"); // heartbeat protocol to check if the device is still connected
  // mqtt_client.loop(); // Keep the MQTT connection by looping the connection to the broker to check if there is any message
  delay(1000);
  camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return;
    }

    Serial.printf("Picture taken! Size: %d bytes\n", fb->len);

    esp_camera_fb_return(fb);
  Serial.println("Looping...");
}

void read_mic_init(void) {
  hw_mic_init(16000);
}

void read_mic_task(void *pvParam) {
  read_mic_init();
  int cur_mem_idx = 0;
  while(1) {
    static unsigned int num_samples = 1600;
    hw_mic_read(mic_samples + cur_mem_idx * 1600, &num_samples);
    mem_idx = cur_mem_idx;
    cur_mem_idx = (cur_mem_idx + 1) % 2;
    xSemaphoreGive(xSemaphore);
  }
}

void process_mic_task(void *pvParam) {
  while(1) {
    xSemaphoreTake(xSemaphore, portMAX_DELAY);
    avg_val = 0.0;
    for (int i=0; i<1600; i++){
      avg_val += (float)abs(mic_samples[mem_idx * 1600 + i]) / 1600;
    }
    Serial.println(avg_val); // Print the average value of the samples
  }
}

void on_message(char* topic, byte* payload, unsigned int length){
  char buff[200];
  memcpy(buff, payload, length);
  buff[length] = '\0'; // end of string
  Serial.printf("Message arrived [%s] %s\n", topic, buff);
  deserializeJson(doc, buff);
    if (doc["cmd"] == "start") {
      // Start the recording
      Serial.println("Start recording");
      doc.clear();
      doc["status"] = "ok";
      doc["value"] = avg_val; 
      serializeJson(doc, buff);
      mqtt_client.publish("ict720/potatogamer/esp32/resp", buff);
      Serial.println("Published");
  }
}