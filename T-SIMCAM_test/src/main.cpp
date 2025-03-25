#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "hw_mic.h"
#include "config.h"
#include <esp_camera.h>
#include <esp_heap_caps.h>

// Microphone and noise detection
int32_t mic_samples[MIC_SAMPLE_COUNT];
unsigned int num_samples = MIC_SAMPLE_COUNT;

// Image buffers
uint8_t* imageBuffers[MAX_IMAGES]; // Array to hold image buffers
size_t imageSizes[MAX_IMAGES];     // Array to hold image sizes

// Function prototypes
void sendTelegramMessage(const char* message);
void sendTelegramPhoto(const uint8_t* buffer, size_t length);
void sendTelegramPhotosAsGroup(std::vector<std::pair<const uint8_t*, size_t>> images);
int captureImages(int numImages);
void checkTelegramMessages();
float calculateNoiseLevel(int32_t* samples, unsigned int num_samples);
void sendNoiseAlert(float noiseLevel_dB);

// Wi-Fi and HTTP clients
WiFiClient espClient;
HTTPClient http;

// Store last processed update ID
long lastUpdateId = 0; 

// Alert cooldown state
bool alertCooldownEnabled = true; 

void setup() {
  // Initialize serial communication
  Serial.begin(baud_rate);
  Serial.println("Starting Baby Monitor...");
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    // Print dots while we wait to connect
    delay(500);
    Serial.print(".");
    attempts++;
  }
  // Check if we connected successfully
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi");
  } else {
    Serial.println("\nFailed to connect to Wi-Fi");
    return;
  }

  // Initialize microphone
  hw_mic_init(MIC_SAMPLE_RATE);

  // Initialize camera
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

  // Camera initialization with resized image
  if (psramFound()) {
    Serial.println("PSRAM found. Using higher resolution and quality settings.");
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;           
    config.fb_count = 2;

  } else {
    Serial.println("PSRAM not found. Using lower resolution and quality settings.");
    config.frame_size = FRAMESIZE_QVGA; // Use QVGA (320x240) if PSRAM is unavailable
    config.jpeg_quality = 20;           // Lower quality for smaller file size
    config.fb_count = 1;                // Single frame buffer
  }

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  } else {
    Serial.println("Camera initialized successfully");
  }
}

void loop() {
  num_samples = MIC_SAMPLE_COUNT;
  hw_mic_read(mic_samples, &num_samples);

  // Calculate and display noise level
  float noiseLevel_dB = calculateNoiseLevel(mic_samples, num_samples);
  Serial.printf("Noise level: %.2f dB\n", noiseLevel_dB);

  // Send alert if noise exceeds threshold
  if (noiseLevel_dB > noiseThreshold) {
    if (!alertCooldownEnabled || millis() - lastAlertTime > alertCooldown) {
      sendNoiseAlert(noiseLevel_dB);
    } else {
      Serial.println("Noise detected, but alert cooldown active.");
    }
  }

  // Check for messages from Telegram
  checkTelegramMessages();

  delay(loopDelay); // Cooldown period
}

