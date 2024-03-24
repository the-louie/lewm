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




    Serial.print("Setting up NTP");
    timeClient.begin();
    timeClient.setTimeOffset(3600);
    while(!timeClient.isTimeSet()) {
      Serial.print(".");
      timeClient.forceUpdate();
    }
    Serial.println("success");


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

    // Build a JSON string
    char mqttMsgBuf[256] = {0};
    char bssidBuf[20] = {0};
    bssid.toCharArray(bssidBuf, 20);

    snprintf(mqttMsgBuf, 255, "{ \"bssid\": \"%s\", \"rtt_gw_ms\": %d, \"rssi\": %d }", bssidBuf, rtt_gw_ms, rssi);

    // Build a topic string
    char hostnameBuf[20] = {0};
    char topicBuf[28] = {0};
    hostname.toCharArray(hostnameBuf, 20);
    snprintf(topicBuf, 28, "lewm/%s", hostnameBuf);

    // Display some debug info
    Serial.print("Publishing to ");
    Serial.print(topicBuf);
    Serial.print(": '");
    Serial.print(mqttMsgBuf);
    Serial.print("'...");

    // Publish the measurements to the MQTT broker
    bool success = mqttClient.publish(topicBuf, mqttMsgBuf);
    if (success) {
      Serial.println("Success");
    } else {
      Serial.println("Fail");
    }
    firstMsg = false;
    lastMsg = now;
  }
}