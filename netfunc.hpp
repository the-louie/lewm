#include <ESP32Ping.h>


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
