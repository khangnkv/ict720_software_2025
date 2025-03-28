# Group BabyCare+: Baby 

## About the Project
BabyCare+ is a smart baby monitoring system designed to help parents keep track of their baby's activities and environmental conditions in real time. By integrating IoT devices and a Telegram bot, this system ensures that parents receive immediate notifications when their baby moves or when environmental conditions change. It provides a seamless and efficient way to enhance child safety and comfort.

## Objective
  The objective of this project is to develop a baby monitoring system using the **M5StickC** and **HAT Yun** devices, which detect motion and environmental changes and sends real-time notifications/alerts to parents through a **Telegram bot**.
  The system will ensure that parents are promptly informed when their baby wakes up or moves and will provide environmental data like temperature, humidity, and air pressure from the HAT Yun sensor. Additionally, the system will send an image from the T-SIMCAM ESP32-S3 when motion is detected, giving parents a complete view of the room’s current situation. This solution aims to provide peace of mind to parents by offering real-time alerts and live environmental monitoring in an efficient and timely manner.
  
  Key features of the system:
- **Motion Detection:** Notifies parents when the baby moves or wakes up from the **M5StickC**.  
- **Environmental Monitoring:** Provides real-time data on surrounding temperature, noise from the **M5StickC**.
- **Noise Detection:** Notifies parents when the noise is too loud or some weird noise from **T-SIMCAM ESP32-S3**.
- **Image Capture:** Sends an image from the **T-SIMCAM ESP32-S3** when motion, temperature, humidity, air pressure, and noise are detected, giving parents an alert text with information and a visual update of the room’s condition.
- **Real-time monitor:** Parents could use a specific word to get the current situation of the room by typing in the telegram chat.  ex. "ALERT"

## Members

| Name                     | ID          | University |
|--------------------------|------------|------------|
| Khang Vinh Khac Nguyen  | 6722040661 | SIIT       |
| Tanagrid Rakwongrit     | 6714552220 | KU         |
| Waritthorn Na Nagara    | 6722040281 | SIIT       |


## Stakeholder
1. Parents
2. Babysitter

## User Stories
1. As a parent, I want to know when my baby needs care at night while ensuring I get enough rest, so I want to receive alerts without needing to constantly check.
2. As a parent, I want to review recorded images from the camera to verify my child was safe for throughout the day.
3. As a babysitter, I want to be alerted if the baby’s room temperature, noise level, or abnormal motion is detected so I can respond promptly.
4. As a parent, I want to get images of the current situation when in my baby room when something is wrong.


