#include <Adafruit_BMP280.h>
#include "M5StickC.h"
#include "SHT20.h"
#include "yunBoard.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

SHT20 sht20;
Adafruit_BMP280 bmp;

uint32_t update_time = 0;
const char* ssid = "XePlant_2.4G";
const char* password = "ilove@coffee";
const char* botToken = "8091802131:AAFXAfn0aJEIVAV1NnTDHh4v8WpVcY8wuC4";
const char* chatID = "-4646258185";
float accX, accY, accZ;
float gyroX, gyroY, gyroZ;
float tmp, hum;
float pressure;
uint16_t light;

// Function declaration
void sendTelegramMessage(const char* message);

void setup() {
    M5.begin();
    Wire.begin(0, 26, 100000UL);
    M5.Imu.Init();

    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
        Serial.print("Attempting to connect to WiFi, status: ");
        Serial.println(WiFi.status());
  }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
        sendTelegramMessage("Hello from M5StickC!");
  } else {
        Serial.println("Failed to connect to WiFi");
  }
    if (!bmp.begin(0x76)) {
        Serial.println(
            F("Could not find a valid BMP280 sensor, check wiring!"));
    }

    /* Default settings from datasheet. */
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,  /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X2,  /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16, /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,   /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_1000); /* Standby time. */

    // put your setup code here, to run once:
}

uint8_t color_light = 5;

void loop() {

    led_set_all((color_light << 16) | (color_light << 8) | color_light);
    if (millis() > update_time) {
        update_time = millis() + 1000;
        tmp         = sht20.read_temperature();
        hum         = sht20.read_humidity();
        light       = light_get();
        pressure    = bmp.readPressure();
        M5.IMU.getAccelData(&accX, &accY, &accZ);
        M5.IMU.getGyroData(&gyroX, &gyroY, &gyroZ);
        Serial.printf("AX: %.2f, AY: %.2f, AZ: %.2f, GX: %.2f, GY: %.2f, GZ: %.2f, T: %.2f C\nPressure: %.2f \n",
            accX, accY, accZ, gyroX, gyroY, gyroZ, tmp, hum);
        // Temperature alert
        if (tmp > 32.0) {
            sendTelegramMessage("🔥 Temperature too high! Over 32°C!");
        }

        // Humidity alert
        if (hum < 20.0) {
            sendTelegramMessage("💧 Humidity too low! Below 20%!");
        }
        // Pressure alert
        if (pressure > 101500) {
            sendTelegramMessage("🌪 Pressure too high! Over 1015 hPa!");
        }

        // Sudden movement alert
        if (abs(accX) > 1.5 || abs(accY) > 1.5 || abs(accZ) > 1.5) {
            sendTelegramMessage("⚠️ Sudden movement detected!");
        }
        char message[100];
        snprintf(message, sizeof(message), "Sensor Data:\nAX: %.2f\nAY: %.2f\nAZ: %.2f\nGX: %.2f\nGY: %.2f\nGZ: %.2f\nT: %.2f C\nH: %.2f\n",
           accX, accY, accZ, gyroX, gyroY, gyroZ, tmp, hum);
        sendTelegramMessage(message);

        // Delay for a minute
        delay(1000);
        
    }

    M5.update();

    if (M5.BtnA.wasPressed()) {
        esp_restart();
    }

    delay(10);
    // put your main code here, to run repeatedly:
}

void sendTelegramMessage(const char* message) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = "https://api.telegram.org/bot" + String(botToken) + "/sendMessage";
  
      http.begin(url);
      http.addHeader("Content-Type", "application/json");
  
      String payload = "{\"chat_id\":\"" + String(chatID) + "\",\"text\":\"" + String(message) + "\",""\"disable_notification\":false}";
      int httpResponseCode = http.POST(payload);
  
      if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println(httpResponseCode);
        Serial.println(response);
      } else {
        Serial.print("Error on sending message: ");
        Serial.println(httpResponseCode);
      }
      http.end();
    } else {
      Serial.println("WiFi not connected");
    }
  }