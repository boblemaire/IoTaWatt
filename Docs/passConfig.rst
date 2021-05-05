Passwords Configuration
=======================

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

Hover over |Setup| and click |Passwords| in the dropdown menu.

.. image:: pics/passConfig/configPasswords.png
    :scale: 23 %
    :align: center
    :alt: Config Device image

Open Access by Default
----------------------
The initial configuration has no access restrictions for reading or modifying device data or configurations.
If the device is exposed to the internet, then it can be read and modified from the internet without additional protections.
If untrusted actors can join your local network (WiFi, LAN) then they can read and modify without additional protections. 


Two Authorization Levels
------------------------
IoTaWatt supports two levels of authorization using digest password authorization. 
This isn't the most secure method of authorization, nor is it the least. 
Given the limitations of an IoT device, you may find it is a reasonable balance between exposure and data value.

Administrative Authorization
----------------------------
To enable authorization, specify an *admin* password. 
Once enabled, all configuration and access, except for User level access described below, will require the password. 
The username will be **admin**
Be sure to carefully note the password that you set, as IoTaWatt does not store the actual value and it cannot be recovered. 
There is a procedure to remove the Admin password, but it involves physically removing the SDcard.

User Authorization
------------------
The optional *user* password is effectively a read only password for use by apps that visualize data. 
The username will be **user**. Sessions authorized as user 
do not allow access to the configuration or SD file system, except for a special /user/ directory

Unrestricted LAN access
-----------------------
Checking this option will allow unrestricted access, i.e. without passwords, for any sessions originating on the local LAN.
This is defined as requests that come from an IP that is not the gateway IP.
With this option, you can have the best of both worlds: Unrestricted access in your home or on your VLAN, 
and authorization protection from unwanted access via the internet when port forwarding is enabled. 

Save
~~~~
Click |save|. Your changes will be saved. 

The new password specifications will take effect immediately.

.. |Setup| image:: pics/SetupButton.png
    :scale: 60 %
    :alt: **Setup button**

.. |Passwords| image:: pics/passConfig/PasswordsButton.png
    :scale: 60 %
    :alt: **Passwords button**

.. |save| image:: pics/SaveButton.png
    :scale: 50 %
    :alt: **Save**