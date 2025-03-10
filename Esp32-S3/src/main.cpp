#include <Arduino.h>
#include <task.h>
#include <queue.h>
#include "hw_mic.h"
#include <WiFiClient.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

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
  Serial.println("Serial Initialized");
  WiFi.begin("BY_DOM_2.4G","0961873889");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  Serial.println("Connected to the WiFi network");
  Serial.printf( "IP Address: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("RSSI: %d\n", WiFi.RSSI());}


  // 1. Prepare Semaphore
  xSemaphore = xSemaphoreCreateBinary();

  // 2. Create Task to Read Mic
  xTaskCreate(read_mic_task, "READ_MIC_TASK", 4096, NULL, 3, NULL);

  // 3. Create Task to Process Mic Data
  xTaskCreate(process_mic_task, "PROCESS_MIC_TASK", 4096, NULL, 3, NULL);

  // 4. Connect to MQTT Broker
  mqtt_client.setServer("broker.emqx.io", 1883);
  mqtt_client.setCallback(on_message);
  mqtt_client.connect("chad_mosthandsomemanonthisplanet");
  mqtt_client.subscribe("ict720/potatogamer/esp32/cmd");
  Serial.println("Connected to MQTT Broker");
}

void loop(void) {
  // mqtt_client.publish("ict720/chad/esp32/beat", "ok"); // heartbeat protocol to check if the device is still connected
  mqtt_client.loop(); // Keep the MQTT connection by looping the connection to the broker to check if there is any message
  delay(1000);
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