 /*********************************** Change log ****************************************************
 *  
 *   03/05/17 2.00.01 Cleaned up and added more documentation to Sample Power.  Also streamlined
 *                    it a little more and fixed a few loose ends.
 *   03/08/17 2.00.02 Recognize /edit and /graph uri in server
 *   03/10/17 2.00.03 API performance enhancement.  Add L1 index cache by full block buffering.
 *   03/12/17 2.00.04 Insist on WiFi connect at startup.
 *   03/12/17 2.00.05 Update frequency, samples/cycle in voltage only sample.
 *   03/17/17 2.00.06 Use ArduinoJson to generate server responses
 *                    Increase emoncms retry interval to 30 seconds
 *   03/17/17 2.00.07 Add partial read of message log. Dress up msgLog startup.
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
 *   07/19/17 2.02.06 Accept emoncms or Emoncms (compatibility)
 *   
 *****************************************************************************************************/
