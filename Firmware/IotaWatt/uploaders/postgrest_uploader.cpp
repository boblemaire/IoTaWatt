#include "postgrest_uploader.h"
#include "splitstr.h"

/*****************************************************************************************
 * PostgREST uploader implementation
 *
 * Uploads IoTaWatt sensor data to PostgreSQL/TimescaleDB via PostgREST API.
 * PostgREST provides automatic RESTful endpoints for PostgreSQL tables.
 *
 * Expected database schema:
 * CREATE TABLE iotawatt (
 *   timestamp TIMESTAMPTZ NOT NULL,
 *   device TEXT NOT NULL,
 *   sensor TEXT NOT NULL,
 *   power DOUBLE PRECISION,
 *   pf DOUBLE PRECISION,
 *   current DOUBLE PRECISION,
 *   v DOUBLE PRECISION
 * );
 *
 * Traces are not added, but T_postgrest is defined if you need to add traces.
 ***************************************************************************************/

// uint32_t postgrest_dispatch(struct serviceBlock *serviceBlock)
// {
//     postgrest_uploader *_this = (postgrest_uploader *)serviceBlock->serviceParm;
//     uint32_t reschedule = _this->dispatch(serviceBlock);
//     if (reschedule)
//     {
//         return reschedule;
//     }
//     return 0;
// }

// uint32_t postgrest_uploader::dispatch(struct serviceBlock *serviceBlock)
// {
//     return Uploader::dispatch(serviceBlock);
// }

/*****************************************************************************************
 * Configuration parsing
 ***************************************************************************************/
// bool postgrest_uploader::configCB(const char *JsonText)
// {
//     DynamicJsonBuffer JsonBuffer;
//     JsonObject &Json = JsonBuffer.parseObject(JsonText);
//     if (!Json.success())
//     {
//         log("%s: JSON parse failed", _id);
//         return false;
//     }
//     return configCB(Json);
// }

bool postgrest_uploader::configCB(JsonObject &Json)
{
    if (Json.containsKey("table"))
    {
        delete[] _table;
        _table = charstar(Json["table"].as<char *>());
    }
    else
    {
        log("%s: table name required", _id);
        return false;
    }

    if (Json.containsKey("deviceName"))
    {
        delete[] _deviceName;
        _deviceName = charstar(Json["deviceName"].as<char *>());
    }
    else
    {
        delete[] _deviceName;
        _deviceName = charstar("$device");
    }

    if (Json.containsKey("schema"))
    {
        delete[] _schema;
        _schema = charstar(Json["schema"].as<char *>());
    }
    else
    {
        delete[] _schema;
        _schema = charstar("public");
    }

    if (Json.containsKey("jwtToken"))
    {
        delete[] _jwtToken;
        _jwtToken = charstar(Json["jwtToken"].as<char *>());
    }

    // Log successful configuration with key details
    if (_jwtToken)
    {
        log("%s: Configured for table %s.%s with JWT auth", _id, _schema, _table);
    }
    else
    {
        log("%s: Configured for table %s.%s (anonymous)", _id, _schema, _table);
    }

    return true;
}

/*****************************************************************************************
 * Parse PostgreSQL timestamp to UNIX timestamp
 * PostgreSQL timestamps can possibility include timezone offsets (although not encouraged!)
 * A few postgresSQL formats are allowed.
 ***************************************************************************************/
