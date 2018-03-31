### IotaWatt 4.8

See the [WiKi](https://github.com/boblemaire/IoTaWatt/wiki) for detailed information concerning installation and use of IoTaWatt. 

IoTaWatt forum is [here](https://community.iotawatt.com/)

![IotaWatt version 4.8](http://iotawatt.com/Images/IoTaWatt_new_case.JPG)

IotaWatt is an open-hardware/open-source project to produce an accurate, low-cost, multi-channel, and easy to use electric power monitor.  It's based on the ESP8266 IoT platform using MCP3208 12 bit ADCs to sample voltage and current at high sample rates. As a datalogger the device has a real-time clock and SDcard and supports data querry with the integrated web server.

* monitors up to 14 circuits  
* Accuracy typically within 1%  
* Local SDcard stores up to 15 years worth of data.
* REST APIs to extract data
* Supports many different make, model and capacity current transformers  
* Supports generic definition of any current transformer  
* Three-phase capable  
* Local LAN browser based configuration  
* Local LAN browser based status display  
* Local LAN browser based graphing analytic tools  
* Open Hardware/Software 
* Upload to influxDB   
* Upload to [Emoncms.org](https://emoncms.org/)  
* Demo configuration app at [IoTaWatt.com](http://iotawatt.com)  

The software samples input channels at a rate of 35-40 channels per second, recording voltage(V), power(Watts), and energy(kWh) to the local SD card every five seconds.  The Emoncms browser based graphing utility can be used on the local WiFi network to view data or the data can be uploaded to an Emoncms server or an influxDB server. In the event of WiFi or internet service disruptions, the IotaWatt will continue to log locally and bulk update the server when WiFi is restored.

The basic configuration uses one voltage reference channel and fourteen current sense channels for monitoring up to fourteen single phase circuits.  Current sense channels may be used, with slight modification, as additional voltage/phase reference channels to allow monitoring of three-phase power systems.  There is also an option to estimate three phase power using a single voltage/phase reference.

The configuration and monitoring app runs on any browser based device that is connected to the local WiFi network. A demo is available at [iotawatt.com](http://iotawatt.com)

Thanks to contributors of other open software that have been incorporated into this project.  In particular the Emoncms, ArduinoJSON and SDWebServer software made a lot of this possible, not to mention the Arduino/ESP8266 project and related forums.

The [WiKi](https://github.com/boblemaire/IoTaWatt/wiki) is a work in progress and provides details of installation and use.
