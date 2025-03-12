#include <Arduino.h>
#include <task.h>
#include <queue.h>
#include "hw_mic.h"
#include <WiFi.h>
#include <HTTPClient.h>

// WiFi and Telegram credentials
const char* ssid = "XePlant_2.4G";
const char* password = "ilove@coffee";
const char* botToken = "8091802131:AAFXAfn0aJEIVAV1NnTDHh4v8WpVcY8wuC4";
const char* chatID = "-4646258185";

// Variables for noise detection
int32_t mic_samples[1600];   // Buffer for microphone samples
float avg_val = 0.0;         // Average noise level
const int noiseThreshold = 3000; // Noise level threshold for alert

// Telegram message function
void sendTelegramMessage(const char* message);

// Mic-related variables
SemaphoreHandle_t xSemaphore = NULL;
int mem_idx = 0;
WiFiClient espClient;
HTTPClient http;

// Mic reading task and processing task
void read_mic_init(void);
void read_mic_task(void *pvParam);
void process_mic_task(void *pvParam);

void setup() {
  Serial.begin(115200);
  delay(15000);  // Wait for the system to settle

  // Connect to WiFi
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(100);
    attempts++;
    Serial.print("Attempting to connect to WiFi...");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
  } else {
    Serial.println("WiFi connection failed");
  }

  // Prepare the Semaphore for synchronization
  xSemaphore = xSemaphoreCreateBinary();

  // Create tasks for reading mic and processing mic data
  xTaskCreate(read_mic_task, "READ_MIC_TASK", 4096, NULL, 3, NULL);
  xTaskCreate(process_mic_task, "PROCESS_MIC_TASK", 4096, NULL, 3, NULL);
}

void loop() {
  delay(1000);  // Main loop delay
}

void read_mic_init(void) {
  hw_mic_init(16000);  // Initialize microphone with sample rate of 16 kHz
}

void read_mic_task(void *pvParam) {
  read_mic_init();
  int cur_mem_idx = 0;
  while (1) {
    static unsigned int num_samples = 1600;
    hw_mic_read(mic_samples + cur_mem_idx * 1600, &num_samples);  // Read mic data into buffer
    mem_idx = cur_mem_idx;
    cur_mem_idx = (cur_mem_idx + 1) % 2;
    xSemaphoreGive(xSemaphore);  // Signal that new data is available
    delay(50);  // Delay to give processing time
  }

}

void process_mic_task(void *pvParam) {
  while (1) {
    xSemaphoreTake(xSemaphore, portMAX_DELAY);  // Wait for new data

    // Calculate the average noise level
    avg_val = 0.0;
    for (int i = 0; i < 1600; i++) {
      avg_val += abs(mic_samples[mem_idx * 1600 + i]) / 1600.0;
    }

    // If the noise exceeds the threshold, send an alert
    if (avg_val > noiseThreshold) {
      Serial.println("Noise level exceeded threshold!");
      sendTelegramMessage("🚨 Noise detected in baby's room! 🚨");
    }
    char message[100];
    snprintf(message, sizeof(message), "Noise level: %.2f", avg_val);
      // To avoid spamming Telegram, we add a delay after sending a message
      delay(10000);  // 10 seconds cooldown before checking again
    sendTelegramMessage(message);
    delay(1000);  // Delay to give reading time
  }
}

void sendTelegramMessage(const char* message) {
  if (WiFi.status() == WL_CONNECTED) {
    http.begin("https://api.telegram.org/bot" + String(botToken) + "/sendMessage");
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"chat_id\":\"" + String(chatID) + "\",\"text\":\"" + String(message) + "\"}";
    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Message sent: " + response);
    } else {
      Serial.print("Error sending message: ");
      Serial.println(httpResponseCode);
    }
    http.end();  // End the HTTP request
  } else {
    Serial.println("WiFi not connected!");
  }
}