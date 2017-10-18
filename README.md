### IotaWatt 2.2

See the [WiKi](https://github.com/boblemaire/IoTaWatt/wiki) for detailed information concerning installation and use of IoTaWatt.

![IotaWatt version 4](http://iotawatt.com/Images/IotaWattincase.jpg)

IotaWatt is an open-hardware/open-source project to produce an accurate, low-cost, multi-channel, and easy to use electric power monitor.  It's based on the ESP8266 IoT platform using MCP3208 12 bit ADCs to sample voltage and current at high sample rates.

The version 4 hardware supports 14 power monitoring channels and uses a nodeMCU esp8266. That version is being designed as a turnkey unit preloaded with firmware and contained in a wall mountable enclosure.

There is also ongoing development to adapt the ESP32.

The software samples input channels at a rate of 35-40 channels per second, and records voltage(V), power(Watts), and energy(kWh) to the local SD card every five seconds.  The Emoncms browser based graphing utility can be used on the local WiFi network to view the data.

In addition, the data can be uploaded to an Emoncms server or an influxDB server. In the event of WiFi or internet service disruptions, the IotaWatt will continue to log locally and bulk update the server when WiFi is restored.

The basic configuration uses one voltage reference channel and fourteen current sense channels for monitoring up to fourteen single phase circuits.  Current sense channels may be used, with slight modification, as additional voltage/phase reference channels to allow monitoring of polyphase power systems.

The configuration and monitoring app runs on any browser based device that is connected to the local WiFi network. There is a demo version at [iotawatt.com](http://iotawatt.com)

Thanks to contributors of other open software that have been incorporated into this project.  In particular the Emoncms, ArduinoJSON and SDWebServer software made a lot of this possible, not to mention the Arduino/ESP8266 project and related forums.

The WiKi is a work in progress and provides details concerning installation and use.
