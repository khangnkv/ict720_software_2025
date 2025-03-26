import paho.mqtt.client as mqtt
from datetime import datetime
import urllib3
import json
import os
import sys
import sqlite3
from pymongo import MongoClient

# initialize environment variables
mongo_uri = os.getenv('MONGO_URI', None)
mongo_db = os.getenv('MONGO_DB', None)
mongo_col_device = os.getenv('MONGO_COL_DEV', None)
mqtt_broker = os.getenv('MQTT_BROKER', None)
mqtt_port = os.getenv('MQTT_PORT', None)
mqtt_topic = os.getenv('MQTT_TOPIC', None)
if mongo_uri is None or mqtt_broker is None or mqtt_port is None or mqtt_topic is None:
    print('MONGO_URI and MQTT settings are required')
    sys.exit(1)

# initialize app
mongo_client = MongoClient(mongo_uri)

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected with result code {reason_code}")
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe(mqtt_topic + "#")

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    print(msg.topic+" "+str(msg.payload))
    
    # Parse the JSON payload
    try:
        data = json.loads(msg.payload)
        
        # Handle alert count messages
        if msg.topic.split('/')[-1] == "alertcount":
            timestamp = data.get('timestamp')
            alert_count = data.get('alert_count')
            
            # Store in SQLite
            c.execute('''INSERT INTO babyMonitor 
                        (timestamp, alert_count, message_type) 
                        VALUES (?, ?, ?)''', 
                        (datetime.now().isoformat(), alert_count, 'alert'))
            conn.commit()
            
            # Store in MongoDB
            db = mongo_client[mongo_db]
            mongo_collection = db['alerts']
            mongo_collection.insert_one({
                'timestamp': datetime.now().isoformat(),
                'alert_count': alert_count,
                'message_type': 'alert'
            })
            print(mongo_collection.count_documents({}))
            print("Inserted to MongoDB")
        
        # Handle noise data messages
        elif msg.topic.split('/')[-1] == "noise":
            timestamp = data.get('timestamp')
            noise_level = data.get('noise')
            
            # Store in SQLite
            c.execute('''INSERT INTO babyMonitor 
                        (timestamp, noise_level, message_type) 
                        VALUES (?, ?, ?)''', 
                        (datetime.now().isoformat(), noise_level, 'noise'))
            conn.commit()
        
        # Handle alert flag messages from M5StickC
        elif msg.topic.split('/')[-1] == "alertflag":
            # Store in SQLite
            c.execute('''INSERT INTO babyMonitor 
                        (timestamp, message_type) 
                        VALUES (?, ?)''', 
                        (datetime.now().isoformat(), 'alert_flag'))
            conn.commit()

            # Store in MongoDB
            db = mongo_client[mongo_db]
            mongo_collection = db['noise']
            mongo_collection.insert_one({
                'timestamp': datetime.now().isoformat(),
                'noise_level': noise_level,
                'message_type': 'noise'
            })
            print(mongo_collection.count_documents({}))
            print("Inserted to MongoDB")
         
    
    except Exception as e:
        print(f"Error processing message: {e}")

# init SQLite
conn = sqlite3.connect('babyMonitor.db')
c = conn.cursor()
c.execute('''CREATE TABLE IF NOT EXISTS babyMonitor (
          _id INTEGER PRIMARY KEY AUTOINCREMENT,
          timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
          alert_count INTEGER,
          noise_level FLOAT,
          message_type TEXT
          )''')
conn.commit()

# init MQTT
mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.on_connect = on_connect
mqttc.on_message = on_message

mqttc.connect(mqtt_broker, int(mqtt_port), 60)

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
mqttc.loop_forever()