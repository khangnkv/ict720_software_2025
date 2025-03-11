#include <Arduino.h>
#include <task.h>
#include <queue.h>
#include "hw_mic.h"
#include "config.h"
#include <WiFiClient.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "esp_camera.h"
#include "esp_http_server.h"

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
void startCamera(void);
void startCameraServer(void);

esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  char *part_header = "Content-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n";
  
  // Set multipart response type
  httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");

  unsigned long lastTime = millis();
  int frameCount = 0;

  while (true) {
      fb = esp_camera_fb_get();
      if (!fb) {
          Serial.println("Camera capture failed");
          continue;
      }

      char buffer[64];
      int header_len = snprintf(buffer, 64, part_header, fb->len);
      httpd_resp_send_chunk(req, buffer, header_len);
      httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
      httpd_resp_send_chunk(req, "\r\n--frame\r\n", 10);

      esp_camera_fb_return(fb);
      delay(100);  // Adjust delay to control frame rate

      frameCount++;
      unsigned long currentTime = millis();
      if (currentTime - lastTime >= 1000) {
          Serial.printf("FPS: %d\n", frameCount);
          frameCount = 0;
          lastTime = currentTime;
      }
  }
  return ESP_OK;
}



void setup(void) {
  // 0. Initialize Serial
  delay(10000);
  Serial.begin(115200);
  if (psramFound()) {
    Serial.println("PSRAM is detected");
} else {
    Serial.println("PSRAM not detected - Camera may fail");
}
  Serial.println("Serial Initialized");
  WiFi.begin("BY_DOM_2.4G","0961873889");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  Serial.println("Connected to the WiFi network");
  Serial.printf( "IP Address: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("RSSI: %d\n", WiFi.RSSI());}


  startCamera();
  startCameraServer();


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
    // Serial.println(avg_val); // Print the average value of the samples
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

void startCamera() {
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
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
        config.frame_size = FRAMESIZE_UXGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
    }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x", err);
      return;
  }

  delay(500); // Give the camera some time to stabilize

  Serial.println("Camera initialized successfully");
}

// void capturePhoto() {
//   camera_fb_t *fb = esp_camera_fb_get();
//   if (!fb) {
//       Serial.println("Camera capture failed");
//       return;
//   }

//   // Save to SPIFFS
//   File file = SPIFFS.open("/photo.jpg", FILE_WRITE);
//   if (!file) {
//       Serial.println("Failed to open file for writing");
//       return;
//   }
//   file.write(fb->buf, fb->len);
//   file.close();

//   Serial.println("Photo saved");

//   esp_camera_fb_return(fb);
// }

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_handle_t server = NULL;
  if (httpd_start(&server, &config) == ESP_OK) {
      httpd_uri_t uri = {
          .uri = "/stream",
          .method = HTTP_GET,
          .handler = stream_handler,
          .user_ctx = NULL
      };
      httpd_register_uri_handler(server, &uri);
      Serial.println("Camera stream available at:");
      Serial.print("http://");
      Serial.print(WiFi.localIP());
      Serial.println("/stream");
  }
}