#include <WiFi.h>
#include <PubSubClient.h>

String last_disconnect_reason = "First connection after waking up."; // Communicate the disconnetc reason back so we can put in to mqtt
char hostnameBuf[28] = {0};
char bssidBuf[20] = {0};

#include "config.hpp" // Load config
#include "utils.hpp"  // Some misc functions
#include "netfunc.hpp" // Pinging and resolving hosts
#include "wifi.hpp"    // Connecting and reconnecting to wifi
#include "mqtt.hpp"    // Reconnecting and sending mqtt messages

long wifiInitialConnectTimeout = 60000; // If we don't get a wifi connection withing this time we should reboot

bool firstMsg = true;
long messageInterval = 10000; // 10 seconds
long messageLastSent = 0;

long timeUpdateLast = 0;
long timeUpdateInterval = 21600000; // 6 hours

int rtt_gw_ms;

unsigned long reconnectPrevMs = millis();
unsigned long reconnectInterval = 600000; // 10 minutes

char topicRttBuf[128] = {0};
char topicRssiBuf[128] = {0};

long timeLastConnected = 0;

void setup()
{
    Serial.begin(115200);
    // while(!Serial){delay(100);}
    delay(500); // Wait a while for everything to calm down.

    Serial.println();
    Serial.println("******************************************************");

    // Set and print hostname
    setHostname();

    // Connect to WIFI
    WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
    Serial.print("Connecting to ");
    Serial.print(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if (millis() > wifiInitialConnectTimeout) {
          ESP.restart();
        }
    }
    timeLastConnected = millis();
    Serial.println("connected");
    bssid = WiFi.BSSIDstr();
    gw_ip = WiFi.gatewayIP();
    local_ip = WiFi.localIP();
    Serial.print("Local IP: ");
    Serial.print(local_ip);
    Serial.print("\tGateway IP: ");
    Serial.print(gw_ip);
    Serial.print("\tBSSID: ");
    Serial.println(bssid);

    // Setup NTP to keep the time
    // Serial.print("Setting up NTP");
    // timeClient.begin();
    // timeClient.setTimeOffset(3600);
    // while(!timeClient.isTimeSet()) {
    //   Serial.print(".");
    //   timeClient.forceUpdate();
    // }
    // Serial.println("success");

    // Connect to MQTT server
    Serial.print("Setting up MQTT...");
    mqttClient.setServer(mqtt_server, 1883);
    Serial.println("done");
    mqttReconnect();
}

void loop(){
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(String(now - timeLastConnected));
    if (now - timeLastConnected > wifiInitialConnectTimeout) {
      ESP.restart();
    }
  } else {
    timeLastConnected = millis();
    // if (now - timeUpdateLast > timeUpdateInterval) {
    //   // Use NTP to get the time of day
    //   timeClient.update();
    //   timeUpdateLast = millis();
    // }

    if (!mqttClient.connected()) { 
      Serial.println("MQTT disconnected");
      mqttReconnect();

    } else { 

      if (last_disconnect_reason != "") {
        Serial.print("Last disconnect reason: ");
        Serial.println(last_disconnect_reason);
        
        char topic[128] = {0};
        snprintf(topic, 128, "lewm/%s/status/reconnect_reason", hostnameBuf);
        Serial.println(topic);
        char message[255] = {0};
        snprintf(message, 255, "%s", last_disconnect_reason);
        mqttClient.publish(topic, message);
        last_disconnect_reason = "";
      }


      // Every now and then do the measurements and send them off
      if (firstMsg || (now - messageLastSent > messageInterval)) {
        // Get the WIFI RSSI
        int8_t rssi = WiFi.RSSI();
        Serial.print("RSSI: "); Serial.print(rssi); Serial.print("dB\t");

        // Ping the local gateway
        Serial.print("GW ("); Serial.print(gw_ip); Serial.print("): ");
        rtt_gw_ms = pingHost(gw_ip);
        if (rtt_gw_ms >= 0) {
          Serial.print(rtt_gw_ms); Serial.print("ms");
        } else {
          Serial.print("ERROR");
        }
        Serial.print("\t-\t");

        char mqttMsRttgBuf[8] = {0}; 
        snprintf(mqttMsRttgBuf, 8, "%d", rtt_gw_ms);
        char mqttMsgRssiBuf[8] = {0}; 
        snprintf(mqttMsgRssiBuf, 8, "%d", rssi);

        // Build a topic strings
        snprintf(topicRttBuf, 128, "lewm/%s/%s/RTT", hostnameBuf, bssidBuf);
        snprintf(topicRssiBuf, 128, "lewm/%s/%s/RSSI", hostnameBuf, bssidBuf);
        
        // Display some debug info
        Serial.print("Publishing to ");
        Serial.print(topicRttBuf);
        Serial.print(": '");
        Serial.print(mqttMsRttgBuf);
        Serial.print("'...");

        // Publish the measurements to the MQTT broker
        bool successRtt = mqttClient.publish(topicRttBuf, mqttMsRttgBuf);
        if (successRtt) {
          Serial.print("Success");
        } else {
          Serial.print("Fail");
        }

        // Display some debug info
        Serial.print("\tPublishing to ");
        Serial.print(topicRssiBuf);
        Serial.print(": '");
        Serial.print(mqttMsgRssiBuf);
        Serial.print("'...");

        // Publish the measurements to the MQTT broker
        bool successRssi = mqttClient.publish(topicRssiBuf, mqttMsgRssiBuf);
        if (successRssi) {
          Serial.print("Success");
        } else {
          Serial.print("Fail");
        }

        Serial.println("");

        firstMsg = false;
        messageLastSent = now;
      }
    }

    if (now - reconnectPrevMs > reconnectInterval) {
      // Disconnect from the wifi periodically to allow for reconnection with alternative base station
      // Rely on automatic reconnection based on the registered events in setup()
      Serial.print("Disconnecting Wifi...");
      WiFi.disconnect();
      Serial.println("Done");
      sleep(100);
      reconnectPrevMs = now;
    }
  }
}