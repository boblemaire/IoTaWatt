#ifndef exportlog_h
#define exportlog_h

#include <IotaWatt.h>

//********************************************************************************************************
// Exportlog class
//
// This class impliments an IoTaWatt Service that creates and maintains an 
// import/export log from data in the current log.
// 
// It is instantiated when an "exportlog" object is encountered in the configuration.
// It will continue to run until restart if deconfigured.
// The file must be manually deleted to remove import/export history.
//
//********************************************************************************************************

class Exportlog {

    public:
        Exportlog();
        ~Exportlog();

    protected:

};

#endif