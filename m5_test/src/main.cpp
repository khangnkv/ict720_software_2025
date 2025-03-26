#include <Adafruit_BMP280.h>
#include "M5StickC.h"
#include "config.h"
#include "SHT20.h"
#include "yunBoard.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

SHT20 sht20;
Adafruit_BMP280 bmp;

uint32_t update_time = sleepTime * 1000;
uint16_t light;

uint8_t brightness = yunBrightness;  

// Last time checked for messages
long lastProcessedUpdateId = 0;

// Variables to track errors
bool sensorReadError = false;
unsigned long lastSensorUpdate = 0;
unsigned long lastWifiCheck = 0;
bool sht20_enabled = true;
bool bmp280_enabled = true;
int i2c_failures = 0;
const int MAX_I2C_FAILURES = 5;
const int MAX_SENSOR_INIT_ATTEMPTS = 3;

int sleepMode = 1; // 0 = light sleep (default), 1 = no sleep, 2 = deep sleep.

// Function declaration
void sendTelegramMessage(const char* message);
void checkTelegramMessages();
void sendStatusUpdate();
void readSensors();
void checkAlerts();
void updateLed();
bool handleI2CError(float value, const char* sensorName, bool allowZero = true);
void reconnectWifi();
void resetI2C();
bool initSensors();

void setup() {
    Serial.begin(115200);
    M5.begin();
    
    // More robust I2C initialization
    bool sensorsInitialized = false;
    for (int attempt = 0; attempt < MAX_SENSOR_INIT_ATTEMPTS && !sensorsInitialized; attempt++) {
        if (attempt > 0) {
            Serial.printf("Sensor initialization attempt %d/%d\n", attempt+1, MAX_SENSOR_INIT_ATTEMPTS);
            delay(1000);
        }
        
        resetI2C();
        sensorsInitialized = initSensors();
    }
    
    // 🟢 Reduce CPU speed to save battery
    setCpuFrequencyMhz(cpu_speed); 
    
    
    // 🟢 Turn off the display completely
    M5.Axp.SetLDO2(show_display);  
    
    // 🟢 Yun Hat LED
    led_set_all(LED_DEFAULT_COLOR);

    delay(500);
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
    } else {
        Serial.println("Failed to connect to WiFi");
    }
    
    if (!bmp.begin(0x76)) {
        Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    }

    sendTelegramMessage("M5StickC&YunHat started successfully (˶ᐢ ᵕ ᐢ˶)");
}

void loop() {
    // Handle LED updates
    updateLed();
    
    // Check WiFi connection every 30 seconds
    if (millis() - lastWifiCheck > 30000) {
        reconnectWifi();
        lastWifiCheck = millis();
    }
    
    // Read sensors and check alerts at appropriate intervals
    if (millis() - lastSensorUpdate > (sleepTime * 1000)) {
        readSensors();
        
        if (!sensorReadError) {
            checkAlerts();
        }
        
        lastSensorUpdate = millis();
        
        // Enter light sleep to save power
        if (sleepMode == 0) {
            // Light sleep - wakes up quickly
            esp_sleep_enable_timer_wakeup(100000); // 100ms sleep
            esp_light_sleep_start();
        } else if (sleepMode == 2) {
            // Deep sleep - send notification first
            String deepSleepMsg = "📵 Entering deep sleep for " + String(sleepTime) + " seconds";
            sendTelegramMessage(deepSleepMsg.c_str());
            delay(1000); // Wait for message to be sent
            
            // Configure deep sleep
            esp_sleep_enable_timer_wakeup(sleepTime * 1000000); // Convert to microseconds
            esp_deep_sleep_start(); // Device will restart after waking
        }
    }
    
    // Check for messages from Telegram
    checkTelegramMessages();
    
    // Handle button presses
    M5.update();
    if (M5.BtnA.wasPressed()) {
        esp_restart();
    }
    
    // Short delay to prevent CPU hogging
    delay(10); // Reduced from 100ms to 10ms for better responsiveness
}

void reconnectWifi() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, attempting to reconnect...");
        WiFi.disconnect();
        WiFi.begin(ssid, password);
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 10) {
            delay(500);
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Reconnected to WiFi");
        } else {
            Serial.println("Failed to reconnect to WiFi");
        }
    }
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
    } else {
        Serial.println("WiFi not connected");
    }
}

