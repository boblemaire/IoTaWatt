

### IotaWatt 1.3

IotaWatt is an open-hardware/open-source project to produce an accurate, low-cost, multi-channel, and easy to use electric power monitor.  It's based on the ESP8266 IoT platform using MCP3208 12 bit ADCs to sample voltage and current at high sample rates. 

One version described here with schematics and PCB layout has been fabricated and several are running well in residential installations.
The board supports either version 0.9 or 1.0 of the NodeMCU open development ESP8266 board running the Arduino/ESP8266 SDK platform.

The newer PCB and schematic set represents the next iteration of development. It is based on the Adafruit "feather" series and uses their ESP8266 feather board stacked on their feather datalogger board with real-time-clock and SDcard.  The IotaWatt ADC board described in this git's Eagle files stacks under the Adafruit boards.  Each ADC board has up to seven inputs, and up to three can be stacked for a total of 7, 14 or 21 input channels.

The software samples input channels at a rate of 35-40 channels per second, and records voltage(V), frequency(Hz), power(Watts), energy(kWh) and power-factor to the local SD card every five seconds.  The eMonCMS browser based graphing utility can be used on the local WiFi network to view the data.

In addition, the data can be uploaded to the eMonCMS.org server at any interval that is a multiple of 5 seconds. In the event of WiFi or internet service disruptions, the IotaWatt will bulk update eMonCMS.org when service is restored.

Any input channel may be used to monitor either voltage or current (watts), however each IotaWatt ADC "feather" board has one auxiliary 5mm barrel plug with associated circuitry that is "voltage ready".  With three ADC boards, the voltage of all three legs of a three-phase power installation can be monitored to get true power on all three legs.

The ADC boards are selectable between 3.2V or 1.2V reference voltage to support either 1V type current transformers (like the popular YHDC STC013-000) or the better 33mv CTs (like the DENT CTHMC-100).  Voltage is individually selectable for each ADC in the stack, and each has a voltage reference to insure accuracy without the need for additional calibration.

Thanks to contributors of other open software that have been incorporated into this project.  In particular the ArduinoJSON and SDWebServer software made a lot of this possible, not to mention the Arduino/ESP8266 project and related forums.
