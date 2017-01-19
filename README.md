

### IotaWatt 1.2

IotaWatt is an open-hardware/open-source project to produce an accurate, low-cost, multi-channel, and easy to use electric power monitor.  It's based on the ESP8266 IoT platform using MCP3208 12 bit ADCs to sample voltage and current at very high sample rates. The version described here with schematics and PCB layout have been fabricated and several are running well in residential installations.

The board supports either version 0.9 or 1.0 of the NodeMCU open development ESP8266 board running the Arduino/ESP8266 SDK platform.

This release represents considerable development from the previous version. Among the most significant:

   - A wifi server that supports a JS application on the local Wifi network.  The application provdes the ability to configure the voltage and current channels of the device, as well as to specify an internet service to upload data. Currently eMonCMS is supported, with an architecture that allows for easy addition of others.


   - On board Sdcard datalogging and message logging.  The device logs all data to the Sdcard at 5 second intervals.  If connectivity to the internet server is lost, the device will catch up when reconnected.  The architecture will support adding a query facility through the web server.

   - Re-architected core makes best use of time by running services to datalog, internet post, and web serve in between sampling power data.  The scheduler concentrates on sampling and does these other functions largely during the several milliseconds available in the half cycle between sampling.  The net result is that the device does everything while averaging more than 60% of its time sampling electricity use.

   - Supports voltage input on any channel, and voltage reference for any power channel can be to any voltage channel.  This opens the door for three phase monitoring as well as more accurate monitoring of US single phase services by monitoring the voltage on both legs.  Also makes monitoring all circuits in a 120/208 “two phase” panel possible.

While this is a major release that does run well, there are as yet many more improvements in progress or on the to-do list.  

- The board will be simplified to elimiminate several components.
- The form factor will change to allow stacked input “shields” that will allow stereo jack - input or screw terminals – or both by stacking different input modumles.
- Support for 33mv CTs will be available on a stackable 1.2V input board that can be - combined with the current 3.2V offering.
- Support for more inputs.  At least 21 in total.

In addition to the hardware circuit described here, to run the system you would need:

- ESP8266 nodeMCU 0.9 or 1.0
- 8GB or better Sdcard with the contents of the Sdcard library copied to the root directory.

To run:

- Boot the device. 
- Connect via the ESP8266 wifi-manager protocol.
- Run the configuration utility at iotawatt.local
- Commit and restart.

You will need to calibrate the AC adapter to get accurate results.  There is a calibration procedure in the configuration utility to do that using a decent line voltage reference.

The latest PCB layout includes jumpers to allow selecting 3.2v or 5v voltage to the ADCs.  Previous versions have all used 5v, but as it turns out, they run fine at 3.2v.  This eliminates the need for the jumper on the MISO SPI line to reduce the voltage to 3.2v.  Be aware the jumper is needed if selecting 5v.

The device currently takes two or three WDT (watchdog timer) resets per day.  Research is ongoing to identify the cause.  Nevertheless, the board reliably reboots and is running normal within 20 seconds, so not a critical problem.

Thanks to contributors of other open software that have been incorporated into this project.  In particular the ArduinoJSON and SDWebServer software made a lot of this possible, not to mention the Arduino/ESP8266 project and related forums.
