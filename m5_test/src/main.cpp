#include <Adafruit_BMP280.h>
#include "M5StickC.h"
#include "config.h"
#include "SHT20.h"
#include "yunBoard.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WifiClient.h>
#include <HTTPClient.h>
#include <PubSubClient.h>  
#include <ArduinoJson.h>


// Sensors
SHT20 sht20;
Adafruit_BMP280 bmp;
uint16_t light;

// Mqtt
WiFiClient espClient;
PubSubClient mqttClient(espClient);
JsonDocument doc;
char payload[256]; // Buffer for MQTT payload
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long mqttReconnectInterval = 5000; // 5 seconds

// Timing
uint32_t nextUpdateTime = 0; 

// Display settings
uint8_t brightness = yunBrightness;  

// Telegram tracking
long lastProcessedUpdateId = 0;

//track alert count
unsigned int alertCount = 0;

bool pendingStatusRequest = false;
unsigned long lastTelegramCheck = 0;
const unsigned long telegramCheckInterval = 2000; // Check telegram every 2 seconds


// Function declarations
void setupWiFi();
void setupSensors();
void readSensors();
void checkAlerts();
void sendTelegramMessage(const char* message);
void checkTelegramMessages();
void sendStatusUpdate();
bool reconnectMQTT();
bool checkMqttConnection();
void mqttSendSensorData();
void mqttSendAlertFlag();
void mqttSendAlertFlagCount();

void setup() {
    Serial.begin(baud_rate);
    M5.begin();
    Wire.begin(0, 26, 100000UL);
    M5.Imu.Init();
    delay(1000);
    
    // Power saving settings
    setCpuFrequencyMhz(cpu_speed); 
    M5.Axp.SetLDO2(show_display);  
    WiFi.setSleep(true);
    
    // Initialize LED
    led_set_all(LED_DEFAULT_COLOR);
    
    // Connect to WiFi and setup sensors
    setupWiFi();
    setupSensors();

    // Initialize MQTT
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    if (reconnectMQTT()) {
        Serial.println("Connected to MQTT broker");
    }

    sendTelegramMessage("M5StickC&YunHat BabyCare+ ready to monitor your baby's room! (˶ᐢ ᵕ ᐢ˶)");
}

void loop() {
    // Update LED
    led_set_all((brightness << 16) | (brightness << 8) | brightness);

    // Check for Telegram messages at regular intervals
    if (millis() - lastTelegramCheck > telegramCheckInterval) {
        checkTelegramMessages();
        lastTelegramCheck = millis();
    }

    // Ensure MQTT connection
    if (!mqttClient.connected()) {
        reconnectMQTT();
    }
    mqttClient.loop();

    // Check if it's time to read sensors and update
    if (millis() > nextUpdateTime) {
        readSensors();
        checkAlerts();
        mqttSendSensorData();
        
        // Set next update time based on sleepTime from config
        nextUpdateTime = millis() + (sleepTime * 1000);
    }

     // Process any pending commands - do this in a less busy moment
     if (pendingStatusRequest) {
        sendStatusUpdate();
        pendingStatusRequest = false;
    }

    // Check for button press
    M5.update();
    if (M5.BtnA.wasPressed()) {
        esp_restart();
    }

    delay(500);
}

void setupWiFi() {
    WiFi.begin(ssid, password);
    int attempts = 0;
    
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi");
    } else {
        Serial.println("\nFailed to connect to WiFi");
    }
}

void setupSensors() {
    if (!bmp.begin(0x76)) {
        Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    }

    // BMP280 settings from datasheet
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode */
                    Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,      /* Filtering */
                    Adafruit_BMP280::STANDBY_MS_1000); /* Standby time */
}

void readSensors() {
    try {
        tmp = sht20.read_temperature();
        hum = sht20.read_humidity();
        light = light_get();
        pressure = bmp.readPressure();
        M5.IMU.getAccelData(&accX, &accY, &accZ);
        M5.IMU.getGyroData(&gyroX, &gyroY, &gyroZ);
        
        Serial.printf("AX:%.2f,AY:%.2f,AZ:%.2f,GX:%.2f,GY:%.2f,GZ:%.2f,T:%.2fC\nH:%.2f\nP:%.2f\n",
            accX, accY, accZ, gyroX, gyroY, gyroZ, tmp, hum, pressure);
        
    } catch (...) {
        Serial.println("Error during sensor reading, using previous values");
    }
}

void checkAlerts() {
    // Temperature alert
    if (tmp > tempHighThreshold) {
        sendTelegramMessage("🔥 Temperature too high! Over 32°C!");
        mqttSendAlertFlag();
        mqttSendAlertFlagCount();
    }

    if (tmp < tempLowThreshold) {
        sendTelegramMessage("❄️ Temperature too low! Below 32°C!");
        mqttSendAlertFlag();
        mqttSendAlertFlagCount();
    }

    // Humidity alert
    if (hum < humLowThreshold) {
        sendTelegramMessage("💧 Humidity too low! Below 40%!");
        mqttSendAlertFlag();
        mqttSendAlertFlagCount();
    }
    if (hum > humHighThreshold) {
        sendTelegramMessage("💧 Humidity too high! Over 70%!");
        mqttSendAlertFlag();
        mqttSendAlertFlagCount();
    }
    
    // Pressure alert
    if (pressure > pressureHighThreshold) {
        sendTelegramMessage("🌪 Pressure too high! Over 1007 hPa!");
        mqttSendAlertFlag();
        mqttSendAlertFlagCount();
    }
    if (pressure > pressureLowThreshold) {
        sendTelegramMessage("🌪 Pressure too low! Below 1001 hPa!");
        mqttSendAlertFlag();
        mqttSendAlertFlagCount();
    }

    // Sudden movement alert
    if (abs(accX) > axisXThreshold || abs(accY) > axisYThreshold || abs(accZ) > axisZThreshold) {
        sendTelegramMessage("⚠️ Sudden movement detected!");
        mqttSendAlertFlag();
        mqttSendAlertFlagCount();
    }
    
    // Optional: Enable light sleep mode
    // esp_sleep_enable_timer_wakeup(sleepTime * 1000000); 
    // esp_light_sleep_start();
}