uint32_t postgrest_uploader::parseTimestamp(const char *timestampStr)
{
    int year, month, day, hour, minute, second;
    int tzHours = 0, tzMinutes = 0;
    char tzSign = '+';

    // PostgreSQL with timezone: "2023-10-15 14:30:25+10:30"
    if (sscanf(timestampStr, "%d-%d-%d %d:%d:%d%c%d:%d",
               &year, &month, &day, &hour, &minute, &second, &tzSign, &tzHours, &tzMinutes) == 9)
    {

        int32_t tzOffsetSeconds = (tzHours * 3600) + (tzMinutes * 60);
        if (tzSign == '-')
        {
            tzOffsetSeconds = -tzOffsetSeconds;
        }

        uint32_t utcTime = Unixtime(year, month, day, hour, minute, second);
        return utcTime - tzOffsetSeconds;
    }

    // PostgreSQL with hour-only timezone: "2023-10-15 14:30:25+10"
    if (sscanf(timestampStr, "%d-%d-%d %d:%d:%d%c%d",
               &year, &month, &day, &hour, &minute, &second, &tzSign, &tzHours) == 8)
    {

        int32_t tzOffsetSeconds = tzHours * 3600;
        if (tzSign == '-')
        {
            tzOffsetSeconds = -tzOffsetSeconds;
        }

        uint32_t utcTime = Unixtime(year, month, day, hour, minute, second);
        return utcTime - tzOffsetSeconds;
    }

    // ISO 8601 UTC: "2023-10-15T14:30:25Z"
    if (sscanf(timestampStr, "%d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour, &minute, &second) == 6)
    {
        return Unixtime(year, month, day, hour, minute, second);
    }

    // ISO 8601 without timezone: "2023-10-15T14:30:25"
    if (sscanf(timestampStr, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6)
    {
        return Unixtime(year, month, day, hour, minute, second);
    }

    // Simple format: "2023-10-15 14:30:25"
    if (sscanf(timestampStr, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6)
    {
        return Unixtime(year, month, day, hour, minute, second);
    }

    return 0;
}

/*****************************************************************************************
 * Query database for last uploaded timestamp to determine resume point
 * This is a query to the REST API (not a direct SQL query)
 ***************************************************************************************/
uint32_t postgrest_uploader::handle_query_s()
{

    String endpoint = "/";
    if (_schema && strcmp(_schema, "public") != 0)
    {
        endpoint += _schema;
        endpoint += ".";
    }
    endpoint += _table;
    endpoint += "?select=timestamp&device=eq.";
    endpoint += resolveDeviceName();
    endpoint += "&order=timestamp.desc&limit=1";

    if (!WiFi.isConnected())
    {
        return UTCtime() + 1;
    }

    HTTPGet(endpoint.c_str(), checkQuery_s);
    return 1;
}

/*****************************************************************************************
 * Process query response to set upload resume point
 ***************************************************************************************/
uint32_t postgrest_uploader::handle_checkQuery_s()
{

    // Check if async request is complete
    if (!_request || _request->readyState() != 4)
    {
        return 10;
    }

    int httpCode = _request->responseHTTPcode();
    String responseText = _request->responseText();

    if (httpCode != 200)
    {
        char message[100];
        if (httpCode < 0)
        {
            sprintf_P(message, PSTR("Query failed, code %d"), httpCode);
        }
        else
        {
            sprintf_P(message, PSTR("Query failed, code %d, response: %.50s"), httpCode, responseText.c_str());
        }
        _statusMessage = charstar(message);

        // Clean up failed request
        delete _request;
        _request = nullptr;
        // Use same as influx V1 method
        delay(5, query_s);
        return 15;
    }

    DynamicJsonBuffer JsonBuffer;
    JsonArray &jsonArray = JsonBuffer.parseArray(responseText);

    if (jsonArray.success() && jsonArray.size() > 0)
    {
        JsonObject &lastRecord = jsonArray[0];
        if (lastRecord.containsKey("timestamp"))
        {
            String timestampStr = lastRecord["timestamp"].as<String>();
            uint32_t timestamp = parseTimestamp(timestampStr.c_str());
            if (timestamp > 0)
            {
                _lastSent = timestamp;
                if (_lastSent >= MAX(Current_log.firstKey(), _uploadStartDate))
                {
                    // Clean up successful request
                    delete _request;
                    _request = nullptr;
                    _state = write_s;
                    return 1;
                }
            }
        }
    }

    // No valid timestamp found, start from configured beginning
    if (_uploadStartDate)
    {
        _lastSent = _uploadStartDate;
    }
    else
    {
        _lastSent = Current_log.firstKey();
    }

    _lastSent = MAX(_lastSent, Current_log.firstKey());
    _lastSent -= _lastSent % _interval;

    if (!_stop)
    {
        log("%s: Start posting at %s", _id, localDateString(_lastSent + _interval).c_str());
    }

    // Clean up successful request
    delete _request;
    _request = nullptr;
    _state = write_s;
    return 1;
}

/*****************************************************************************************
 * Build JSON payload and upload to PostgREST
 * Note multiple units (e.g. Watts, PF, Amps, Volts) on a single sensor are not supported.
 * The UI reflects this (i.e. you can't select multiple units for a sensor).
 * TODO: Allow multiple units per sensor.
 ***************************************************************************************/
uint32_t postgrest_uploader::handle_write_s()
{
    if (_stop)
    {
        stop();
        return 1;
    }

    uint32_t dataThreshold = _lastSent + _interval + (_interval * _bulkSend);

    if (Current_log.lastKey() < dataThreshold)
    {
        if (oldRecord)
        {
            delete oldRecord;
            oldRecord = nullptr;
            delete newRecord;
            newRecord = nullptr;
        }
        return UTCtime() + 1;
    }

    if (!oldRecord)
    {
        // swap old and new records to reduce SD card reads
        oldRecord = new IotaLogRecord;
        newRecord = new IotaLogRecord;
        newRecord->UNIXtime = _lastSent + _interval;
        Current_log.readKey(newRecord);
    }

    if (reqData.available() == 0)
    {
        reqData.print("[");
    }

    while (reqData.available() < uploaderBufferLimit &&
           newRecord->UNIXtime < Current_log.lastKey())
    {

        if (micros() > bingoTime)
        {
            // Don't hog the CPU
            return 10;
        }

        IotaLogRecord *swap = oldRecord;
        oldRecord = newRecord;
        newRecord = swap;
        newRecord->UNIXtime = oldRecord->UNIXtime + _interval;
        Current_log.readKey(newRecord);

        double elapsedHours = newRecord->logHours - oldRecord->logHours;
        if (elapsedHours == 0)
        {
            if ((newRecord->UNIXtime + _interval) <= Current_log.lastKey())
            {
                return 1;
            }
            return UTCtime() + 1;
        }

        // Format timestamp as UTC-with-a-TZ for PostgreSQL TIMESTAMPTZ
        String timestampStr = datef(oldRecord->UNIXtime, "YYYY-MM-DD hh:mm:ss+00:00");

        Script *script = _outputs->first();
        while (script)
        {
            double value = script->run(oldRecord, newRecord);
            if (value == value)
            {
                if (reqData.available() > 1)
                {
                    reqData.print(",");
                }

                reqData.printf("{\"timestamp\":\"%s\",\"device\":\"%s\",\"sensor\":\"%s\"",
                               timestampStr.c_str(),
                               resolveDeviceName().c_str(),
                               script->name());

                // Map units to database columns
                const char *units = script->getUnits();
                if (strcmp(units, "Watts") == 0 || strcmp(units, "Wh") == 0 || strcmp(units, "kWh") == 0)
                {
                    reqData.printf(",\"power\":%.*f,\"pf\":null,\"current\":null,\"v\":null",
                                   script->precision(), value);
                }
                else if (strcmp(units, "PF") == 0)
                {
                    reqData.printf(",\"power\":null,\"pf\":%.*f,\"current\":null,\"v\":null",
                                   script->precision(), value);
                }
                else if (strcmp(units, "Amps") == 0)
                {
                    reqData.printf(",\"power\":null,\"pf\":null,\"current\":%.*f,\"v\":null",
                                   script->precision(), value);
                }
                else if (strcmp(units, "Volts") == 0)
                {
                    reqData.printf(",\"power\":null,\"pf\":null,\"current\":null,\"v\":%.*f",
                                   script->precision(), value);
                }
                else
                {
                    // Skip unsupported units
                    script = script->next();
                    continue;
                }
                reqData.print("}");
            }
            script = script->next();
        }

        _lastPost = oldRecord->UNIXtime;
    }

    if (reqData.available() <= 1)
    {
        reqData.flush();
        delete oldRecord;
        oldRecord = nullptr;
        delete newRecord;
        newRecord = nullptr;
        return UTCtime() + 5;
    }

    reqData.print("]");

    delete oldRecord;
    oldRecord = nullptr;
    delete newRecord;
    newRecord = nullptr;

    String endpoint = "/";
    if (_schema && strcmp(_schema, "public") != 0)
    {
        endpoint += _schema;
        endpoint += ".";
    }
    endpoint += _table;

    HTTPPost(endpoint.c_str(), checkWrite_s, "application/json");
    return 1;
}

/*****************************************************************************************
 * Process upload response
 ***************************************************************************************/
uint32_t postgrest_uploader::handle_checkWrite_s()
{
    if (!_request)
    {
        return 10;
    }

    // Handle connection failures (readyState != 4) as failures requiring cleanup
    if (_request->readyState() != 4)
    {

        delete _request;
        _request = nullptr;
        _state = write_s;
        return UTCtime() + 10;
    }

    int httpCode = _request->responseHTTPcode();
    String responseText = _request->responseText();

    // PostgREST returns 201 for successful inserts (vs InfluxDB's 204)
    if (_request->responseHTTPcode() == 201)
    {

        delete[] _statusMessage;
        _statusMessage = nullptr;
        _lastSent = _lastPost;
        _state = write_s;
        return 1;
    }

    // Deal with failure - follow InfluxDB v1 pattern
    char msg[100];
    sprintf_P(msg, PSTR("Post failed %d"), _request->responseHTTPcode());
    delete[] _statusMessage;
    _statusMessage = charstar(msg);
    delete _request;
    _request = nullptr;
    _state = write_s;
    return UTCtime() + 10;
}

void postgrest_uploader::setRequestHeaders()
{
    _request->setReqHeader("Content-Type", "application/json");
    _request->setReqHeader("Accept", "application/json");
    _request->setReqHeader("Prefer", "return=minimal");

    if (_jwtToken)
    {
        String auth = "Bearer ";
        auth += _jwtToken;
        _request->setReqHeader("Authorization", auth.c_str());
    }
}

int postgrest_uploader::scriptCompare(Script *a, Script *b)
{
    return strcmp(a->name(), b->name());
}

/*****************************************************************************************
 * Resolve device name with variable substitution
 ***************************************************************************************/
String postgrest_uploader::resolveDeviceName()
{
    if (!_deviceName)
    {
        return String(deviceName);
    }

    String result = String(_deviceName);

    if (result.indexOf("$device") >= 0)
    {
        result.replace("$device", String(deviceName));
    }

    return result;
}

// /*****************************************************************************************
//  * Execute HTTP GET request to PostgREST endpoint
//  ***************************************************************************************/
// void postgrest_uploader::HTTPGet(const char *endpoint, states completionState)
// {
//     if (!_GETrequest)
//     {
//         _GETrequest = new GETrequest;
//     }
//     delete _GETrequest->endpoint;
//     _GETrequest->endpoint = charstar(endpoint);
//     _GETrequest->completionState = completionState;

//     if (!WiFi.isConnected())
//     {
//         log("%s: No WiFi connection", _id);
//         return;
//     }

//     if (!_request)
//     {
//         _request = new asyncHTTPrequest;
//     }

//     _request->setTimeout(30);
//     _request->setDebug(false);

//     String fullURL = _url->build();
//     fullURL += endpoint;

//     if (!_request->open("GET", fullURL.c_str()))
//     {
//         delete _request;
//         _request = nullptr;
//         return;
//     }

//     if (_jwtToken)
//     {
//         String auth = "Bearer ";
//         auth += _jwtToken;
//         _request->setReqHeader("Authorization", auth.c_str());
//     }
//     _request->setReqHeader("Accept", "application/json");
//     _request->setReqHeader("User-Agent", "IoTaWatt");

//     if (!_request->send())
//     {
//         delete _request;
//         _request = nullptr;
//     }
//     else
//     {
//         _state = completionState;
//     }
// }

// uint32_t postgrest_uploader::handle_HTTPpost_s()
// {
//     return uploader::handle_HTTPpost_s();
// }