// Calculate noise level in decibels from microphone samples
float calculateNoiseLevel(int32_t* samples, unsigned int num_samples) {
  // Calculate RMS (Root Mean Square) value
  float sumSquares = 0.0;
  for (int i = 0; i < num_samples; i++) {
    sumSquares += (float)samples[i] * samples[i];
  }
  float rms = sqrt(sumSquares / num_samples);

  // Normalize the RMS value to fit into a decibel scale
  float referenceValue = 32768.0; // Assuming 16-bit microphone
  float noiseLevel_dB = 20 * log10(rms / referenceValue);
  
  return noiseLevel_dB;
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

void sendTelegramPhoto(const uint8_t* buffer, size_t length) {
  if (WiFi.status() == WL_CONNECTED) {
    String url = "https://api.telegram.org/bot" + String(botToken) + "/sendPhoto";
    http.begin(url);

    String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    String bodyStart = "--" + boundary + "\r\n";
    bodyStart += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + String(chatID) + "\r\n";
    bodyStart += "--" + boundary + "\r\n";
    bodyStart += "Content-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\n";
    bodyStart += "Content-Type: image/jpeg\r\n\r\n";

    String bodyEnd = "\r\n--" + boundary + "--\r\n";

    size_t totalLength = bodyStart.length() + length + bodyEnd.length();
    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    http.addHeader("Content-Length", String(totalLength));

    // Create the full payload
    uint8_t* payload = new uint8_t[totalLength];
    memcpy(payload, bodyStart.c_str(), bodyStart.length());
    memcpy(payload + bodyStart.length(), buffer, length);
    memcpy(payload + bodyStart.length() + length, bodyEnd.c_str(), bodyEnd.length());

    // Send the payload
    int httpResponseCode = http.POST(payload, totalLength);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Photo sent: " + response);
    } else {
      Serial.print("Error sending photo: ");
      Serial.println(httpResponseCode);
    }

    // Clean up
    delete[] payload;
    http.end();
  } else {
    Serial.println("Wi-Fi not connected!");
  }

}
void sendTelegramPhotosAsGroup(std::vector<std::pair<const uint8_t*, size_t>> images) {
  if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Wi-Fi not connected!");
      return;
  }

  HTTPClient http;
  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  String url = "https://api.telegram.org/bot" + String(botToken) + "/sendMediaGroup";
  http.begin(url);
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

  // **Step 1: Create JSON Metadata**
  String mediaJson = "[";
  for (size_t i = 0; i < images.size(); i++) {
      if (i > 0) mediaJson += ",";
      mediaJson += "{ \"type\": \"photo\", \"media\": \"attach://photo" + String(i) + "\" }";
  }
  mediaJson += "]";

  // **Step 2: Build Multipart Request Body**
  String requestData = "--" + boundary + "\r\n";
  requestData += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
  requestData += String(chatID) + "\r\n";

  requestData += "--" + boundary + "\r\n";
  requestData += "Content-Disposition: form-data; name=\"media\"\r\n\r\n";
  requestData += mediaJson + "\r\n";

  // **Convert requestData to byte vector**
  std::vector<uint8_t> postData(requestData.begin(), requestData.end());

  // **Step 3: Attach Images**
  for (size_t i = 0; i < images.size(); i++) {
      String header = "--" + boundary + "\r\n";
      header += "Content-Disposition: form-data; name=\"photo" + String(i) + "\"; filename=\"photo.jpg\"\r\n";
      header += "Content-Type: image/jpeg\r\n\r\n";

      // Append header
      postData.insert(postData.end(), header.begin(), header.end());

      // Append image data
      postData.insert(postData.end(), images[i].first, images[i].first + images[i].second);

      // Append newline after image
      postData.insert(postData.end(), "\r\n", "\r\n" + 2);
  }

  // **Step 4: Close the multipart request**
  String closing = "--" + boundary + "--\r\n";
  postData.insert(postData.end(), closing.begin(), closing.end());

  // **Step 5: Send Request**
  Serial.println("Sending request to Telegram...");
  int httpResponseCode = http.POST(postData.data(), postData.size());

  if (httpResponseCode > 0) {
      Serial.println("Photo group sent: " + http.getString());
  } else {
      Serial.print("Error sending photo group: ");
      Serial.println(httpResponseCode);
  }

  http.end();
}


