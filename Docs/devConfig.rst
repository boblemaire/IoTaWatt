Device Configuration
====================

Successful startup will be indicated by a dull green glow on the LED.
If the LED is off, or blinking a sequence,
see the `troubleshooting <troubleshooting.html>`_ section.

After successful startup, you can connect to the device with your 
web browser to access the configuration app. 
Use the url: ``http://iotawatt.local`` or, if your device
has been renamed, use ``http://<newname>.local``. The configuration app starts with a row of buttons:

.. image:: pics/mainMenu.png
    :scale: 75 %
    :align: center
    :alt: Main Menu image

Hover over |Setup| and click |device| in the dropdown menu.

.. image:: pics/devConfig/configDevice.png
    :scale: 43 %
    :align: center
    :alt: Config Device image

Device name
-----------

You can change the ``Device name`` to another 8 character name if you wish.
If you have more than one IoTaWatt you will need to do this so that they
each have unique names. When you press save, the IoTaWatt will restart using
the new name. From then on, ``http://<newname>.local`` will be the url
that you will use to access the device from your browser, and the new 
``Device name`` will be the password that you must use to 
connect to the AP if configuring for a different WiFi network.

TimeZone
--------

Set your local Time Zone relative to UTC time. 
All of the measurements are time stamped using UTC, 
but `log messages <messageLog.html>`_ and various reporting apps 
will use this offset to show the data in local time.

If your time zone is subject to "Daylight Saving Time", 
check the ``Allow Daylight Time`` box.
IoTaWatt has DST rules for most of North America, Europe, Australia and New Zealand.

Auto-update Class
----------------- 

Auto-update Class tells IotaWatt if you want to receive 
automatic updates of the firmware and what type of updates 
you are interested in. The choices are:

    :NONE:
        Do not Auto-update this device.
    :MAJOR:
        Only update major releases of the software.
    :MINOR:
        Update with minor releases. More frequent 
        but somewhat tested firmware. This is recommended.
    :BETA:
        Latest production firmware.
    :ALPHA:
        Recently released firmware with the latest features - 
        and the latest bugs.

IotaWatt checks the IotaWatt.com site for new software regularly.
The update process takes less than a minute.
New firmware is authenticated with a digital signature from IotaWatt and installed automatically.

Save
~~~~

Click |save|. Your changes will be saved. 
If you changed the name of your device,
it will restart when you press save and you will need to 
restart the configuration application from ``http://<newname>.local``.

The next step is `VT Configuration <VTconfig.html>`__

.. |Setup| image:: pics/SetupButton.png
    :scale: 60 %
    :alt: **Setup button**

.. |device| image:: pics/deviceButton.png
    :scale: 60 %
    :alt: **Device Button**

.. |save| image:: pics/SaveButton.png
    :scale: 50 %
    :alt: **Save**