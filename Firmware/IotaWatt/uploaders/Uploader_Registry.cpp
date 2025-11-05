#include "Uploader_Registry.h"

#define MAX_LIST_SIZE 6

// Registry maintains two lists:
//  A list of the names of the uploaders, which is delimited by a nullptr entry;

char **uploaderNames = nullptr;

//  A list of the instance of the corresponding uploader if instantiated (configured).

Uploader **uploaderInstances;

//  Also maintains count of the number of entries in the name list exclusive of the delimiting entry;

int uploaderCount = 0;
int uploaderInstancesCount = 0;

// Called from setup to initialize the list of uploaders

void declare_uploaders(){

    // Allocate and initialize the lists

    trace(T_uploaderRegistry, 0);
    uploaderNames = new char *[MAX_LIST_SIZE+1];
    uploaderInstances = new Uploader *[MAX_LIST_SIZE];
    memset(uploaderNames, 0, MAX_LIST_SIZE * 4 + 4);
    memset(uploaderInstances, 0, MAX_LIST_SIZE * 4);

    /******************************************************************************************/
    /******************************************************************************************/
    /************************** Add new uploaders below ***************************************/
    /******************************************************************************************/
    /******************************************************************************************/

    trace(T_uploaderRegistry, 0,1);    
    uploaderNames[uploaderCount++] = charstar("Emoncms");
    trace(T_uploaderRegistry, 0,2);
    uploaderNames[uploaderCount++] = charstar("influxDB");
    trace(T_uploaderRegistry, 0,3);
    uploaderNames[uploaderCount++] = charstar("influxDB2");
    
}

void set_buffer_limit(){
    uploaderInstancesCount = PVoutput ? 1 : 0;
    for (int i = 0; i < uploaderCount; i++){
        if(uploaderInstances[i]){
            uploaderInstancesCount++;
        }
    }
    uploaderBufferLimit = MIN(uploaderBufferTotal / uploaderInstancesCount, 4000);
}

    // new_uploader(ID) - called to instantiate an uploader.

    Uploader *new_uploader(const char *ID)
{

    trace(T_uploaderRegistry, 10,0);
    for (int i = 0; i < uploaderCount; i++){
        trace(T_uploaderRegistry, 10,1);
        if (strcmp(ID, uploaderNames[i]) == 0){
            if(uploaderInstances[i] == nullptr){
                uploaderInstancesCount++;

                /******************************************************************************************/
                /******************************************************************************************/
                /************************** Add new uploaders below ***************************************/
                /******************************************************************************************/
                /******************************************************************************************/

                if (strcmp(ID, "Emoncms") == 0)
                {
                    uploaderInstances[i] = new Emoncms_uploader;
                }
                else if (strcmp(ID, "influxDB") == 0)
                {
                    uploaderInstances[i] = new influxDB_uploader;
                }
                else if (strcmp(ID, "influxDB2") == 0)
                {
                    uploaderInstances[i] = new influxDB2_uploader;
                }
                else{
                    uploaderInstancesCount--;
                }
                
            }
            trace(T_uploaderRegistry, 10,3);
            set_buffer_limit();
            return uploaderInstances[i];
        }
    }
    return nullptr;
}

bool delete_uploader(const char *ID){
    trace(T_uploaderRegistry, 20,0);
    for (int i = 0; i < uploaderCount; i++){
        if (strcmp(ID, uploaderNames[i]) == 0){
            trace(T_uploaderRegistry, 20,1);
            if(uploaderInstances[i]){
                uploaderInstances[i]->end();
                uploaderInstances[i] = nullptr;
                uploaderInstancesCount--;
                uploaderBufferLimit = MIN(uploaderBufferTotal / uploaderInstancesCount + (PVoutput ? 1 : 0), 4000);
                return true;
            }
            return false;
        }
    }
    return false;
}

Uploader *get_uploader_instance(const char *ID){
    trace(T_uploaderRegistry, 30,0);
    for (int i = 0; i < uploaderCount; i++){
        if (strcmp(ID, uploaderNames[i]) == 0){
            return uploaderInstances[i];
        }
    }
    return nullptr;
}

char** get_uploader_list(){
    trace(T_uploaderRegistry, 40,0);
    return uploaderNames;
}

int get_uploaderCount(){
    trace(T_uploaderRegistry, 50,0);
    return uploaderCount;
}



