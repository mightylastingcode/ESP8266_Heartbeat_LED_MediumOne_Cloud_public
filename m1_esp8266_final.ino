/***************************************************************************
  This is an example of program for connected the Adafruit Huzzah to the 
  Medium One Prototyping Sandbox.  Visit www.medium.one for more information.
  Author: Medium One
  Last Revision Date: May 1, 2018

  v1 : 7/10/2019 Modify LED control
                 Add sampling rate change code.
 ***************************************************************************/

/*
 *   IDE: Arduino IDE 1.89
 *   Board File: ESP8266 by Adafruit v2.3.0 (Adafruit Huzzad ESP8266)
 */

 
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <sscanf_esp8266.h>  // not implemented in esp8266 core library
                             // library provided by  Opsycon AB

/*
// ************** ENTER THE FOLLOWING CONFIGURATION FOR CODE TO WORK ***************
// Medium One Prototyping Sandbox Device Credentials
char server[] = "mqtt.mediumone.com";
int port = 61620;
char pub_topic[]="0/<Project MQTT ID>/<User MQTT>/device/";
char sub_topic[]="1/<Project MQTT ID>/<User MQTT>/device/event";
char mqtt_username[]="<Project MQTT ID>/<User MQTT>";
char mqtt_password[]="<API Key>/<User Password>";

// Wifi credentials
char WIFI_SSID[] = "<WIFI_SSID>";
char WIFI_PASSWORD[] = "<WIFI_SSID>";
// ************** ENTER THE FOLLOWING CONFIGURATION FOR CODE TO WORK ***************
*/


// Medium One Prototyping Sandbox Device Credentials
char server[] = "mqtt.mediumone.com";
int port = 61620;

// device_esp8266_1
char pub_topic[]="0/<Project MQTT ID>/<User MQTT>/esp8266/";
char sub_topic[]="1/<Project MQTT ID>/<User MQTT>/esp8266/event";
char mqtt_username[]="<Project MQTT ID>/<User MQTT>";
char mqtt_password[]="<API Key>/<User Password>";

// Wifi credentials
char WIFI_SSID[] = "<WIFI_SSID>";
char WIFI_PASSWORD[] = "<WIFI_SSID>";



// set pin for LED
//const int LED_PIN = 2; // Adafruit ESP8266 Feather Board
const int LED_PIN = 0;   // Adafruit ESP8266 Module

// ongoing timer counter for heartbeat
static int heartbeat_timer = 0;

// ongoing timer counter for sensor
static int sensor_timer = 0;

// set heartbeat period in milliseconds
static int heartbeat_period = 60000;

// set sensor transmit period in milliseconds
static int sensor_period = 5000;

// track time when last connection error occurs
long lastReconnectAttempt = 0;

// wifi client with security
WiFiClientSecure wifiClient; 

void setup() {
  
  // init uart
  Serial.begin(9600);
  while(!Serial){}

  // wifi setup
  WiFi.mode(WIFI_STA);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  //not sure this is needed
  delay(5000);
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("Failed to connect, resetting"));
    ESP.reset();
  }
  
  // if you get here you have connected to the WiFi
  Serial.println(F("Connected to Wifi!"));

  Serial.println(F("Init hardware LED"));
  pinMode(LED_PIN, OUTPUT);
 
  // connect to MQTT broker to send board reset msg
  connectMQTT();

  Serial.println("Complete Setup");
}

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  int i = 0;
  uint32_t temp = 0;
  
  char message_buff[length + 1];
  for(i=0; i < length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
  
  Serial.print(F("Received some data: "));
  Serial.println(String(message_buff));
  Serial.print(F("Received data len: "));
  Serial.println(strlen(message_buff));
  // Process message to turn LED on and off
  
  // L2:0 led on 
  // L2:1 led off
  if (message_buff[0] == 'L')
  {
    Serial.println(F("Request: Change RED Led state."));
    if (strlen(message_buff) == 4) 
    {
      if (String(message_buff[3]) == "0") { 
        // Turn off LED
        digitalWrite(LED_PIN, HIGH);
      } else if (String(message_buff[3]) == "1") { 
        // Turn on LED
        digitalWrite(LED_PIN, LOW);
      }    
    }    
  } 
  else if (message_buff[0] == 'S') 
  {
    Serial.println(F("Request: Change the sampling rate."));
    sscanf(&message_buff[1],"%d", &temp);
    heartbeat_period = temp * 10;  // multiply by 10 for ms clock.
    Serial.print(F("New sampling interval time is "));
    Serial.print(heartbeat_period);
    Serial.println(F(" ms"));
  } 
  else 
  {
    Serial.println(F("Unknown request from the cloud app."));
    }
  
  
}

PubSubClient client(server, port, callback, wifiClient);

boolean connectMQTT()
{    
  // Important Note: MQTT requires a unique id (UUID), we are using the mqtt_username as the unique ID
  // Besure to create a new device ID if you are deploying multiple devices.
  // Learn more about Medium One's self regisration option on docs.mediumone.com
  if (client.connect((char*) mqtt_username,(char*) mqtt_username, (char*) mqtt_password)) {
    Serial.println(F("Connected to MQTT broker"));

    // send a connect message
    if (client.publish((char*) pub_topic, "{\"event_data\":{\"connected\":true}, \"add_client_ip\":true}")) {
      Serial.println("Publish connected message ok");
    } else {
      Serial.print(F("Publish connected message failed: "));
      Serial.println(String(client.state()));
    }

    // subscrive to MQTT topic
    if (client.subscribe((char *)sub_topic,1)){
      Serial.println(F("Successfully subscribed"));
    } else {
      Serial.print(F("Subscribed failed: "));
      Serial.println(String(client.state()));
    }
  } else {
    Serial.println(F("MQTT connect failed"));
    Serial.println(F("Will reset and try again..."));
    abort();
  }
  return client.connected();
}

void loop() {
  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 1000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (connectMQTT()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    client.loop();
  }
  heartbeat_loop();
}

void heartbeat_loop() {
  if ((millis()- heartbeat_timer) > heartbeat_period) {
    heartbeat_timer = millis();
    String payload = "{\"event_data\":{\"millis\":";
    payload += millis();
    payload += ",\"heartbeat\":true}}";
    
    if (client.loop()){
      Serial.print(F("Sending heartbeat: "));
      Serial.println(payload);
  
      if (client.publish((char *) pub_topic, (char*) payload.c_str()) ) {
        Serial.println(F("Publish ok"));
      } else {
        Serial.print(F("Failed to publish heartbeat: "));
        Serial.println(String(client.state()));
      }
    }
  }
}