void sendTelegramMessage(const char* message) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(botToken) + "/sendMessage";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"chat_id\":\"" + 
                    String(chatID) +
                    "\",\"text\":\"" + 
                    String(message) + "\"}";
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
}

void checkTelegramMessages() {
    if (WiFi.status() != WL_CONNECTED) {
        return;  // Skip if WiFi is not connected
    }
    
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(botToken) + 
                 "/getUpdates?offset=" + String(lastProcessedUpdateId + 1) + "&limit=1";
    
    http.begin(url);
    http.setTimeout(5000);  // Set timeout to 5 seconds
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
        String response = http.getString();
        
        // Extract update_id (to avoid processing the same message multiple times)
        int updateIdPos = response.indexOf("\"update_id\":");
        if (updateIdPos > 0 && response.indexOf("\"message\"") > 0) {
            // Parse out the update_id
            long updateId = response.substring(updateIdPos + 12, response.indexOf(",", updateIdPos)).toInt();
            
            // Only process if it's a new message
            if (updateId > lastProcessedUpdateId) {
                lastProcessedUpdateId = updateId;
                
                // Extract the message text
                int textPos = response.indexOf("\"text\":\"");
                if (textPos > 0) {
                    // Find the end of the text field
                    int textEndPos = response.indexOf("\"", textPos + 8);
                    if (textEndPos > textPos) {
                        // Extract the message text
                        String messageText = response.substring(textPos + 8, textEndPos);
                        Serial.println("Received message: " + messageText);
                        
                        // Queue the command instead of executing immediately
                        if (messageText == "/status") {
                            Serial.println("Status command received - queued for processing");
                            pendingStatusRequest = true;
                            // Send acknowledgment immediately
                            sendTelegramMessage("Command received! Processing status update...");
                        }
                    }
                }
            }
        }
    }
    http.end();
}

void sendStatusUpdate() {
    char message[250];
    unsigned long timeToNext = 0;
    if (nextUpdateTime > millis()) {
        timeToNext = (nextUpdateTime - millis()) / 1000;
    }
    
    snprintf(message, sizeof(message), 
             "📊 Status Report:\n"
             "Temperature: %.1f°C\n"
             "Humidity: %.1f%%\n"
             "Pressure: %.0f Pa\n"
             "Acceleration:\n"
             "X=%.2f Y=%.2f Z=%.2f\n"
             "Light: %d\n"
             "Update every %lu sec\n"
             "Next update in %lu sec", 
             tmp, hum, pressure,
             accX, accY, accZ, light,
             sleepTime, timeToNext);

    // Send first part immediately
    sendTelegramMessage(message);
    
    Serial.println("Status update sent successfully");
}

bool checkMqttConnection() {
    if (!mqttClient.connected()) {
        if (millis() - lastMqttReconnectAttempt > mqttReconnectInterval) {
            lastMqttReconnectAttempt = millis();
            if (!reconnectMQTT()) {
                Serial.println("MQTT connection failed, will retry later");
                return false;
            }
        } else {
            return false; 
        }
    }
    return true;
}

bool reconnectMQTT() {
    if (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        
        // Create a random client ID
        String clientId = MQTT_CLIENT_ID;
        clientId += String(random(0xffff), HEX);
        
        // Attempt to connect
        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("connected");
            return true;
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            return false;
        }
    }
    return true;
}

void mqttSendSensorData() {
    if (!checkMqttConnection()) {
        return; // Exit if not connected
    }

    // Clear document and prepare payload
    doc.clear();
    doc["timestamp"] = millis();
    doc["temperature"] = tmp;
    doc["humidity"] = hum;
    doc["pressure"] = pressure;
    doc["light"] = light;
    doc["acc_x"] = accX;
    doc["acc_y"] = accY;
    doc["acc_z"] = accZ;
    
    // Serialize to JSON and publish
    serializeJson(doc, payload);
    bool published = mqttClient.publish(MQTT_TOPIC_STATUS, payload);
    
    if (published) {
        Serial.println("MQTT sensor data published successfully");
    } else {
        Serial.println("Failed to publish MQTT sensor data");
    }
    
    // Process any incoming messages
    mqttClient.loop();
}

void mqttSendAlertFlag() {
    if (!checkMqttConnection()) {
        return; // Exit if not connected
    }

    // Send a simple alert flag message
    bool published = mqttClient.publish(MQTT_TOPIC_ALERT_FLAG, "alert");
    
    if (published) {
        Serial.println("MQTT alert flag published successfully");
    } else {
        Serial.println("Failed to publish MQTT alert flag");
    }
    
    // Process any incoming messages
    mqttClient.loop();
}

void mqttSendAlertFlagCount() {
    if (!checkMqttConnection()) {
        return; // Exit if not connected
    }

    // Increment alert counter
    alertCount++;
    
    doc.clear();
    doc["timestamp"] = millis();
    doc["alert_flag_count"] = alertCount;
   
    // Serialize to JSON and publish
    serializeJson(doc, payload);
    bool published = mqttClient.publish(MQTT_TOPIC_ALERT_FLAG_COUNT, payload);
    
    if (published) {
        Serial.println("MQTT alert count published successfully");
    } else {
        Serial.println("Failed to publish MQTT alert count");
    }
    
    // Process any incoming messages
    mqttClient.loop();
}