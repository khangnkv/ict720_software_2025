# ict720_software_2025

# Group BabyCare+
.
# Objective
  The objective of this project is to develop a baby monitoring system using the M5StickC and HAT Yun device, which detects motion and environmental changes and sends real-time notifications/alerts to parents through a Telegram bot. The system will ensure that parents are promptly informed when their baby wakes up or moves and will provide environmental data like temperature, humidity, and air pressure from the HAT Yun sensor. Additionally, the system will send an image from the T-SIMCAM ESP32-S3 when motion is detected, giving parents a complete view of the room’s current situation. This solution aims to provide peace of mind to parents by offering real-time alerts and live environmental monitoring in an efficient and timely manner.

# Member
Khang Vinh Khac Nguyen 6722040661 \
Tanagrid Rakwongrit 6714552220 \
Waritthorn Na Nagara 6722040281

# Stakeholder
1. Parents
2. Babysitter

# User Stories
1. As a parent, I want to know when my baby needs care at night, but I need rest too, so I want to sleep without worry
2. As a parent, I want to check my babysitter whether he/she take care of my child properly during my working hours, for I am not available at home, and it is risky if the babysitter does something wrong without notifying
3. As a parent, I want to review recorded video and audio from the camera to ensure my child was properly cared for throughout the day.
4. As a babysitter, I want to be alerted if the baby’s room temperature, noise level, or abnormal motion are happening so I use T-SIMCAM ESP32-S3 for motion detection and using M5StickC for the other abnormalities detection which alert me


# Software Models
1. State Diagram
2. System Architecture
   ![System Architecture](https://github.com/khangnkv/ict720_software_2025/blob/main/System%20Architecutre.png)
    1. Motion, Temperature, and Noise Tracking
       - M5 Capsule: Attached to the baby crib to collect surround temperature, noise, and motion data.
       - ESP32 LilyCam: Attached in the room at the angle where the children is visible within the crib
       - Wi-Fi Transmission: Send all data collected to the server (both local and cloud)
       - Data Storage and Analysis: Detect irregular movement, surrounding temperature, or noise in the room.
   2. Remote Monitoring & Manual Control
       - Cloud Dashboard/App: allow programmers to see admin data in details and summarized infromation for parents
       - Manual Appliances Control: user can turn on/off camera and devices through app.
   3. Safety & Energy Efficiency:
       - Hardware turns off at the scheduled setting (turn off day to conserve energy)
       - If anything abnormal is detected while baby is monitored, the app will send a Telegram Notificaiton alert along with images/videos recorded after trigger
3. Sequence Diagram
4. Overview
5. Connectivity
6. Data Modeling
  ![Baby Chatbot Modeling](https://github.com/khangnkv/ict720_software_2025/blob/main/Baby%20Telegram%20Chatbot.png)
7. Telegram Bot Alert
