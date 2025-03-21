#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "hw_mic.h"
#include "config.h"
#include <esp_camera.h>
#include <esp_heap_caps.h>


// Microphone and noise detection
int32_t mic_samples[1600];

// Image buffers
uint8_t* imageBuffers[MAX_IMAGES]; // Array to hold image buffers
size_t imageSizes[MAX_IMAGES];     // Array to hold image sizes

// Function prototypes
void sendTelegramMessage(const char* message);
void sendTelegramPhoto(const uint8_t* buffer, size_t length);
void sendTelegramPhotosAsGroup(std::vector<std::pair<const uint8_t*, size_t>> images);
int captureImages(int numImages);
void checkTelegramMessages();

// Wi-Fi and HTTP clients
WiFiClient espClient;
HTTPClient http;

void setup() {
  // Initialize serial communication
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
    config.fb_count = 1;
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
  unsigned int num_samples = 1600;
  hw_mic_read(mic_samples, &num_samples);

  // Calculate RMS (Root Mean Square) value
  float sumSquares = 0.0;
  for (int i = 0; i < num_samples; i++) {
      sumSquares += (float)mic_samples[i] * mic_samples[i];
  }
  float rms = sqrt(sumSquares / num_samples);

  // Normalize the RMS value to fit into a decibel scale
  float referenceValue = 32768.0; // Assuming 16-bit microphone
  float noiseLevel_dB = 20 * log10(rms / referenceValue);
  Serial.printf("Noise level: %.2f dB\n", noiseLevel_dB);

  // Send alert if noise exceeds threshold
  if (noiseLevel_dB > noiseThreshold && millis() - lastAlertTime > alertCooldown) {
    sendTelegramMessage("🚨 Noise detected in baby's room! 🚨");
    sendTelegramMessage(("Current noise level: " + String(noiseLevel_dB) + " dB").c_str());
    lastAlertTime = millis();
    // Capture and send photo
    // camera_fb_t * fb = esp_camera_fb_get();
    // if(fb && fb->len <= 15 * 1024) {
    //   sendTelegramPhoto(fb->buf, fb->len);
    // } else {
    //   Serial.println("img too large");
    // } 
    // if (fb) esp_camera_fb_return(fb);
    // // Update the last alert time
    // lastAlertTime = millis();
    // Capture 10 images
    int numCaptured = captureImages(MAX_IMAGES);
    if (numCaptured > 0) {
      std::vector<std::pair<const uint8_t*, size_t>> images;
      for (int i = 0; i < numCaptured; i++) {
        images.push_back({imageBuffers[i], imageSizes[i]});
      }
      sendTelegramPhotosAsGroup(images);
    } else {
      Serial.println("❌ No images captured.");
    }
  checkTelegramMessages();
  }
  delay(500); // Cooldown period
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
  String url = "https://api.telegram.org/bot" + String(botToken) + "/getUpdates?offset=-1";
  http.begin(url);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Telegram response: " + response);

      if (response.indexOf("ALERT") != -1 ||
      response.indexOf("Temperature too high!") != -1 || 
      response.indexOf("Humidity too low!") != -1 || 
      response.indexOf("Pressure too high!") != -1 || 
      response.indexOf("Sudden movement detected!") != -1) {  // All keywords for alerts
          Serial.println("🚨 ALERT detected! Capturing image...");

          // Capture image
          camera_fb_t *fb = esp_camera_fb_get();
          if (!fb) {
              Serial.println("Failed to capture image");
              return;
          }

          // Send the captured image to Telegram
          sendTelegramPhoto(fb->buf, fb->len);
          esp_camera_fb_return(fb);
      }
  } else {
      Serial.print("Error checking messages: ");
      Serial.println(httpResponseCode);
  }

  http.end();
}
