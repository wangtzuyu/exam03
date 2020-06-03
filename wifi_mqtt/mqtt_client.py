import paho.mqtt.client as paho
import matplotlib.pyplot as plt
import numpy as np
import time

t = np.zeros(60) # time vector;
y_x = np.zeros(60) # signal vector; 
y_y = np.zeros(60) # signal vector; 
y_z = np.zeros(60) # signal vector;
input_string = "input_string"
num = 0
# MQTT broker hosted on local machine
mqttc = paho.Client()

# Settings for connection
# TODO: revise host to your ip
host = "192.168.1.117" 
topic= "Mbed"

# Callbacks
def on_connect(self, mosq, obj, rc):
    print("Connected rc: " + str(rc))

def on_message(mosq, obj, msg):
    print("[Received] Topic: " + msg.topic + ", Message: " + msg.payload.decode() + "\n")
    input_string = msg.payload.decode(encoding='UTF-8')
    x = input_string.split("#",5)
    print(x[0]+" "+x[1]+" "+x[2]+" "+x[3]+" "+x[4]+" ")
    num = int(x[0])-1
    t[num]   = float(x[1])
    y_x[num] = float(x[2])
    y_y[num] = float(x[3])
    y_z[num] = float(x[4])
    if t[num]==float(20):
        print("start plot!")
        fig, ax = plt.subplots(1, 1)
        ax.plot(t[:num],y_x[:num],color="red",linestyle="-", label="x")
        ax.plot(t[:num],y_y[:num],color="blue",linestyle="-", label="y")
        ax.plot(t[:num],y_z[:num],color="green",linestyle="-", label="z")
        ax.legend(loc='upper left', frameon=False)
        ax.set_xlabel('Time')
        ax.set_ylabel('Acc Vector')
        plt.show()

def on_subscribe(mosq, obj, mid, granted_qos):
    print("Subscribed OK")

def on_unsubscribe(mosq, obj, mid, granted_qos):
    print("Unsubscribed OK")

# Set callbacks
mqttc.on_message = on_message
mqttc.on_connect = on_connect
mqttc.on_subscribe = on_subscribe
mqttc.on_unsubscribe = on_unsubscribe
# Connect and subscribe
print("Connecting to " + host + "/" + topic)
mqttc.connect(host, port=1883, keepalive=60)
mqttc.subscribe(topic, 0)
# Loop
mqttc.loop_forever()

