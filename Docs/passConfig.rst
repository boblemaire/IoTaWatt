Passwords Configuration
=======================
The initial configuration has no access restrictions for reading or modifying device data or configurations.
If the device is exposed to the internet, then it can be read and modified from the internet without additional protections.
If untrusted actors can join your local network (WiFi LAN) then they can read and modify without additional protections.

If your LAN isn't secure, or you intend to provide access from the internet via port-forwarding, you should 
require authorization.  IoTaWatt supports two levels of authorization using the relatively secure 
digest password authorization protocol. This isn't the most secure method of authorization, nor is it the least. 
Given the limitations of an IoT device, you may find it is a reasonable balance between exposure and data value.

Administrative Authorization
----------------------------
To enable authorization, specify an *admin* password. 
Once enabled, all configuration and access, except for User level access described below, will require the password. 
The username is **admin**
Be sure to carefully note the password that you set, as IoTaWatt does not store the actual value and it cannot be recovered. 
There is a procedure to remove the Admin password, but it involves physically removing the SDcard.

User Authorization
------------------
The optional *user* password is effectively a read only password for use by apps that visualize data. 
The username is **user**. Sessions authorized as user 
do not allow access to the configuration or SD file system, except for a special /user/ directory

Unrestricted LAN access
-----------------------
Checking this option will allow unrestricted access, i.e. without passwords, for any sessions originating on the local LAN.
This is defined as requests that come from an IP that is not the gateway IP.
With this option, you can have the best of both worlds: Unrestricted access in your home or on your VLAN, 
and authorization protection from unwanted access via the internet when port forwarding is enabled. 

Setting and changing passwords
------------------------------
Hover over |Setup| and select |Passwords|. Specify a new **admin** password and optionally a **user** password.
Check the "Unrestricted LAN access" if so desired. Click |save|. 
Your changes will be saved. 

The new password specifications will take effect immediately.

.. image:: pics/passConfig/configPasswords.png
    :scale: 60 %
    :align: center
    :alt: Passwords setup display

.. |Setup| image:: pics/SetupButton.png
    :scale: 60 %
    :alt: **Setup button**

.. |Passwords| image:: pics/passConfig/PasswordsButton.png
    :scale: 60 %
    :alt: **Passwords button**

.. |save| image:: pics/SaveButton.png
    :scale: 50 %
    :alt: **Save**