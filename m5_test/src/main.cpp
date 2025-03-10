#include <Arduino.h>
#include <M5StickC.h>
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "XePlant_2.4G";
const char* password = "ilove@coffee";
const char* botToken = "8091802131:AAFXAfn0aJEIVAV1NnTDHh4v8WpVcY8wuC4";
const char* chatID = "-4646258185";

// Function declaration
void sendTelegramMessage(String message);

void setup() {
  Serial.begin(115200);
  M5.begin();
  M5.Lcd.setTextSize(1);
  M5.Lcd.fillScreen(BLACK);

  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    M5.Lcd.print(".");
    attempts++;
    Serial.print("Attempting to connect to WiFi, status: ");
    Serial.println(WiFi.status());
  }
  if (WiFi.status() == WL_CONNECTED) {
    M5.Lcd.println("Connected to WiFi");
    Serial.println("Connected to WiFi");
    sendTelegramMessage("Hello from M5StickC!");
  } else {
    M5.Lcd.println("Failed to connect to WiFi");
    Serial.println("Failed to connect to WiFi");
  }
}

void loop() {
  delay(100); // Delay to avoid spamming the Telegram API /// Tessst
}

void sendTelegramMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(botToken) + "/sendMessage?chat_id=" + String(chatID) + "&text=" + message;

    http.begin(url);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(response);
      M5.Lcd.println("Message sent!");
    } else {
      Serial.print("Error on sending message: ");
      Serial.println(httpResponseCode);
      M5.Lcd.printf("Error: %d", httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
    M5.Lcd.println("WiFi not connected");
  }
}
