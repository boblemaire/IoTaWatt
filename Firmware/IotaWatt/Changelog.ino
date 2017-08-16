 /*********************************** Change log ****************************************************
 *  
 *   03/05/17 2.00.01 Cleaned up and added more documentation to Sample Power.  Also streamlined
 *                    it a little more and fixed a few loose ends.
 *   03/08/17 2.00.02 Recognize /edit and /graph uri in server
 *   03/10/17 2.00.03 API performance enhancement.  Add L1 index cache by full block buffering.
 *   03/12/17 2.00.04 Insist on WiFi connect at startup.
 *   03/12/17 2.00.05 Update frequency, samples/cycle in voltage only sample.
 *   03/17/17 2.00.06 Use ArduinoJson to generate server responses
 *                    Increase Emoncms retry interval to 30 seconds 
 *   03/18/17 2.00.08 Fix rounding in API Json output
 *   03/18/17 2.00.09 Fix typo in json rework
 *   03/18/17 2.00.10 Change wifi retry interval
 *   
 *   04/14/17 2.01.00 Major update - move inputs to a class, add outputs, CSS
 *   04/21/17 2.01.01 Add polyphase correction with single VT
 *   05/06/17 2.01.02 Rework samplePower for better phase correction
 *   05/28/17 2.01.03 Add automatic update and rework WiFiManager use
 *   06/04/17 2.01.04 Miscelaneous cleanup
 *   06/26/17 2.01.05 Ongoing development
 *   07/09/17 2.02.00 Version 4 hardware support
 *   07/12/17 2.02.01 Fix sample power, enhance graph
 *   07/15/17 2.02.02 Enhance status display
 *   07/17/17 2.02.03 Fix problems with sample power & multiple V channels
 *   07/19/17 2.02.04 Changes to Emoncms support 
 *   07/19/17 2.02.05 Bump version
 *   07/19/17 2.02.06 Accept Emoncms or Emoncms (compatibility)
 *   07/23/17 2.02.07 Overhaul RTC initialization and power fail logging
 *   07/23/17 2.02.08 Add LED problem indicators during startup
 *   07/23/17 2.02.09 Add LED indication of connection and timer status
 *   08/07/17 2.02.10 Add Emonpi URL support
 *   08/11/17 2.02.11 Upgrade to new script format (requires new index.htm)
 *   08/12/17 2.02.12 Change WiFi pwd to device name. Filter ADC lsb noise.
 *   08/15/17 2.02.13 Add secure encrypted posting to Emoncms
 *   08/16/17 2.02.14 Fix problem changing Emoncms method on the fly
 *   
 *****************************************************************************************************/
