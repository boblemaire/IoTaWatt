#ifndef updater_h
#define updater_h

#include <Arduino.h>

/*************************************************************************************************
 * 
 *          updater - Service to check and update firmware
 * 
 *************************************************************************************************/
uint32_t updater(struct serviceBlock* _serviceBlock);


/************************************************************************************************************
 * bool copyUpdate(String version)
 * 
 * Copy release files staged in the update directory to the SD root.
 * Delete the files as they are copied and delete the directory when complete.
 * 
 ***********************************************************************************************************/
bool copyUpdate(String version);  

/*************************************************************************************************
 *  bool checkUpdate()
 *  
 *  Check to see if there is an update available.
 *  If so, download, install, return true;.
 *  If not, return false;
 ************************************************************************************************/
bool checkUpdate();

#endif // updater_h
