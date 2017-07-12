

### IotaWatt 2.1

See the [WiKi](https://github.com/boblemaire/IoTaWatt/wiki) for detailed information concerning installation and use of IoTaWatt.

IotaWatt is an open-hardware/open-source project to produce an accurate, low-cost, multi-channel, and easy to use electric power monitor.  It's based on the ESP8266 IoT platform using MCP3208 12 bit ADCs to sample voltage and current at high sample rates.

The hardware in development support2 14 power monitoring channels and uses a nodeMCU esp8266. That version is being designed to be more of a turnkey unit preloaded with firmware and contained in a wall mountable enclosure.

There is also ongoing development to adapt the ESP32.

The software samples input channels at a rate of 35-40 channels per second, and records voltage(V), power(Watts), and energy(kWh) to the local SD card every five seconds.  The eMonCMS browser based graphing utility can be used on the local WiFi network to view the data.

In addition, the data can be uploaded to the eMonCMS.org server. In the event of WiFi or internet service disruptions, the IotaWatt will continue to log locally and bulk update eMonCMS.org when service is restored.

Any input channel may be used to monitor either voltage or current (watts), however the 14 power channel version requires removing the burden resistors and some minimal external circuitry (2 resistors and a capacitor) to sense voltage on a power channel.  The software requires no changes.

The configuration and monitoring app runs on any browser based device that is connected to the local WiFi network. There is a demo version at [iotawatt.com](http://iotawatt.com)

Thanks to contributors of other open software that have been incorporated into this project.  In particular the ArduinoJSON and SDWebServer software made a lot of this possible, not to mention the Arduino/ESP8266 project and related forums.

The WiKi is a work in progress and goes into detail concerning installation and use.