void checkTelegramMessages() {
    if (WiFi.status() == WL_CONNECTED) {
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
                    } else if (response.indexOf("\"/help\"") != -1) {
                        sendTelegramMessage("M5&Yunhat Available commands:\n"
                                           "/status - Get current sensor readings\n"
                                           "/interval [seconds] - Set update interval\n"
                                           "/sleep [mode] - Set sleep mode (0=light/1=none/2=deep)\n"
                                           "/restart - Restart device\n"
                                           "/help - Show this help message");
                    } else if (response.indexOf("\"/sleep") != -1) {
                        // Extract the mode after /sleep
                        int cmdPos = response.indexOf("\"/sleep");
                        int spacePos = response.indexOf(" ", cmdPos);
                        if (spacePos > 0) {
                            int valuePos = spacePos + 1;
                            int endPos = response.indexOf("\"", valuePos);
                            if (endPos > valuePos) {
                                String modeStr = response.substring(valuePos, endPos);
                                
                                if (modeStr.equalsIgnoreCase("light")) {
                                    sleepMode = 0;
                                    sendTelegramMessage("Sleep mode set to light sleep");
                                } else if (modeStr.equalsIgnoreCase("none")) {
                                    sleepMode = 1;
                                    sendTelegramMessage("Sleep mode set to no sleep (continuous operation)");
                                } else if (modeStr.equalsIgnoreCase("deep")) {
                                    sleepMode = 2;
                                    sendTelegramMessage("⚠️ WARNING: Deep sleep mode will shut down device completely until next update interval.\nDevice will wake, send data, then sleep again.");
                                } else {
                                    sendTelegramMessage("Invalid sleep mode. Use 'light', 'none', or 'deep'");
                                }
                            }
                        } else {
                            // If no parameter, just report current mode
                            String currentMode = sleepMode == 0 ? "light sleep" : (sleepMode == 1 ? "no sleep" : "deep sleep");
                            String modeMsg = "Current sleep mode: " + currentMode;
                            sendTelegramMessage(modeMsg.c_str());
                        }
                    } else if (response.indexOf("\"/restart\"") != -1) {
                        sendTelegramMessage("Restarting device...");
                        delay(1000);
                        esp_restart();
                    } else if (response.indexOf("\"/interval") != -1) {
                        // Extract the number after /interval
                        int cmdPos = response.indexOf("\"/interval");
                        int spacePos = response.indexOf(" ", cmdPos);
                        if (spacePos > 0) {
                            int valuePos = spacePos + 1;
                            int endPos = response.indexOf("\"", valuePos);
                            if (endPos > valuePos) {
                                String valueStr = response.substring(valuePos, endPos);
                                int newInterval = valueStr.toInt();
                                if (newInterval >= 5 && newInterval <= 3600) {
                                    sleepTime = newInterval;
                                    char msg[50];
                                    snprintf(msg, sizeof(msg), "Update interval set to %d seconds", newInterval);
                                    sendTelegramMessage(msg);
                                } else {
                                    sendTelegramMessage("Invalid interval. Please use a value between 5 and 3600 seconds.");
                                }
                            }
                        }
                    }
                }
            }
        }
        http.end();
    }
}

void sendStatusUpdate() {
    char message[300]; // Increased buffer size
    int length = snprintf(message, sizeof(message), 
             "📊 Status Report:\n"
             "Temperature: %.2f°C\n"
             "Humidity: %.2f%%\n"
             "Pressure: %.2f Pa (%.2f hPa)\n"
             "Light: %d\n"
             "Acceleration: X=%.2f Y=%.2f Z=%.2f\n"
             "Gyro: X=%.2f Y=%.2f Z=%.2f\n"
             "Sleep mode: %s\n"
             "Update interval: %lu seconds\n"
             "Next update in: %lu seconds", 
             tmp, hum, pressure, pressure/100.0, light, 
             accX, accY, accZ, gyroX, gyroY, gyroZ, 
             sleepMode == 0 ? "Light" : (sleepMode == 1 ? "None" : "Deep"),
             sleepTime,
             (sleepTime - ((millis() - lastSensorUpdate) / 1000)));
    
    // Check if message was truncated
    if (length >= sizeof(message)) {
        Serial.println("Warning: Status message was truncated");
    }
    
    sendTelegramMessage(message);
}

void updateLed() {
    led_set_all((brightness << 16) | (brightness << 8) | brightness);
}

