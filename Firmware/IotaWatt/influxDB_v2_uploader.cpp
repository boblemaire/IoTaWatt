#include "influxDB_v2_uploader.h"
#include "splitstr.h"

/*****************************************************************************************
 *          handle_query_s()
 * **************************************************************************************/
uint32_t influxDB_v2_uploader::handle_query_s(){
    trace(T_influx2,20);

    // Set range for lookback.
    // Initially 1 hour, then x10 each successive iteration.

    uint32_t rangeFloor = MAX(Current_log.firstKey(), _uploadStartDate);
    uint32_t timenow = UTCtime();
    timenow -= timenow % _interval;
    uint32_t rangeStop = MAX(timenow - (_lookbackHours * 3600), rangeFloor);
    _lookbackHours = _lookbackHours ? (_lookbackHours * 10) : 1;
    uint32_t rangeBegin = MAX(timenow - (_lookbackHours * 3600), rangeFloor);
    
    // If entire range searched (with no success),
    // Set to start at _uploadStartDate or now()
    
    if(rangeBegin >= rangeStop){
        _lastSent = timenow;
        if(_uploadStartDate){
            _lastSent = rangeFloor;
        }
        _lastSent -= _lastSent % _interval;
        log("%s: Start posting %s", _id, localDateString(_lastSent + _interval).c_str());
        _state = write_s;
        return 1;
    }

    // Build a flux query to find last record in set of measurements.
    
    reqData.flush();
    reqData.printf_P(PSTR("from(bucket: \"%s\")\n  |> range(start: %d, stop: %d)\n  |> "), _bucket, rangeBegin, rangeStop);
    reqData.print(F("filter(fn: (r) => "));
    trace(T_influx2,20);
    if(_tagSet){
        trace(T_influx2,21);    
        influxTag* tag = _tagSet;
        while(tag){
            String tagValue = tag->value;
            if( ! (strstr(tag->value,"$name") || strstr(tag->value,"$units"))){
                reqData.printf_P(PSTR("r[\"%s\"] == \"%s\" and "), tag->key, tag->value);
            }
            tag = tag->next;
        }
    }
    String prefix = "(";
    Script *script = _outputs->first();
    trace(T_influx2,20);
    while(script)
    {
        trace(T_influx2,21);
        reqData.printf_P(PSTR("%sr[\"_measurement\"] == \"%s\""), prefix.c_str(), varStr(_measurement, script).c_str());
        prefix = " or ";
        script = script->next();
    }
    trace(T_influx2,20);
    reqData.print(F("))\n  |> last()\n  |> map(fn: (r) => ({_measurement: r._measurement, _time: (uint(v:r._time)) / uint(v:1000000000)}))\n"));
    reqData.print(F("  |> sort (columns: [\"_time\"], desc: true)\n"));

    // Initiate the HTTP request.

    String endpoint = "/query?orgID=";
    endpoint += _orgID;
    HTTPPost(endpoint.c_str(), checkQuery_s, "application/vnd.flux");
    return 1;
}

/*****************************************************************************************
 *          setRequestHeaders()
 * **************************************************************************************/
void influxDB_v2_uploader::setRequestHeaders(){
    String auth = "Token ";
    auth += _token;
    _request->setReqHeader(F("Authorization"), auth.c_str());
}

/*****************************************************************************************
 *         handle_checkQuery_s()
 * **************************************************************************************/
uint32_t influxDB_v2_uploader::handle_checkQuery_s(){
    trace(T_influx2,30);

    // Deal with failure.

    if(_request->responseHTTPcode() != 200){
        trace(T_influx2,31);
        delete[] _statusMessage;
        _statusMessage = charstar(F("Last sent query failed. HTTPcode %d"), String(_request->responseHTTPcode()).c_str());
        Serial.println(_request->responseText());
        delay(60, query_s);
        return 1;
    }

    // retrieve the response and parse first line.

    String response = _request->responseText();
    trace(T_influx2,30);
    splitstr headline(response.c_str(), ',', '\n');
    trace(T_influx2,32);

    // If no second line, query again.

    char *datapos = strchr(response.c_str(), '\n');
    trace(T_influx2,32);
    if(!datapos){
        trace(T_influx2,33);
        _state = query_s;
        return 1;
    }

    // Have second line, parse that

    trace(T_influx2,34);
    splitstr dataline(datapos + 1, ',', '\n');
    trace(T_influx2,34);
    if(dataline.length() == 0 || dataline.length() != headline.length()){
        _state = query_s;
        return 1;
    }
    for (int i = 0; i < headline.length(); i++){
        if(strcmp(headline[i],"_time")){
            char *data = dataline[i];
            if(data){
                _lastSent = strtol(data, 0, 10);
                if(_lastSent >= MAX(Current_log.firstKey(), _uploadStartDate)){
                    if( ! _stop){
                        log("%s: Resume posting %s", _id, localDateString(_lastSent + _interval).c_str());
                    }
                    _lookbackHours = 0;
                    _state = write_s;
                    return 1;
                }
            }
        }
    }
    delete _statusMessage;
    _statusMessage = charstar("Last sent query invalid response stopping");
    _stop = true;
    stop();
    return 1;
}

/*****************************************************************************************
 *          handle_write_s
 * **************************************************************************************/
