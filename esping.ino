#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Ping.h>
#include <PubSubClient.h>

// Load config
#include "config.hpp"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

WiFiClient espClient;
PubSubClient mqttClient(espClient);

bool firstMsg = true;
long messageInterval = 300000; // 5 minutes
long lastMsg = 0;
char msg[50];
int value = 0;

String bssid;
String hostname;
char node_hostname[255];
int rtt_gw_ms;
int rtt_sunet_ms;
unsigned long dns_resolve_ms;

unsigned long reconnectPrevMs = millis();
unsigned long reconnectInterval = 3600000; // 1 hour

String current_timestamp;

IPAddress ping_sunet_ip;

void mqttReconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(node_hostname)) {
      Serial.print("connected as ");
      Serial.println(hostname);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void FillCstring( char * &cstr, String TheString) {
 TheString.toCharArray(cstr, TheString.length()+1);
}

String getDateTime() {
  int hour = timeClient.getHours();
  String hourStr = String(hour);
  if (hour < 10) {
    hourStr = String("0") + hourStr;
  }
  int minute = timeClient.getMinutes();
  String minuteStr = String(minute);
  if (minute < 10) {
    minuteStr = String("0") + minuteStr;
  }
  int second = timeClient.getSeconds();
  String secondStr = String(second);
  if (second < 10) {
    secondStr = String("0") + secondStr;
  }

  time_t epochTime = timeClient.getEpochTime();

  //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime);

  int monthDay = ptm->tm_mday;
  String monthDayStr = String(monthDay);
  if (monthDay < 9) {
    monthDayStr = String("0") + String(monthDayStr);
  }

  int currentMonth = ptm->tm_mon+1;
  String currentMonthStr = String(currentMonth);
  if (currentMonth < 10) {
    currentMonthStr = String("0") + String(currentMonthStr);
  }

  int currentYear = ptm->tm_year+1900;

  return String(currentYear) + "-" + currentMonthStr + "-" + monthDayStr + " " + hourStr + ":" + minuteStr + ":" + secondStr;
}

void randStr(char* str, uint len) {
  for(uint i = 0; i < len; i++ ) {
    str[i] = random(0, 25) + 'a';
  }
}

int pingHost(IPAddress ip_address) {
  bool success = Ping.ping(ip_address, 5);
  if (!success) {
    return -1;
  } else {
    int avgTime = round(Ping.averageTime());
    if (avgTime > 9999) {
      return -1;
    }
    return avgTime;
  }
}

void array_to_string(byte array[], unsigned int len, char buffer[])
{
    for (unsigned int i = 0; i < len; i++)
    {
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;
        buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
        buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    }
    buffer[len*2] = '\0';
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  bssid = WiFi.BSSIDstr();
  Serial.print("BSSID: ");
  Serial.println(bssid);
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
  Serial.println("Trying to Reconnect");
  WiFi.begin(ssid, password);
}

unsigned long dnsResolveRandom() {
    char subdomain[16] = {0};
    char maindomain[16] = ".any.phetast.nu";
    char fulldomain[sizeof(subdomain) + sizeof(maindomain) + 1] = {0};
    randStr(subdomain, sizeof(subdomain));
    strncpy(fulldomain, subdomain, sizeof(subdomain));
    strncat(fulldomain, maindomain, sizeof(maindomain));
    IPAddress temp_ip;
    unsigned long t0_dns = millis();
    WiFi.hostByName(fulldomain, temp_ip);
    unsigned long t_dns = millis() - t0_dns;
    return t_dns;
}


void setup()
{
    Serial.begin(115200);
    while(!Serial){delay(100);}

    Serial.println();
    Serial.println("******************************************************");

    // Set and print hostname
    String mac = WiFi.macAddress();
    hostname = String("lewm-" + mac);
    hostname.replace(":", "");
    char * cptr = &node_hostname[0];
    WiFi.setHostname(node_hostname);
    FillCstring(cptr, hostname);
    Serial.print("Using hostname ");
    Serial.println(hostname);



    Serial.print("Connecting to ");
    Serial.print(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
    Serial.println("connected");


    Serial.print("Local IP: ");
    Serial.print(WiFi.localIP());
    Serial.print("\tGateway IP: ");
    Serial.println(WiFi.gatewayIP());


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
    Serial.print(current_timestamp);
    Serial.print("\t");

    // Resolve a random domain name
    dns_resolve_ms = dnsResolveRandom();
    Serial.print("DNS: ");
    Serial.print(dns_resolve_ms);
    Serial.print("ms\t");


    // Get the WIFI RSSI
    int8_t rssi = WiFi.RSSI();
    Serial.print("RSSI: ");
    Serial.print(rssi);
    Serial.print("dB\t");

    // Ping the local gateway
    Serial.print("GW (");
    Serial.print(WiFi.gatewayIP());
    Serial.print("): ");
    rtt_gw_ms = pingHost(WiFi.gatewayIP());
    if (rtt_gw_ms >= 0) {
      Serial.print(rtt_gw_ms);
      Serial.print("ms");
    } else {
      Serial.print("ERROR");
    }
    Serial.print("\t");


    // Ping the Sunet server
    WiFi.hostByName("ping.sunet.se", ping_sunet_ip);
    Serial.print("Sunet (");
    Serial.print(ping_sunet_ip);
    Serial.print("): ");
    rtt_sunet_ms = pingHost(ping_sunet_ip);
    if (rtt_sunet_ms >= 0) {
      Serial.print(rtt_sunet_ms);
      Serial.print("ms");
    } else {
      Serial.print("ERROR");
    }
    Serial.println(".");


    // Build a JSON string
    char mqttMsgBuf[256] = {0};
    char bssidBuf[20] = {0};
    bssid.toCharArray(bssidBuf, 20);
    char timestampBuf[20] = {0};
    current_timestamp.toCharArray(timestampBuf, 20);
    snprintf(mqttMsgBuf, 255, "{ \"timestamp\": \"%s\", \"bssid\": \"%s\", \"rtt_gw_ms\": %d, \"rtt_sunet_ms\": %d, \"dns_query_ms\": %d, \"rssi\": %d }", timestampBuf, bssidBuf, rtt_gw_ms, rtt_sunet_ms, rssi, dns_resolve_ms);

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
      Serial.println("Done");
    } else {
      Serial.println("Fail.");
    }
    firstMsg = false;
    lastMsg = now;
  }
}