#include "emoncms_uploader.h"

/*****************************************************************************************
 *          TickBuildLastSent()
 * **************************************************************************************/
uint32_t emoncms_uploader::handle_query_s(){
        
    trace(T_influx1,20);
    stop();
    return 1;

    // Send the request

    trace(T_influx1,20);
    HTTPPost("/query", checkQuery_s, "application/x-www-form-urlencoded");
    return 1;
}

/*****************************************************************************************
 *          TickCheckLastSent()
 * **************************************************************************************/
uint32_t emoncms_uploader::handle_checkQuery_s(){
    // trace 30
    
    return 1;
}

/*****************************************************************************************
 *          handle_write_s())
 * **************************************************************************************/
uint32_t emoncms_uploader::handle_write_s(){
    trace(T_influx1,60);

    // This is the predominant state of the uploader
    // It will wait until there is enough data for the next post
    // then build the payload and initiate the post operation.

    if(_stop){
        stop();
        return 1;
    }

    // If not enough data to post, set wait and return.

    if(Current_log.lastKey() < (_lastSent + _interval + (_interval * _bulkSend))){
        if(oldRecord){
            delete oldRecord;
            oldRecord = nullptr;
            delete newRecord;
            newRecord = nullptr;
        }
        return UTCtime() + 1;
    }

    // If datalog buffers not allocated, do so now and prime latest.

    trace(T_influx1,60);
    if(! oldRecord){
        trace(T_influx1,61);
        oldRecord = new IotaLogRecord;
        newRecord = new IotaLogRecord;
        newRecord->UNIXtime = _lastSent + _interval;
        Current_log.readKey(newRecord);
    }

    // Build post transaction from datalog records.

    while(reqData.available() < _bufferLimit && newRecord->UNIXtime < Current_log.lastKey()){

        // Swap newRecord top oldRecord, read next into newRecord.

        trace(T_influx1,60);
        IotaLogRecord *swap = oldRecord;
        oldRecord = newRecord;
        newRecord = swap;
        newRecord->UNIXtime = oldRecord->UNIXtime + _interval;
        Current_log.readKey(newRecord);

        // Compute the time difference between log entries.
        // If zero, don't bother.

        trace(T_influx1,62);    
        double elapsedHours = newRecord->logHours - oldRecord->logHours;
        if(elapsedHours == 0){
            trace(T_influx1,63);
            if((newRecord->UNIXtime + _interval) <= Current_log.lastKey()){
                return 1;
            }
            return UTCtime() + 1;
        }

        // Build measurements for this interval

        trace(T_influx1,62); 
        String lastMeasurement;
        String thisMeasurement;
        Script *script = _outputs->first();
        while(script)
        {
            
        }
        _lastPost = oldRecord->UNIXtime;
        reqData.printf(" %d\n", _lastPost);
    }

    // Free the buffers

    delete oldRecord;
    oldRecord = nullptr;
    delete newRecord;
    newRecord = nullptr;

    // Initiate HTTP post.

    String endpoint = "/write?precision=s&db=";
    
    HTTPPost(endpoint.c_str(), checkWrite_s, "text/plain");
    return 1;
}

/*****************************************************************************************
 *          handle_checkWrite_s()
 * **************************************************************************************/
uint32_t emoncms_uploader::handle_checkWrite_s(){
    trace(T_influx1,91);

    // Check the result of a write transaction.
    // Usually success (204) just note the lastSent and
    // return to buildPost.

    if(_request->responseHTTPcode() == 204){
        delete[] _statusMessage;
        _statusMessage = nullptr;
        _lastSent = _lastPost; 
        _state = write_s;
        trace(T_influx1,93);
        return 1;
    }

    // Deal with failure.

    trace(T_influx1,92);
    char msg[100];
    sprintf_P(msg, PSTR("Post failed %d"), _request->responseHTTPcode());
    delete[] _statusMessage;
    _statusMessage = charstar(msg);
    delete _request;
    _request = nullptr; 
    _state = write_s;
    return UTCtime() + 10;
}

/*****************************************************************************************
 *          setRequestHeaders()
 * **************************************************************************************/
void emoncms_uploader::setRequestHeaders(){
    trace(T_influx1,95);
    
    _request->setReqHeader("Content-Type","application/x-www-form-urlencoded");
    trace(T_influx1,95);
}


//********************************************************************************************************************
//
//               CCC     OOO    N   N   FFFFF   III    GGG    CCC   BBBB
//              C   C   O   O   NN  N   F        I    G      C   C  B   B
//              C       O   O   N N N   FFF      I    G  GG  C      BBBB
//              C   C   O   O   N  NN   F        I    G   G  C   C  B   B
//               CCC     OOO    N   N   F       III    GGG    CCC   B BBB
//
//********************************************************************************************************************
bool emoncms_uploader::configCB(JsonObject& config){
    
    trace(T_Emoncms,100);
    hex2bin(_cryptoKey, config["apikey"].as<char*>(), 16);
    delete[] _node;
    _node = charstar(config["node"].as<char*>());
    delete[] _userID;
    _userID = charstar(config["userid"].as<char*>());
    _encrypted = (strlen(_userID) == 0) ? false : true;
    trace(T_Emoncms,101);
    Script* script = _outputs->first();
    int index = 0;
    while(script){
        if(String(script->name()).toInt() <= index){
            log("%s: output sequence error, stopping.");
            return false;
        }
        else {
            index = String(script->name()).toInt();
        }
        script = script->next();
    }
    trace(T_Emoncms,102);
    return true; 
}
