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
const char* ssid = "MOUREE-2.4G";
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
    delay(1000);
    // 🟢 Reduce CPU speed to save battery
    setCpuFrequencyMhz(80); 
    // 🟢 Turn off the display completely
    M5.Axp.SetLDO2(false);  
    // 🟢 Yun Hat LED
    led_set_all(0x000000);
    // 🟢 Enable WiFi sleep mode to save power
    WiFi.setSleep(true);


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
        sendTelegramMessage("Hello from your hot chick!");
  } else {
        Serial.println("Failed to connect to your hot chick");
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

uint8_t brightness = 1;  // Low brightness (0-255)
void loop() {
    led_set_all((brightness << 16) | (brightness << 8) | brightness); // Dim White LED
    if (millis() > update_time) {
        update_time = millis() + 1000;
        tmp         = sht20.read_temperature();
        hum         = sht20.read_humidity();
        light       = light_get();
        pressure    = bmp.readPressure();
        M5.IMU.getAccelData(&accX, &accY, &accZ);
        M5.IMU.getGyroData(&gyroX, &gyroY, &gyroZ);
        Serial.printf("AX:%.2f,AY:%.2f,AZ:%.2f,GX:%.2f,GY:%.2f,GZ:%.2f,T:%.2fC\nH:%.2f\nP:%.2f\n",
            accX, accY, accZ, gyroX, gyroY, gyroZ, tmp, hum, pressure);
        // Temperature alert
        if (tmp > 32.0) {
            sendTelegramMessage("🔥 Temperature too high! Over 32°C!");
        }

        // Humidity alert
        if (hum < 70) {
            sendTelegramMessage("💧 Humidity too low! Below 70%!");
        }
        // Pressure alert
        if (pressure > 100000) {
            sendTelegramMessage("🌪 Pressure too high! Over 1000 hPa!");
        }

        // Sudden movement alert
        if (abs(accX) > 0.7|| abs(accY) > 0.7 || abs(accZ) > 1.5) {
            sendTelegramMessage("⚠️ Sudden movement detected!");
        }
        char message[100];
        snprintf(message, sizeof(message), "Sensor Data:\nAX:%.2f\nAY:%.2f\nAZ:%.2f\nGX:%.2f\nGY:%.2f\nGZ:%.2f\nT:%.2fC\nH:%.2f\nP:%.2f\n",
           accX, accY, accZ, gyroX, gyroY, gyroZ, tmp, hum, pressure);
        sendTelegramMessage(message);
        // Delay for a minute
        // 🟢 Put the device into light sleep mode for 30 seconds to save power
        esp_sleep_enable_timer_wakeup(30 * 1000000); // 30 seconds sleep
        esp_light_sleep_start(); // Enter light sleep
        
    }

    M5.update();

    if (M5.BtnA.wasPressed()) {
        esp_restart();
    }
    delay(1000);
}

void sendTelegramMessage(const char* message) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = "https://api.telegram.org/bot" + String(botToken) + "/sendMessage";
  
      http.begin(url);
      http.addHeader("Content-Type", "application/json");
  
      String payload = "{\"chat_id\":\"" + 
                        String(chatID) +
                        "\",\"text\":\"" + 
                        String(message) + "\"";
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