// Capture `numImages` images and store them in buffers
int captureImages(int numImages) {
  int capturedCount = 0;

  for (int i = 0; i < numImages; i++) {
      // Delay to allow camera to get a new frame (adjust as needed)
      delay(500);  

      camera_fb_t* fb = esp_camera_fb_get();
      if (!fb) {
          Serial.println("Failed to capture image: " + String(i));
          continue;
      }

      Serial.println("Captured image " + String(i) + " - Size: " + String(fb->len) + " bytes");

      // Allocate memory and copy image data
      imageBuffers[i] = (uint8_t*)malloc(fb->len);
      if (imageBuffers[i] == NULL) {  
          Serial.println("Memory allocation failed for image " + String(i));
          esp_camera_fb_return(fb);
          continue;
      }

      memcpy(imageBuffers[i], fb->buf, fb->len);
      imageSizes[i] = fb->len;
      capturedCount++;

      esp_camera_fb_return(fb);  // Free camera buffer
  }

  Serial.println("Captured " + String(capturedCount) + " images.");
  return capturedCount;
}

// Free allocated image buffers after sending
void freeImageBuffers(int numImages) {
  for (int i = 0; i < numImages; i++) {
      if (imageBuffers[i] != NULL) {
          free(imageBuffers[i]);
          imageBuffers[i] = NULL;
      }
  }
  Serial.println("Freed all image buffers.");
}

