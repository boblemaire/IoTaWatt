#include "emoncms_uploader.h"
#include "splitstr.h"

/*****************************************************************************************
 *          queryLast() // Use this to initiate the last sent query.
 * **************************************************************************************/
void emoncms_uploader::queryLast(){
    _script = _outputs->first();
    _state = query_s;
}

/*****************************************************************************************
 *          TickBuildLastSent()
 * **************************************************************************************/
uint32_t emoncms_uploader::handle_query_s(){
        
    trace(T_influx1,20);
    
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
            trace(T_influx1,63);     
            double value = script->run(oldRecord, newRecord, elapsedHours);
            if(value == value){
                trace(T_influx1,64);   
                thisMeasurement = varStr(_measurement, script);
                if(_staticKeySet && thisMeasurement.equals(lastMeasurement)){
                    reqData.printf_P(PSTR(",%s=%.*f"), varStr(_fieldKey, script).c_str(), script->precision(), value);
                } else {
                    if(lastMeasurement.length()){
                        reqData.printf(" %d\n", oldRecord->UNIXtime);
                    }
                    reqData.write(thisMeasurement);
                    if(_tagSet){
                        trace(T_influx1,64);
                        influxTag* tag = _tagSet;
                        while(tag){
                            reqData.printf_P(PSTR(",%s=%s"), tag->key, varStr(tag->value, script).c_str());
                            tag = tag->next;
                        }
                    }
                    trace(T_influx1,65);
                    reqData.printf_P(PSTR(" %s=%.*f"), varStr(_fieldKey, script).c_str(), script->precision(), value);
                }
            }
            lastMeasurement = thisMeasurement;
            script = script->next();
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
    endpoint += _database;
    if(_retention){
        endpoint += "&rp=";
        endpoint += _retention;
    }
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
    if(_user && _pwd){
        xbuf xb;
        xb.printf("%s:%s", _user, _pwd);
        base64encode(&xb);
        String auth = "Basic ";
        auth += xb.readString(xb.available());
        _request->setReqHeader("Authorization", auth.c_str()); 
    }
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
        trace(T_influx1, 101);
        delete[] _database;
        _database = charstar(config.get<char*>("database"));
        delete[] _user;
        _user = charstar(config.get<const char*>("user"));
        delete[] _pwd;
        _pwd = charstar(config.get<char *>("pwd"));
        delete _retention;
        _retention = charstar(config.get<const char*>("retp"));
        
        trace(T_influx1, 101);
        delete[] _measurement;
        _measurement = charstar(config.get<const char *>("measurement"));
        if (!_measurement)
        {
            _measurement = charstar("$name");
        }
        trace(T_influx1, 102);
        delete[] _fieldKey;
        _fieldKey = charstar(config.get<const char *>("fieldkey"));
        if (!_fieldKey)
        {
            _fieldKey = charstar("value");
        }
        trace(T_influx1, 102);
        _stop = config.get<bool>("stop");

        // Build tagSet

        trace(T_influx1, 103);
        delete _tagSet;
        _tagSet = nullptr;
        JsonArray &tagset = config["tagset"];
        _staticKeySet = true;
        if (tagset.success())
        {
            trace(T_influx1, 103);
            for (int i = tagset.size(); i > 0;)
            {
                i--;
                influxTag *tag = new influxTag;
                tag->next = _tagSet;
                _tagSet = tag;
                tag->key = charstar(tagset[i]["key"].as<const char *>());
                tag->value = charstar(tagset[i]["value"].as<const char *>());
                if ((strstr(tag->value, "$units") != nullptr) || (strstr(tag->value, "$name") != nullptr))
                    _staticKeySet = false;
            }
        }
    
        // Build the measurement scriptset

    trace(T_influx1,104);
    delete _outputs;
    _outputs = nullptr;
    JsonVariant var = config["outputs"];
    if(var.success()){
        trace(T_influx1,105);
        _outputs = new ScriptSet(var.as<JsonArray>());
    }
    else {
        log("%s: No measurements.", _id);
        return false;
    }

            // sort the measurements by measurement name

    const char *measurement = _measurement;
    _outputs->sort([this](Script* a, Script* b)->int {
        return strcmp(varStr(_measurement, a).c_str(), varStr(_measurement, b).c_str());
    });

    return true;
}
    

String emoncms_uploader::varStr(const char* in, Script* script)
{
    // Return String with variable substitutions.

  String out;
  while(*in){ 
    if(memcmp(in,"$device",7) == 0){
      out += deviceName;
      in += 7;
    }
    else if(memcmp(in,"$name",5) == 0){
      out += script->name();
      in += 5;
    }
    else if(memcmp(in,"$units",6) == 0){
      out += script->getUnits();
      in += 6;
    }
    else {
      out += *(in++);
    }
  } 
  return out;
}
