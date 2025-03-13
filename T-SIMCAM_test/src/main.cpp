#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "hw_mic.h"

// Wi-Fi and Telegram credentials
const char* ssid = "XePlant_2.4G";
const char* password = "ilove@coffee";
const char* botToken = "8091802131:AAFXAfn0aJEIVAV1NnTDHh4v8WpVcY8wuC4";
const char* chatID = "-4646258185";

// Microphone and noise detection
int32_t mic_samples[1600];
float avg_val = 0.0;
const int noiseThreshold = 3000;

void sendTelegramMessage(const char* message);

// Wi-Fi and HTTP clients
WiFiClient espClient;
HTTPClient http;

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi");
  } else {
    Serial.println("\nFailed to connect to Wi-Fi");
    return;
  }

  // Initialize microphone
  hw_mic_init(16000);
}

void loop() {
  unsigned int num_samples = 1600;
  hw_mic_read(mic_samples, &num_samples);

  // Calculate average noise level
  avg_val = 0.0;
  for (int i = 0; i < 1600; i++) {
    avg_val += abs(mic_samples[i]) / 1600.0;
  }

  Serial.printf("Noise level: %.2f\n", avg_val);

  // Send alert if noise exceeds threshold
  if (avg_val > noiseThreshold) {
    sendTelegramMessage("🚨 Noise detected in baby's room! 🚨");
    delay(10000); // Cooldown period
  }

  delay(1000); // Delay before next reading
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
    http.end();
  } else {
    Serial.println("Wi-Fi not connected!");
  }
}