void checkTelegramMessages() {
  // Use offset to only get new messages (offset = lastUpdateId + 1)
  String url = "https://api.telegram.org/bot" + String(botToken) + "/getUpdates?offset=" + String(lastUpdateId + 1);
  http.begin(url);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
      String response = http.getString();
      
      // Check if there are any updates in the response
      if (response.indexOf("\"ok\":true") != -1 && response.indexOf("\"result\":[{") != -1) {
          // Extract update_id using string operations
          int updateIdPos = response.indexOf("\"update_id\":");
          if (updateIdPos != -1) {
              // Find the position of the update_id value
              updateIdPos += 12; // Length of "update_id":
              int updateIdEnd = response.indexOf(",", updateIdPos);
              if (updateIdEnd != -1) {
                  // Extract and convert the update_id to a long integer
                  String updateIdStr = response.substring(updateIdPos, updateIdEnd);
                  long currentUpdateId = updateIdStr.toInt();
                  
                  // Update our last processed ID
                  lastUpdateId = currentUpdateId;
                  
                  // Extract the message text - look for "text":"..."
                  int textPos = response.indexOf("\"text\":\"");
                  if (textPos != -1) {
                      textPos += 8; // Length of "text":"
                      int textEnd = response.indexOf("\"", textPos);
                      if (textEnd != -1) {
                          String messageText = response.substring(textPos, textEnd);
                          Serial.println("Received message: " + messageText);
                          
                          // Process command if it starts with "/"
                          if (messageText.length() > 0 && messageText.charAt(0) == '/') {
                              // Extract command (remove the "/" and get the command word)
                              String command = messageText.substring(1);
                              command.toLowerCase(); // Convert to lowercase for case-insensitive matching
                              
                              Serial.println("Processing command: " + command);
                              
                              // Process different commands
                              if (command.startsWith("alert")) {
                                // Check if it's a subcommand for alert cooldown
                                if (command.indexOf("cooldown") != -1) {
                                    if (command.indexOf("off") != -1) {
                                        alertCooldownEnabled = false;
                                        sendTelegramMessage("Alert cooldown disabled. Will notify on every noise detection.");
                                        Serial.println("Alert cooldown disabled by user");
                                        return;
                                    } else if (command.indexOf("on") != -1) {
                                        alertCooldownEnabled = true;
                                        sendTelegramMessage("Alert cooldown enabled. Will respect cooldown period between alerts.");
                                        Serial.println("Alert cooldown enabled by user");
                                        return;
                                    } else if (command.indexOf("status") != -1) {
                                        String status = alertCooldownEnabled ? "enabled" : "disabled";
                                        sendTelegramMessage(("Alert cooldown is currently " + status).c_str());
                                        return;
                                    }
                                }
                                
                                // Regular alert command (existing functionality)
                                Serial.println("🚨 ALERT command received! Capturing images...");
                                int numCaptured = captureImages(MAX_IMAGES);
                                if (numCaptured > 0) {
                                    std::vector<std::pair<const uint8_t*, size_t>> images;
                                    for (int i = 0; i < numCaptured; i++) {
                                        images.push_back({imageBuffers[i], imageSizes[i]});
                                    }
                                    
                                    sendTelegramPhotosAsGroup(images);
                                    
                                    // Free memory after sending
                                    freeImageBuffers(numCaptured);
                                } else {
                                    Serial.println("❌ No images captured.");
                                    sendTelegramMessage("Sorry, failed to capture images");
                                }
                             } 
                             else if (command == "photo" || command == "picture") {
                                  Serial.println("Capturing single image...");
                                  camera_fb_t* fb = esp_camera_fb_get();
                                  if (!fb) {
                                      Serial.println("Failed to capture image");
                                      sendTelegramMessage("Sorry, failed to capture image");
                                  } else {
                                      Serial.println("Captured image - Size: " + String(fb->len) + " bytes");
                                      sendTelegramPhoto(fb->buf, fb->len);
                                      esp_camera_fb_return(fb);  // Free camera buffer
                                  }
                              }
                              else if (command == "photos" || command == "pictures") {
                                  Serial.println("Capturing multiple images...");
                                  int numCaptured = captureImages(MAX_IMAGES);
                                  if (numCaptured > 0) {
                                      std::vector<std::pair<const uint8_t*, size_t>> images;
                                      for (int i = 0; i < numCaptured; i++) {
                                          images.push_back({imageBuffers[i], imageSizes[i]});
                                      }
                                      
                                      sendTelegramPhotosAsGroup(images);
                                      
                                      // Free memory after sending
                                      freeImageBuffers(numCaptured);
                                  } else {
                                      Serial.println("❌ No images captured.");
                                      sendTelegramMessage("Sorry, failed to capture images");
                                  }
                              }
                              else if (command == "noise" || command == "sound") {
                                  num_samples = MIC_SAMPLE_COUNT;
                                  hw_mic_read(mic_samples, &num_samples);
                                  float noiseLevel_dB = calculateNoiseLevel(mic_samples, num_samples);
                                  sendTelegramMessage(("Current noise level: " + String(noiseLevel_dB) + " dB").c_str());
                              }
                              else if (command == "help") {
                                  // Send help message with available commands
                                  String helpMsg = "Available commands:\n";
                                  helpMsg += "/photo - Take a single photo\n";
                                  helpMsg += "/photos - Take multiple photos\n";
                                  helpMsg += "/alert - Trigger alert mode\n";
                                  helpMsg += "/alert cooldown on - Enable alert cooldown\n";
                                  helpMsg += "/alert cooldown off - Disable alert cooldown\n";
                                  helpMsg += "/alert cooldown status - Check cooldown status\n";
                                  helpMsg += "/noise - Check current noise level\n";
                                  helpMsg += "/help - Show this help message";
                                  sendTelegramMessage(helpMsg.c_str());
                              }
                              else {
                                  sendTelegramMessage("Unknown command. Type /help for available commands.");
                              }
                          }
                      }
                  }
              }
          }
      }
  } else {
      Serial.print("Error checking messages: ");
      Serial.println(httpResponseCode);
  }

  http.end();
}

// Send alert with noise level and capture/send images
void sendNoiseAlert(float noiseLevel_dB) {
  sendTelegramMessage("🚨 Noise detected in baby's room! 🚨");
  sendTelegramMessage(("Current noise level: " + String(noiseLevel_dB) + " dB").c_str());
  lastAlertTime = millis();

  // Capture 10 images
  int numCaptured = captureImages(MAX_IMAGES);
  if (numCaptured > 0) {
    std::vector<std::pair<const uint8_t*, size_t>> images;
    for (int i = 0; i < numCaptured; i++) {
      images.push_back({imageBuffers[i], imageSizes[i]});
    }
    sendTelegramPhotosAsGroup(images);
    
    // Free memory after sending
    freeImageBuffers(MAX_IMAGES);
  } else {
    Serial.println("❌ No images captured.");
  }
}