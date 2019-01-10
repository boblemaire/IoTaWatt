===========
Message Log
===========
The message log is the IoTaWatt's Diary. 
It makes notes about various events that occur, 
both ordinary and unusual. These messages can be helpful in 
understanding what is happening with the device, 
or to provide insight into why something doesn't 
appear to be working as it should.

The primary method of accessed the message log is 
by hovering over the Tools main menu button and clicking 
the Message Log button. 
The config utility will cause your browser to download 
and display the most recent 10000 characters in the log. 
You can access the entire log by using the File Manager 
app to download the file /iotawatt/iotamsgs.txt and 
viewing with a text file editor.

With the exception of the first messages after startup, 
all of the messages in the log will have a date and time stamp. 
Messages issued by one of the IoTaWatt Services will begin 
with the name of the service followed by a colon, 
as in this message issued from the datalog Service 
indicating it has successfully started.

::

    12/23/17 21:57:06 dataLog: service started.

While it would be impossible to maintain a list of all 
of the possible messages, 
the messages issued at startup are pretty straightforward 
and provide a lot of information.

::

    ** Restart **

    SD initialized.
    1/08/19 19:39:43z Real Time Clock is running. Unix time 1546976383
    1/08/19 19:39:43z Power failure detected.
    1/08/19 19:39:43z Reset reason: External System
    1/08/19 19:39:43z ESP8266 ChipID: 427289
    1/08/19 19:39:43z IoTaWatt revision 4.9, firmware version 02_03_20
    1/08/19 19:39:43z SPIFFS mounted.
    1/08/19 14:39:44 Local time zone: -5:00
    1/08/19 14:39:44 Using Daylight Saving Time (BST) when in effect.
    1/08/19 14:39:44 device name: IotaWatt
    1/08/19 14:39:47 Connecting with WiFiManager.
    1/08/19 14:39:53 MDNS responder started for hostname IotaWatt
    1/08/19 14:39:53 LLMNR responder started for hostname IotaWatt
    1/08/19 14:39:53 HTTP server started
    1/08/19 14:39:53 WiFi connected. SSID=flyaway, IP=192.168.1.102, channel=1, RSSI -69db
    1/08/19 14:39:53 timeSync: service started.
    1/08/19 14:39:56 statService: started.
    1/08/19 14:39:56 Updater: service started. Auto-update class is BETA
    1/08/19 14:39:57 dataLog: service started.
    1/08/19 14:39:57 dataLog: Last log entry 01/08/19 14:39:35
    1/08/19 14:39:57 historyLog: service started.
    1/08/19 14:39:57 historyLog: Last log entry 01/08/19 14:39:00
    1/08/19 14:39:58 influxDB: started, url=192.168.1.101:8086, db=iotawatt, interval=10
    1/08/19 14:39:58 Updater: Auto-update is current for class BETA.
    1/08/19 14:39:58 influxDB: Start posting at 01/08/19 14:39:40

Some things to look for:

    Real Time Clock is running. Unix time 1546976383

Except for initial setup, the RTC should always be running.
If it is not, suspect the battery needs replacement.

    Power failure detected.

This indicates that power interrupted prior to the restart.

    Reset reason: External System

This is the reason for the restart.
External System is a normal cause. Reasons like WDT and 
Exception indicate program faults and, if frequent, 
should be posted to the support forum.

    IoTaWatt revision 4.9, firmware version 02_03_20

The hardware and firmware versions.  Good to know if things
go wrong, and also useful toget the corresponding version
of this documentation.

    Local time zone: -5:00

The local time zone specified in you config file.
This message and all subsequent messages should
be timestamped with local-time as opposed to UTC
which was indicated in prior messages with by the suffix 'z'.

    device name: IotaWatt

This is the external name of the unit.  It will
be used as the hostname when connecting to
WiFi and it is the name that you must use to 
access the device as in http://iotawatt.local.

    WiFi connected. SSID=flyaway, IP=192.168.1.102, 
    channel=1, RSSI -69db

Indicates connection to WiFi and the IP address 
assigned. The RSSI is an indication of WiFi signal strength.
A good number would be between -50 and -78 or so.
If you are having WiFi problems, this metric along with
the channel number can be helpful in resolving.

    timeSync: service started.

All of the regular services will log their startup.
Some will also provide additional information 
about their configuration or state. All messages 
from system services begin with the name of the service
followed by a colon.


