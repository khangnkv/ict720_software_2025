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

    sendTelegramMessage("M5StickC&YunHat started successfully (˶ᐢ ᵕ ᐢ˶)");
}

void loop() {
    // Update LED
    led_set_all((brightness << 16) | (brightness << 8) | brightness);
    
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
    
    // Check for Telegram messages
    checkTelegramMessages();

    // Check for button press
    M5.update();
    if (M5.BtnA.wasPressed()) {
        esp_restart();
    }
    
    delay(1000);
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
    tmp = sht20.read_temperature();
    hum = sht20.read_humidity();
    light = light_get();
    pressure = bmp.readPressure();
    M5.IMU.getAccelData(&accX, &accY, &accZ);
    M5.IMU.getGyroData(&gyroX, &gyroY, &gyroZ);
    
    Serial.printf("AX:%.2f,AY:%.2f,AZ:%.2f,GX:%.2f,GY:%.2f,GZ:%.2f,T:%.2fC\nH:%.2f\nP:%.2f\n",
        accX, accY, accZ, gyroX, gyroY, gyroZ, tmp, hum, pressure);
}

void checkAlerts() {
    // Temperature alert
    if (tmp > tempThreshold) {
        sendTelegramMessage("🔥 Temperature too high! Over 32°C!");
        mqttSendAlertFlag(); // Add this line
    }

    // Humidity alert
    if (hum < humThreshold) {
        sendTelegramMessage("💧 Humidity too low! Below 70%!");
        mqttSendAlertFlag(); // Add this line
    }
    
    // Pressure alert
    if (pressure > pressureThreshold) {
        sendTelegramMessage("🌪 Pressure too high! Over 1000 hPa!");
        mqttSendAlertFlag(); // Add this line
    }

    // Sudden movement alert
    if (abs(accX) > axisXThreshold || abs(accY) > axisYThreshold || abs(accZ) > axisZThreshold) {
        sendTelegramMessage("⚠️ Sudden movement detected!");
        mqttSendAlertFlag(); // Add this line
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
    HTTPClient http;
    // Request messages NEWER than the last one we processed
    String url = "https://api.telegram.org/bot" + String(botToken) + 
                 "/getUpdates?offset=" + String(lastProcessedUpdateId + 1) + "&limit=1";
    
    http.begin(url);
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
                
                // Check for commands
                if (response.indexOf("\"/status\"") != -1) {
                    sendStatusUpdate();
                }
            }
        }
    }
    http.end();
}

void sendStatusUpdate() {
    char message[250];
    snprintf(message, sizeof(message), 
             "📊 Status Report:\n"
             "Temperature: %.2f°C\n"
             "Humidity: %.2f%%\n"
             "Pressure: %.2f Pa\n"
             "Acceleration: X=%.2f Y=%.2f Z=%.2f\n"
             "Gyro: X=%.2f Y=%.2f Z=%.2f\n"
             "light: %d\n"
             "Update every %lu secound\n"
             "Next update in %lu secound", 
             tmp, hum, pressure, accX, accY, accZ, gyroX, gyroY, gyroZ,light,
             sleepTime, nextUpdateTime - millis() / 1000);
    sendTelegramMessage(message);
}

bool checkMqttConnection() {
    if (!mqttClient.connected()) {
        // Only try to reconnect at certain intervals to avoid blocking
        if (millis() - lastMqttReconnectAttempt > mqttReconnectInterval) {
            lastMqttReconnectAttempt = millis();
            if (!reconnectMQTT()) {
                Serial.println("MQTT connection failed, will retry later");
                return false;
            }
        } else {
            return false; // Skip this attempt if we're waiting for reconnect interval
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
            
            // Subscribe to alert flag topic
            mqttClient.subscribe(MQTT_TOPIC_ALERT_FLAG);
            Serial.print("Subscribed to topic: ");
            Serial.println(MQTT_TOPIC_ALERT_FLAG);
            
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