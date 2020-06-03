import paho.mqtt.client as paho
import time
import matplotlib.pyplot as plt
import numpy as np
import math
# https://os.mbed.com/teams/mqtt/wiki/Using-MQTT#python-client

# MQTT broker hosted on local machine
mqttc = paho.Client()

# Settings for connection

host = "localhost"
topic = "velocity"
t = [] 
X = []
Y = []
Z = []
samplecount=0

# Callbacks
def on_connect(self, mosq, obj, rc):
      print("Connected rc: " + str(rc))

def on_message(mosq, obj, msg):
    print("[Received] Topic: " + msg.topic + ", Message: " + str(msg.payload) + "\n")
    global samplecount,X,Y,Z,t
    a=str(msg.payload,encoding = "utf-8")
    result= float(a[1:])
    if 'E' in a:
        print("End")
        mqttc.disconnect()
    elif 'S' in a:
        samplecount=result
    elif 'X' in a:
        X.append(result)
    elif 'Y' in a:
        Y.append(result)
    elif 'Z' in a:
        Z.append(result)
    elif 'T' in a:
        t.append(result)

    #print(result)
    
  
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

# Loop forever, receiving messages
mqttc.loop_forever()

tilt=[]
#calculating for volocity
for i in range(0,int(samplecount)):
    vX = vX+X[i]*i
    vY = vY+Y[i]*i
    vH = math.sqrt(vX**2+vY**2)      
'''      
    X_angle = math.atan(X[i] / (math.sqrt(Y[i]**2 + Z[i]**2)))
    Y_angle = math.atan(Y[i] / (math.sqrt(X[i]**2 + Z[i]**2)))
    Z_angle = math.atan((math.sqrt(X[i]**2 + Y[i]**2)) / Z[i])
'''

'''
    Pitch = X_angle * 180 / math.pi
    Roll = Y_angle * 180 / math.pi
    Yaw = Z_angle * 180 / math.pi
'''   
'''  
    if ( Pitch >= 45.0 or Roll >= 45.0 or Yaw >= 45.0 ) :
        tilt.append(1)
    else :
        tilt.append(0)
'''
fig, ax = plt.subplots(2, 1)
ax[0].plot(t,X,color="blue", linewidth=1.0, linestyle="-",label="x-acc")
ax[0].plot(t,Y,color="red", linewidth=1.0, linestyle="-",label="y-acc")
ax[0].plot(t,Z,color="green", linewidth=1.0, linestyle="-",label="z-acc")
ax[0].legend(loc='lower right')
ax[0].set_xlabel('Time')
ax[0].set_ylabel('Acc Vector')

ax[1].stem(t,vH) 
ax[1].set_xlabel('Time')
ax[1].set_ylabel('vH')
plt.show()