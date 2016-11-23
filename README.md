# IoTaWatt
Internet of Things appliance Watt monitor

This project is a 14 channel ESP8266 electric power monitor.
It measures average watts over a periodic interval, records the 
data on a local SDcard and uploads via WiFi.
This is a work in progress.

* Runs on 5v micro USB power to NodeMCU 0.9 or 1.0 ESP8266 board.
* Samples power at rate of 30,000+ samples-per-second.
* Uses external current transformers (CT)s.
* Reference voltage using 9vAC external brick.
* Connection to eMonCMS.org server.
* Programmed in C++ using Arduino IDE.

The source code is comprised of seven .ino files that were in a single library in
my Arduino IDE.  Trying not to spend a lot of time learning how GitHub works,
so already i've screwed up and uploaded seven individual files.  How to put them all 
in a single library is not obvious to me right now.  There is one simple class that
I threw together to allow some limited use of the MCP23S17 GPIO chip, I'll upload that 
when I learn how.  What it does is pretty obvious in the code.  In addition to the 
ESP variants of standard classes, I've used: SdFat (allows long filenames), 
WiFiUDP (because it was there), and the very powerful ArduinoJson by Benoît Blanchon
which makes handling the Json Config and Table files extremely easy.

To do:

* Configure via WiFi.
* Log to the SD.
* Calibration with AC cord adapter.
* CT calibration project ongoing.
* Support for connection to other similar power monitoring servers.
