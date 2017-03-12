

### IotaWatt 2.0

IotaWatt is an open-hardware/open-source project to produce an accurate, low-cost, multi-channel, and easy to use electric power monitor.  It's based on the ESP8266 IoT platform using MCP3208 12 bit ADCs to sample voltage and current at high sample rates.

The newer PCB and schematic set contained here are the next iteration of development. It is based on the Adafruit "feather" series and uses their ESP8266 feather board stacked on their feather datalogger board with real-time-clock and SDcard.  The IotaWatt ADC board described in this git's Eagle files stacks under the Adafruit boards.  Each ADC board has up to seven inputs, and up to three can be stacked for a total of 7, 14 or 21 input channels.

The software is also running on older versions of hardware based on the NodeMCU vesion 0.9 and 1.0 commodity development MCUs.  Those processors can still be used instead of the Adafruit board so long as the appropriate electrical connections are adapted.

The software samples input channels at a rate of 35-40 channels per second, and records voltage(V), power(Watts), and energy(kWh) to the local SD card every five seconds.  The eMonCMS browser based graphing utility can be used on the local WiFi network to view the data.

In addition, the data can be uploaded to the eMonCMS.org server. In the event of WiFi or internet service disruptions, the IotaWatt will continue to log locally and bulk update eMonCMS.org when service is restored.

Any input channel may be used to monitor either voltage or current (watts), however each IotaWatt ADC "feather" board has one auxiliary 5mm barrel plug with associated circuitry that is "voltage ready".  With three ADC boards, the voltage of all three legs of a three-phase power installation can be monitored to get true power on all three legs.

The ADC boards are selectable between 3.2V or 1.2V reference voltage to support either 1V type current transformers (like the popular YHDC STC013-000) or the better 33mv CTs (like the DENT CTHMC-100).  Voltage is individually selectable for each ADC in the stack, and each has a voltage reference to insure accuracy without the need for additional calibration.

The configuration and monitoring app runs on any browser based device that is connected to the local WiFi network. 

Thanks to contributors of other open software that have been incorporated into this project.  In particular the ArduinoJSON and SDWebServer software made a lot of this possible, not to mention the Arduino/ESP8266 project and related forums.

The WiKi is a work in progress and goes into detail concerning installation and use.
