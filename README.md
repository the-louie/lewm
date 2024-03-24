# LEWM - Louies ESP Wireless Monitor
Super basic code (in the earlist of stages) for pinging the local gateway, checking RSSI and reporting it
via MQTT.
The idea is to only test the wireless link between the client and the Wifi-AP, other tests is better performed
by other hosts in the network.

# Documentation

## IP and Hostname
DHCP is used to retrive an IP-address and the local GW-address. The hostname is `lewm-<MAC-address>`.

## Configure
All configuring is done by copying `config-example.hpp` to `config.hpp` and updating the values with your own.
There is only three values, the WIFI name and password, and the IP-address to the MQTT server, all values should
be strings.

## Output
The output is sent to the MQTT-server as a JSON-object, the topic is `lewm/<hostname>`, and the format is:
```
{ "bssid": "BB:SS:11:DD:00:00", "rtt_gw_ms": 2, "rssi": -60 }
```

# TODO
* Clean the code up and refactor it inte "readable" chunks.
* Add a configuration mode where the node acts as an AP and you can configure it so we don't need to hardcode stuff.