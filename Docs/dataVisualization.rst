=================================
Data Visualization
=================================

IoTaWatt is capable of storing ten years or more of high resolution data. 
An integrated web-server provides access to that data using a versatile 
query/graph application called Graph+ and also provides a RESTful data query 
facility capable of producing JSON or CSV formatted downloads.

You can also upload your data to InfluxDB, PVoutput or Emoncms.org providing 
a variety of alternative ways to organize and view your voltage,
power and energy use. The servers can be fast and accessible 
from any place where there is internet connectivity. 
But there are some circumstances under which you may prefer to 
access your data directly from the IoTaWatt:

*   You prefer the simplicity and power of the local graphic viewer.
*   You are not using Emoncms or influxDB to save your data.
*   You require finer resolution than is used in your Emoncms feeds.
*   You need to see data that is collected by IoTaWatt but is not being
    uploaded to your cloud service.

There are two graphing packages available with the current release:

`Graph+ <graphPlus.html>`_
--------------------------------------

This latest graphic viewer unlocks virtually all of the data in the datalog. You can
graph all of the metrics associated with each input or output, including Volts,
Watts, Wh, Amps, VA, PF and Hz.

There are selectable predefined time-periods like "Today", "Yesterday", "Last Week"
and "Last Month", and a calendar interface that can be used to select custom date/time
bounds. Data can be grouped by hour, day, week or month. The IoTaWatt query recognizes
week and month boundaries and daylight-time changes.

`Original Graph <originalGraph.html>`_
--------------------------------------

The graph program provided in the initial releases through 02_03_02 is still
supported and available for those who may have become comfortable with it and
whose capabilities are adequate for your needs.  It remains as a menu pick under
Data tab.

The new Graphic Viewer is a superset of the functions of the Original Graph, so
this feature should be considered depricated and may be removed in a future release.