uint32_t influxDB_v2_uploader::handle_write_s(){
    trace(T_influx2,60);

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

    trace(T_influx2,60);
    if(! oldRecord){
        trace(T_influx2,61);
        oldRecord = new IotaLogRecord;
        newRecord = new IotaLogRecord;
        newRecord->UNIXtime = _lastSent + _interval;
        Current_log.readKey(newRecord);
    }

    // Build post transaction from datalog records.

    while(reqData.available() < uploaderBufferLimit && newRecord->UNIXtime < Current_log.lastKey()){
        
        if(micros() > bingoTime){
            return 10;
        }

        // Swap newRecord top oldRecord, read next into newRecord.

        trace(T_influx2,60);
        IotaLogRecord *swap = oldRecord;
        oldRecord = newRecord;
        newRecord = swap;
        newRecord->UNIXtime = oldRecord->UNIXtime + _interval;
        Current_log.readKey(newRecord);

        // Compute the time difference between log entries.
        // If zero, don't bother.

        trace(T_influx2,62);    
        double elapsedHours = newRecord->logHours - oldRecord->logHours;
        if(elapsedHours == 0){
            trace(T_influx2,63);
            if((newRecord->UNIXtime + _interval) <= Current_log.lastKey()){
                return 1;
            }
            return UTCtime() + 1;
        }

        // Build measurements for this interval

        trace(T_influx2,62); 
        String lastMeasurement;
        String thisMeasurement;
        Script *script = _outputs->first();
        while(script)
        {
            trace(T_influx2,63);     
            double value = script->run(oldRecord, newRecord, elapsedHours);
            if(value == value){
                trace(T_influx2,64);   
                thisMeasurement = varStr(_measurement, script);
                if(_staticKeySet && thisMeasurement.equals(lastMeasurement)){
                    reqData.printf_P(PSTR(",%s=%.*f"), varStr(_fieldKey, script).c_str(), script->precision(), value);
                } else {
                    if(lastMeasurement.length()){
                        reqData.printf(" %d\n", oldRecord->UNIXtime);
                    }
                    reqData.write(thisMeasurement);
                    if(_tagSet){
                        trace(T_influx2,64);
                        influxTag* tag = _tagSet;
                        while(tag){
                            reqData.printf_P(PSTR(",%s=%s"), tag->key, varStr(tag->value, script).c_str());
                            tag = tag->next;
                        }
                    }
                    trace(T_influx2,65);
                    reqData.printf_P(PSTR(" %s=%.*f"), varStr(_fieldKey, script).c_str(), script->precision(), value);
                }
            }
            lastMeasurement = thisMeasurement;
            script = script->next();
        }
        _lastPost = oldRecord->UNIXtime;
        reqData.printf(" %d\n", _lastPost);
    }

    // Initiate HTTP post.

    String endpoint = "/write?precision=s&orgID=";
    endpoint += _orgID;
    endpoint += "&bucket=";
    endpoint += _bucket;
    HTTPPost(endpoint.c_str(), checkWrite_s, "text/plain");
    return 1;
}

/*****************************************************************************************
 *          handle_checkWrite_s()
 * **************************************************************************************/
uint32_t influxDB_v2_uploader::handle_checkWrite_s(){
    trace(T_influx2,91);

    // Check the result of a write transaction.
    // Usually success (204) just note the lastSent and
    // return to buildPost.

    if(_request->responseHTTPcode() == 204){
        delete[] _statusMessage;
        _statusMessage = nullptr;
        _lastSent = _lastPost; 
        _state = write_s;
        trace(T_influx2,93);
        return 1;
    }

    // Deal with failure.

    if(_request->responseHTTPcode() == 429){
        log("influxDB2_v2: Rate exceeded");
        Serial.println(_request->responseText());
        delay(5, HTTPpost_s);
        return 1;
    }

    trace(T_influx2,92);
    char msg[100];
    sprintf_P(msg, PSTR("Post failed %d"), _request->responseHTTPcode());
    delete[] _statusMessage;
    _statusMessage = charstar(msg);
    delete _request;
    _request = nullptr; 
    _state = write_s;
    return UTCtime() + 2;
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
bool influxDB_v2_uploader::configCB(JsonObject& config){
    trace(T_influx2, 101);
    delete[] _bucket;
    _bucket = charstar(config.get<char *>("bucket"));
    if (strlen(_bucket) == 0)
    {
        log("%s: Bucket not specified", _id);
        return false;
    }

    trace(T_influx2, 101);
    delete[] _orgID;
    _orgID = charstar(config.get<const char *>("orgid"));
    if (!_orgID || strlen(_orgID) != 16)
    {
        log("%s: Invalid organization ID", _id);
        return false;
    }
    trace(T_influx2, 101);
    delete[] _token;
    _token = charstar(config.get<const char *>("authtoken"));
    if (!_token || strlen(_token) != 88)
    {
        log("%s: Invalid authorization token", _id);
        return false;
    }
    trace(T_influx2, 101);
    delete[] _measurement;
    _measurement = charstar(config.get<const char *>("measurement"));
    if (!_measurement)
    {
        _measurement = charstar("$name");
    }
    trace(T_influx2, 102);
    delete[] _fieldKey;
    ;
    _fieldKey = charstar(config.get<const char *>("fieldkey"));
    if (!_fieldKey)
    {
        _fieldKey = charstar("value");
    }
    trace(T_influx2, 102);
    _stop = config.get<bool>("stop");

    // Build tagSet

    trace(T_influx2, 103);
    delete _tagSet;
    _tagSet = nullptr;
    JsonArray &tagset = config["tagset"];
    _staticKeySet = true;
    if (tagset.success())
    {
        trace(T_influx2, 103);
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

            // sort the measurements by measurement name

    const char *measurement = _measurement;
    _outputs->sort([this](Script* a, Script* b)->int {
        return strcmp(varStr(_measurement, a).c_str(), varStr(_measurement, b).c_str());
    });

    return true;
}
    

String influxDB_v2_uploader::varStr(const char* in, Script* script)
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
