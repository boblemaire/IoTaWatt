===============
Troubleshooting
===============

Led Indicator
-------------

The IotaWatt has a bi-color external LED indicator that is useful in 
determining the current state and for explicitly indicating 
serious problems. Whenever there is a question about whether 
something may be wrong, it is important to look at the LED indicator. 
The LED can blink RED (R) or GREEN (G). 
When indicating a device state, it will repeat a sequence of 
colored blinks.

Dull Green Glow
---------------

This typically means that the device is connected and working properly.

Dull Red Glow
-------------

This indicates that the device is working properly, 
but either there is no WiFi connection or the real time clock 
is not initialized and the time cannot be established from 
time-servers on the internet via WiFi. 
As long as the real time clock is initialized, 
the IotaWatt can run indefinitely without WiFi. 
If you are logging to Emoncms or another server, 
that will be suspended, but IotaWatt will recognize 
when the WiFi connection is restored, 
change the led to dull green, 
and send all of the data that it was unable to send during the outage.

To determine if IotaWatt is connected to the WiFi network, 
try to run the IotaWatt app from a browser on a device that 
is connected to the same WiFi network. 
If it works, the problem was probably the real time clock. 

If the app doesn't start, there is a problem with the WiFi connection. 
To be sure, restart the IotaWatt by disconnecting the 5VDC power momentarily 
and then observe the led during startup. 
Follow the troubleshooting guide for led indications during startup.

Not Illuminated
---------------

Under virtually all circumstances, the led should be illuminated. 
If not, the most probable cause is that there is no 5VDC supply. 
The power supply may be faulty or the USB plug may not be inserted all the way.
Check these things first. Try unplugging the 5VDC USB supply and reconnecting. 
Try a different 5VDC supply if available. If all else fails, a more serious 
problem with the device must be considered. 
Consult the supplier or someone who can troubleshoot electronic the hardware.

Continuous Red-Green-Red-Green.....
-----------------------------------

Downloading new firmware release. 
Updates are published from time to time and most IotaWatt subscribe 
to an auto-update class for which the current release may change. 
When a new release becomes available, 
the device initiates a download of a large file that contains 
all of the elements of the new release. 
The download usually takes about 5-6 seconds, but can take longer. 
While the download is in progress, the IoTaWatt does nothing else 
and will blink the led RED-GREEN continuously. 
At the completion of the download, 
the release will be installed and your IoTaWatt will restart. 
The entire process should never take more than a minute.

Led Sequences
-------------

red-green-green
^^^^^^^^^^^^^^^

IoTaWatt is having trouble connecting to the WiFi network. 
If this is a new IoTaWatt, or the network has changed, 
you will need to specify a new network. 
Otherwise, wait several minutes and the LED pattern 
should change to a new code. Follow the advice for that code.

red-green-red
^^^^^^^^^^^^^

A corrupted datalog (current or history) has been discovered. 
The file is being scanned from beginning top end and a diagnostic 
file will be created. This can take an hour or more for large logs. 
Allow to run to completion, after which the damaged log will be 
deleted and a new log created.

red-red-green
^^^^^^^^^^^^^

The configured WiFi network is unavailable and the real-time-clock 
is not running. IoTaWatt can run without WiFi, 
but it cannot accumulate log data if it doesn't know what time it is, 
and it sets the real-time-clock from the internet. 
Determine the problem with your WiFi network or connect to a new network.

green-red-red
^^^^^^^^^^^^^

IoTaWatt is having trouble accessing the SDcard inside the device. 
This typically happens during a restart of the device. 
You will need to power off the device and open it up by 
removing the four screws located under the rubber feet. 
Check that the SDcard is firmly seated in the socket and retry.

If problem persists, your card may have failed. 
You can try to insert the card into a computer that accepts SD 
cards to see if it can be read. If so, the problem is probably 
the IoTaWatt SD card socket and the device will need to be replaced. 
The good news is that the card should work in a replacement device 
and pick up where you left off with all historical data intact.

If your card cannot be read by another device, it is probably 
the card itself. Unfortunately, these things do fail from time-to-time. 
To be sure, insert any other available SDcard and try to restart. 
If the error indication changes to something else, it is probably 
a failure of the SDcard. Sadly, you will need to obtain a new 
one and initialize it with the files that are in the SD directory 
of the GitHub project. All historical data is lost and you will 
need to reconfigure the device as if were new.

red-red-red
^^^^^^^^^^^

This is a catch-all panic code, where the firmware has detected a 
situation that should not happen, or is impossible to deal with. 
Possibly there will be some diagnostic clues in the message log, 
or it may require connecting a serial terminal to the USB port 
to obtain further diagnostics. Problems in this category are beyond 
the scope of this document.

green-red-red-red
^^^^^^^^^^^^^^^^^

The config.txt file was not found on the SDcard.  Possibly
the SDcard has been damaged, or the file was inadvertently
deleted.  The card must be removed, examined in another 
computer, and a new config.txt file provided.

green-red-red-green
^^^^^^^^^^^^^^^^^^^

The config.txt file format is invalid (not valid Json)
and could not be used.  This could be the result of
editing the file improperly, or an error in saving the
configuration.  It may be possible to repair the file 
by mounting the SDcard in another computer and using
a Json linter to find the errors.  Otherwise, it may 
be necessary to replace the config.txt file.
