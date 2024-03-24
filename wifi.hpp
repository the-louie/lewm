String bssid;
String hostname;
IPAddress gw_ip;
IPAddress local_ip;

char node_hostname[255];  // Used in the MQTT code also

void setHostname() {
    String mac = WiFi.macAddress();
    hostname = String("lewm-" + mac);
    hostname.replace(":", "");
    char * cptr = &node_hostname[0];
    FillCstring(cptr, hostname);  // FIXME: Is this needed?
    WiFi.setHostname(node_hostname);
    Serial.print("Using hostname ");
    Serial.println(hostname);
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  bssid = WiFi.BSSIDstr();
  Serial.print("Reconnected to BSSID: ");
  Serial.println(bssid);
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
  Serial.println("Trying to Reconnect");
  WiFi.begin(ssid, password);
}