## Hardwares
![M5 Stick C](https://github.com/khangnkv/ict720_software_2025/blob/main/images/messageImage_1742539123663.jpg)\
**↑M5StickC 2019 Edition↑**
[Documentations and Specifications](https://shop.m5stack.com/products/stick-c?variant=43982750843137)
![YUN HAT](https://github.com/khangnkv/ict720_software_2025/blob/main/images/messageImage_1742539212655.jpg)\
**↑M5StickC Yun Hat attachment (SHT20,BMP280,SK6812)↑**
[Documentations and Specifications](https://docs.m5stack.com/en/hat/hat-yun)
![ESP32 LilyCam](https://github.com/khangnkv/ict720_software_2025/blob/main/images/ESP32-S3-camera-board-mPCIe-socket-720x441.jpg)\
**↑T-SIMCAM ESP32-S3 CAM with WiFi and Bluetooth Module↑**
[Specifications]([https://docs.m5stack.com/en/hat/hat-yun](https://lilygo.cc/products/t-simcam))
## Software Models
1. State Diagram
![State Diagram](https://github.com/khangnkv/ict720_software_2025/blob/main/images/state_diagram.png)
2. System Architecture
   ![System Architecture](https://github.com/khangnkv/ict720_software_2025/blob/main/images/system_architecture.png)
    1. Environment and Noise Tracking
       - M5StickC: Detects motion abnormalities
       - Yun Hat: Assists with environmental sensing
       - T-SIMCAM ESP32-S3: Captures images and noise when alerts are triggered or when requested
       - Wi-Fi Transmission: Sends all collected data to the database 
       - Data Storage and Analysis: Detects irregular movement, temperature spikes, noise disturbances, etc.
   2. Manual Control
       - Manual Control: Users can give commands to Telegram ChatBot to take pictures or current environment status
   3. Safety
       - If an abnormality is detected, the system automatically sends an alert with captured images through Telegram
3. Sequence Diagram
![Sequence Diagram](https://github.com/khangnkv/ict720_software_2025/blob/main/images/sequence-diagram.png)
4. Data Flow Process
![Data Flow Diagram](https://github.com/khangnkv/ict720_software_2025/blob/main/images/dataflow.png)
5. Data Modeling
![Data Modeling](https://github.com/khangnkv/ict720_software_2025/blob/main/images/data%20modeling.png)

7. Telegram Bot Alert\
   <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/help_command_test-case.png" alt="/help command test case" width="600" height="400">\
   **↑User can type "/help" in telegram chat↑**
   <br/><br/>
   <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/photo_command_test-case.png" alt="/photo test Case" width="600" height="350">\
   **↑User can type "/photo" in telegram chat↑**
   <br/><br/>
   <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/photos_command_test-case.png" alt="/photos test Case" width="550" height="350">\
   **↑User can type "/photos" in telegram chat↑**
   <br/><br/>
   <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/alert-cooldown-on_command_test-case.png" alt="/alert cooldown on test Case" width="600" height="150">\
   **↑User can type "/alert cooldown on" in telegram chat↑**
   <br/><br/>
   <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/cooldown_active.png" alt="/alert cooldown on test Case" width="600" height="70">\
   **↑Cooldown example↑**
   <br/><br/>
   <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/alert-cooldown-off__command_test-case.png" alt="/alert cooldown off test Case" width="650" height="150">\
   **↑User can type "/alert cooldown off" in telegram chat↑**\
   <br/><br/>
      <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/alert-cooldown-status_command_test-case.png" alt="/alert cooldown status test Case" width="500" height="150">\
   **↑User can type "/alert cooldown status" in telegram chat↑**\
   <br/><br/>
   <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/noise_command_test-case.png" alt="/noise test Case" width="380" height="150">\
   **↑User can type "/noise" in telegram chat↑**
   <br/><br/>
   <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/status_command_test-case.png" alt="/status test Case" width="500" height="350">\
   **↑User can type "/status" in telegram chat↑**\
   <br/><br/>
      <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/dashbpard_command_test-case.png" alt="/dashboard test Case" width="600" height="300">\
   **↑User can type "/dashboard" in telegram chat↑**\
   <br/><br/>
   <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/test_1.png" alt="/dashboard test Case" width="500" height="500">\
   **↑Example Alert in telegram chat↑**\
   <br/><br/>
   <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/pressure_test-case.png" alt="Air Pressure test Case" width="600" height="80">\
   **↑Air pressure alert in telegram chat↑**\
   <br/><br/>
   <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/noise__test-case.png" alt="Noise test Case" width="500" height="500">\
   **↑Noise alert in telegram chat↑**\
   <br/><br/>
   <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/motion_test-case.png" alt="Motion test Case" width="600" height="100">\
   **↑Motion alert in telegram chat↑**\
   <br/><br/>
   <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/test_2.png" alt="Test case" width="600" height="100">\
   **↑Temperature too low↑**\
   <br/><br/>
   <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/humidity_low.png" alt="Humidity low" width="600" height="100">\
   **↑Humidity too low↑**\
   <br/><br/>
   <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/pressure_high.png" alt="Air Pressure low" width="600" height="100">\
   **↑Air pressure too low↑**\
   <br/><br/>
   <img src="https://github.com/khangnkv/ict720_software_2025/blob/main/images/dashboard.jpg" alt="Motion test Case" width="700" height="380">\
   **↑Dashboard example↑**\
   <br/><br/>
9. Conclusion
BabyCare+ is designed to provide parents and caregivers with a smarter, more efficient way to monitor a baby’s safety and well-being. By integrating IoT technology, real-time data collection, and an intuitive Telegram chatbot, the system ensures that parents receive immediate alerts about movement, environmental changes, and unusual noise levels. This solution provides peace of mind by allowing parents to stay informed without constant manual checks.

With its ability to analyze motion, temperature, humidity, and other environmental factors, BabyCare+ provides a comprehensive monitoring experience tailored to the needs of first-time parents and babysitters. By combining automation, real-time alerts, and remote access, this project contributes to a safer and more convenient parenting experience, ultimately improving the quality of life for both parents and their children.
