String bssid;
String hostname;
IPAddress gw_ip;
IPAddress local_ip;


char node_hostname[255];  // Used in the MQTT code also

void setHostname() {
    hostname = String("lewm-" + WiFi.macAddress());
    hostname.replace(":", "");
    char * cptr = &node_hostname[0];
    FillCstring(cptr, hostname);  // FIXME: Is this needed?
    WiFi.setHostname(node_hostname);

    // Build char array string
    hostname.toCharArray(hostnameBuf, 28);

    Serial.print("Using hostname ");
    Serial.println(hostname);
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  bssid = WiFi.BSSIDstr();
  bssid.toCharArray(bssidBuf, 20);
  Serial.print("Reconnected to BSSID: ");
  Serial.println(bssid);
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
  last_disconnect_reason = String(info.wifi_sta_disconnected.reason);
  delay(100);
  Serial.println("Trying to Reconnect");
  WiFi.begin(ssid, password);

}