void readSensors() {
    sensorReadError = false;
    
    // Add a small delay before starting I2C communications
    delay(20);
    
    // Read temperature and humidity if SHT20 is enabled
    if (sht20_enabled) {
        tmp = sht20.read_temperature();
        if (handleI2CError(tmp, "temperature")) {
            i2c_failures++;
            // Check if we've had too many failures
            if (i2c_failures > MAX_I2C_FAILURES) {
                Serial.println("Too many I2C failures, resetting I2C bus");
                resetI2C();
                i2c_failures = 0;
            }
            delay(100); // Add delay after error
        } else {
            // Only try to read humidity if temperature succeeded
            delay(50); // Add delay between I2C operations
            hum = sht20.read_humidity();
            if (handleI2CError(hum, "humidity")) {
                i2c_failures++;
            } else {
                i2c_failures = 0; // Reset counter on success
            }
        }
        delay(50); // Add delay between different sensors
    }
    
    // Read light sensor
    light = light_get();
    delay(50);
    
    // Read pressure sensor if BMP280 is enabled
    if (bmp280_enabled) {
        pressure = bmp.readPressure();
        if (handleI2CError(pressure, "pressure")) {
            i2c_failures++;
        } else {
            i2c_failures = 0;
        }
        delay(50);
    }
    
    // Read IMU data - separate different IMU reads
    M5.IMU.getAccelData(&accX, &accY, &accZ);
    delay(20);
    M5.IMU.getGyroData(&gyroX, &gyroY, &gyroZ);
    
    // Log data to serial
    Serial.printf("AX:%.2f,AY:%.2f,AZ:%.2f,GX:%.2f,GY:%.2f,GZ:%.2f,T:%.2fC,H:%.2f,P:%.2f,L:%d,Errors:%d\n",
        accX, accY, accZ, gyroX, gyroY, gyroZ, tmp, hum, pressure, light, i2c_failures);
}

void checkAlerts() {
    // Temperature alert
    if (tmp > tempThreshold) {
        sendTelegramMessage("🔥 Temperature too high! Over 32°C!");
    }

    // Humidity alert
    if (hum < humThreshold) {
        sendTelegramMessage("💧 Humidity too low! Below 70%!");
    }
    
    // Pressure alert - Changed threshold to be reasonable
    // Normal atmospheric pressure is around 101325 Pa (1013.25 hPa)
    if (pressure > pressureThreshold) {
        sendTelegramMessage("🌪️ Pressure too high! Over 1000 hPa!");
    }

    // Sudden movement alert
    if (abs(accX) > axisXThreshold || abs(accY) > axisYThreshold || abs(accZ) > axisZThreshold) {
        sendTelegramMessage("⚠️ Sudden movement detected!");
    }
}

bool handleI2CError(float value, const char* sensorName, bool allowZero) {
    // Modified to accept zero values for IMU data since they can legitimately be zero
    if (isnan(value) || (!allowZero && value == 0)) {
        Serial.print("Error reading ");
        Serial.println(sensorName);
        sensorReadError = true;
        return true;
    }
    return false;
}

void resetI2C() {
    // Complete I2C bus reset
    Serial.println("Resetting I2C bus...");
    
    // End current I2C communication
    Wire.end();
    delay(100);
    
    // Pull both lines high for stability
    pinMode(0, OUTPUT);
    pinMode(26, OUTPUT);
    digitalWrite(0, HIGH);
    digitalWrite(26, HIGH);
    delay(500);
    
    // Reinitialize with slower clock
    Wire.begin(0, 26, 10000UL); // Very slow clock for maximum compatibility
    delay(500);
}

bool initSensors() {
    bool success = true;
    
    // Initialize IMU
    if (!M5.IMU.Init()) {
        Serial.println("IMU initialization failed");
        success = false;
    }
    delay(200);
    
    // Initialize BMP280
    if (!bmp.begin(0x76)) {
        Serial.println(F("Could not find a valid BMP280 sensor"));
        success = false;
    } else {
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                      Adafruit_BMP280::SAMPLING_X2,
                      Adafruit_BMP280::SAMPLING_X16,
                      Adafruit_BMP280::FILTER_X16,
                      Adafruit_BMP280::STANDBY_MS_1000);
    }
    delay(200);
    
    // Try reading SHT20 to verify
    float testTemp = sht20.read_temperature();
    if (isnan(testTemp)) {
        Serial.println("SHT20 initialization failed");
        success = false;
    }
    
    return success;
}