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

To do:

* Configure via WiFi.
* Calibration with AC cord adapter.
* CT calibration project ongoing.
* Support for connection to other similar power monitoring servers.
