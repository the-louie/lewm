#include <WiFi.h>
#include <PubSubClient.h>

#include "config.hpp" // Load config
#include "utils.hpp"  // Some misc functions
#include "netfunc.hpp" // Pinging and resolving hosts
#include "wifi.hpp"    // Connecting and reconnecting to wifi
#include "mqtt.hpp"    // Reconnecting and sending mqtt messages

bool firstMsg = true;
long messageInterval = 300000; // 5 minutes
long lastMsg = 0;

int rtt_gw_ms;

unsigned long reconnectPrevMs = millis();
unsigned long reconnectInterval = 3600000; // 1 hour

String current_timestamp;

void setup()
{
    Serial.begin(115200);
    while(!Serial){delay(100);}

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
    }
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
    Serial.print("Setting up NTP");
    timeClient.begin();
    timeClient.setTimeOffset(3600);
    while(!timeClient.isTimeSet()) {
      Serial.print(".");
      timeClient.forceUpdate();
    }
    Serial.println("success");

    // Connect to MQTT server
    Serial.print("Setting up MQTT...");
    mqttClient.setServer(mqtt_server, 1883);
    Serial.println("done");
    mqttReconnect();
}

void loop(){
  unsigned long now = millis();
  if (!mqttClient.connected()) {
    Serial.println("MQTT disconnected");
    mqttReconnect();
  }
  mqttClient.loop();

  // Disconnect from the wifi  periodically to allow for reconnection with alternative base station
  if ((WiFi.status() == WL_CONNECTED) && (now - reconnectPrevMs > reconnectInterval)) {
    Serial.print("Disconnecting Wifi...");
    WiFi.disconnect();
    Serial.println("Done");
    sleep(1);
    reconnectPrevMs = now;
  }

  // Every now and then do the measurements and send them off
  if ((WiFi.status() == WL_CONNECTED) && (firstMsg || (now - lastMsg > messageInterval))) {
    // Use NTP to get the time of day
    timeClient.update();
    current_timestamp = getDateTime();
    Serial.print(current_timestamp); Serial.print("\t");

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

    // Build string
    char bssidBuf[20] = {0};
    bssid.toCharArray(bssidBuf, 20);

    // Build hostname string
    char hostnameBuf[28] = {0};
    hostname.toCharArray(hostnameBuf, 28);

    // Build a topic strings
    char topicRttBuf[128] = {0};
    snprintf(topicRttBuf, 128, "lewm/%s/%s/RTT", hostnameBuf, bssidBuf);
    char topicRssiBuf[128] = {0};
    snprintf(topicRssiBuf, 128, "lewm/%s/%s/RSSI", hostnameBuf, bssidBuf);

    char mqttMsRttgBuf[8] = {0}; 
    snprintf(mqttMsRttgBuf, 8, "%d", rtt_gw_ms);
    char mqttMsgRssiBuf[8] = {0}; 
    snprintf(mqttMsgRssiBuf, 8, "%d", rssi);

    // Display some debug info
    Serial.print("Publishing to ");
    Serial.print(topicRttBuf);
    Serial.print(": '");
    Serial.print(mqttMsRttgBuf);
    Serial.print("'...");

    // Publish the measurements to the MQTT broker
    bool successRtt = mqttClient.publish(topicRttBuf, mqttMsRttgBuf);
    if (successRtt) {
      Serial.println("Success");
    } else {
      Serial.println("Fail");
    }

    // Display some debug info
    Serial.print("Publishing to ");
    Serial.print(topicRssiBuf);
    Serial.print(": '");
    Serial.print(mqttMsgRssiBuf);
    Serial.print("'...");

    // Publish the measurements to the MQTT broker
    bool successRssi = mqttClient.publish(topicRssiBuf, mqttMsgRssiBuf);
    if (successRssi) {
      Serial.println("Success");
    } else {
      Serial.println("Fail");
    }

    firstMsg = false;
    lastMsg = now;
  }
}