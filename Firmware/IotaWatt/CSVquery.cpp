#include "IotaWatt.h"

CSVquery::CSVquery()
    :_oldRec(nullptr)
    ,_newRec(nullptr)
    ,_begin(0)
    ,_end(0)
    ,_format(formatJson)
    ,_setup(false)
    ,_header(false)
    ,_missingSkip(false)
    ,_missingNull(true)
    ,_missingZero(false)
    ,_columns(nullptr)
    {}

CSVquery::~CSVquery(){
    trace(T_CSVquery,1);
    delete _columns;
    delete _oldRec;
    delete _newRec;
}

bool    CSVquery::setup(){
    trace(T_CSVquery,10);
    if( ! (server.hasArg(F("begin")) && server.hasArg(F("end")) &&
           server.hasArg(F("group")) && server.hasArg(F("columns")))){
        return false;       
    }
    
    _begin = parseTimeArg(server.arg(F("begin")));
    _end = parseTimeArg(server.arg(F("end")));
    if(_end == 0 || _begin == 0 || _end < _begin) return false;
    trace(T_CSVquery,10);
    
    String group = server.arg(F("group"));
    group.toLowerCase();
    if(group.equals("auto")){
        uint32_t interval = (_end - _begin) / 720;
        if(interval <= 5) interval = 5;
        else if(interval <= 10) interval = 10;
        else if(interval <= 15) interval = 15;
        else if(interval <= 20) interval = 20;
        else if(interval <= 30) interval = 30;
        else if(interval % 60) interval += 60 - (interval % 60);
        _groupMult = interval;
        _groupUnits = tUnitsSeconds;
    } else {
        _groupMult = MAX(1, group.toInt());
        if(group.endsWith("s")) _groupUnits = tUnitsSeconds;
        else if(group.endsWith("m")) _groupUnits = tUnitsMinutes;
        else if(group.endsWith("h")) _groupUnits = tUnitsHours;
        else if(group.endsWith("d")) _groupUnits = tUnitsDays;
        else if(group.endsWith("w")) _groupUnits = tUnitsWeeks;
        else if(group.endsWith("mo")) _groupUnits = tUnitsMonths;
        else if(group.endsWith("y")) _groupUnits = tUnitsYears;
        else if(_groupMult > 0 && _groupMult % 5 == 0) _groupUnits = tUnitsSeconds;
        else return false;
    }

    if(server.hasArg(F("missing"))){
        String arg = server.arg(F("missing"));
        _missingSkip = (arg.equalsIgnoreCase("skip")) ? true : false;
        _missingNull = (arg.equalsIgnoreCase("null")) ? true : false;
        _missingZero = (arg.equalsIgnoreCase("zero")) ? true : false;
    }
    if(server.hasArg(F("header"))){
        String arg = server.arg(F("header"));
        if(arg.equalsIgnoreCase("yes")) _header = true;
        if(arg.equalsIgnoreCase("no")) _header = false;
    }
    if(server.hasArg(F("format"))){
        String arg = server.arg(F("format"));
        if(arg.equalsIgnoreCase("json")){
             _format = formatJson;
        }
        if(arg.equalsIgnoreCase("CSV")) _format = formatCSV;
    }
    trace(T_CSVquery,10);

        // Parse the columns array and build list of column table entries.

    trace(T_CSVquery,12);
    String array = server.arg(F("columns"));
    if(array[0] != '[' || array[array.length()-1] != ']'){
        return false;
    }
    array = array.substring(1,array.length()-1);
    String header;
    trace(T_CSVquery,13);
    while(array.length()){

            // Add a column to the list

        column* col = new column;
        col->next = _columns;
        _columns = col;
        
            // Parse the next element from the array

        trace(T_CSVquery,13);
        String element;
        int index = array.indexOf(',');
        if(index == -1){
            element = array;
            array = "";
        }
        else {
            element = array.substring(0,index);
            array.remove(0,index+1);
        }

            // Parse the name of the element
        
        trace(T_CSVquery,13);
        String name = element;
        int period = element.indexOf('.');
        if(period != -1){
            name.remove(period);
            element.remove(0,period+1);
            element.toLowerCase();
        } else {
            element = "";
        }
        if(name.length() == 0) continue;

        if(name.equalsIgnoreCase("time")){
            col->source = 'T';
            col->unit = 'I';
            col->timeLocal = true;
        }
        
        trace(T_CSVquery,14);
        if(col->source == ' '){
            for(int j=0; j<maxInputs; j++){
                if(inputChannel[j]->isActive() && name.equals(inputChannel[j]->_name)){
                    col->source = 'I';
                    if(inputChannel[j]->_type == channelTypeVoltage){
                        col->unit = 'V';
                    } else {
                        col->unit = 'P';
                    }
                    col->input = inputChannel[j]->_channel;
                    break;
                }
            }
        }

        if(col->source == ' '){
            trace(T_CSVquery,15);
            Script* script = outputs->first();
            while(script){
                if(name.equals(script->name())){
                    col->source = 'O';
                    if(strcmp(script->getUnits(),"Volts") == 0){
                        col->unit = 'V';
                    }
                    else if(strcmp(script->getUnits(),"Watts") == 0){
                        col->unit = 'P';
                    }
                    else col->decimals = script->precision();
                    col->script = script;
                    break;
                } 
                script = script->next(); 
            }  
        }

            // Process any methods

        while(element.length()){
            String method = element;
            period = element.indexOf('.');
            if(period != -1){
                method.remove(period);
                element.remove(0,period+1);
            } else {
                element = "";
            }
            if(method.length() == 0) continue;

            if(col->source == 'T'){
                if(method.equals("local")) col->timeLocal = true;
                if(method.equals("utc")) col->timeLocal = false;
                if(method.equals("iso")) col->unit = 'I';
                if(method.equals("unix")) col->unit = 'U';
            }

            else if(col->unit == 'P' && method.equals("kwh")){
                col->unit = 'E';
                col->decimals = 3;
            }

            else if(col->unit == 'E' && method.equals("delta")){
                col->delta = true;
            }

            else if(method.startsWith("d")){
                if(method.length() != 2 | method[1] < '0' | method[1] > '9') return false;
                col->decimals = method[1] - '0';
            }

            else {
                return false;
            }
        }
        trace(T_CSVquery,16);
    }

        // Convert list from LIFO to FIFO

    column* col = _columns;
    column* prev = nullptr;
    while(col){
        column* next = col->next;
        col->next = prev;
        prev = col;
        col = next;
    }
    _columns = prev;

    // Display the columns

    trace(T_CSVquery,18);
    col = _columns;
    while(col){
        String name;
        if(col->source == 'I'){
            name = inputChannel[col->input]->_name;
        }
        if(col->source == 'O'){
            name = col->script->name();
        }
        col = col->next;
    }

    trace(T_CSVquery,19);
    _setup = true;
    return true;
}

bool    CSVquery::isJson(){
    return _format == formatJson;
}
bool    CSVquery::isCSV(){
    return _format == formatCSV;
}

//*****************************************************************************************
//                  buildHeader
//*****************************************************************************************
void CSVquery::buildHeader(){

    if(_format == formatJson){
        _buffer.print("{\"header\":[");
    }
    column* col = _columns;
    bool first = true;
    while(col){
        if( ! first){
            _buffer.print(',');
            if(_format == formatCSV){
                _buffer.print(' ');
            }
        } 
        first = false;
        if(_format == formatJson){
            _buffer.print('"');
        }
        if(col->source == 'T'){
            _buffer.print("Time");
        }
        else if (col->source == 'I'){
            _buffer.print(inputChannel[col->input]->_name);
        }
        else if(col->source == 'O'){
            _buffer.print(col->script->name());
        }
        if(_format == formatJson){
            _buffer.print('"');
        }
        col = col->next;
    }

    if(_format == formatJson){
        _buffer.print("],\r\n\"data\":");
    }
    else if(_format == formatCSV){
        _buffer.print("\r\n");
    }
    return;
}

//*****************************************************************************************
//                  buildLine
//*****************************************************************************************
void CSVquery::buildLine(){
    column* col = _columns;
    double elapsedHours = _newRec->logHours - _oldRec->logHours;
    bool first = true;
        
    while(col){

        if( ! first){
            if(_format == formatJson){
                _buffer.print(',');
            } else {
                _buffer.print(", ");
            }
        } 
        first = false;

        if(elapsedHours == 0 && _missingSkip){
            continue;
        }

        else if(col->source == 'T'){
            uint32_t Time = col->timeLocal ? UTC2Local(_oldRec->UNIXtime) : _oldRec->UNIXtime;
            if(col->unit == 'U'){
                _buffer.print(Time);
            }
            else {
                _tm = gmtime((time_t*) &Time); 
                char out[80];
                strftime(out, 80, "%FT%T", _tm);
                if(_format == formatJson){
                    _buffer.printf("\"%s\"", out);    
                } else {
                    _buffer.print(out);
                }
            }
        }

        else if(elapsedHours == 0){
            if(_missingZero){
                _buffer.print('0');
            }
            else if(_missingNull){
                _buffer.print("null");
            }
        }

        else if(col->source == 'I'){
            double value = 0.0;
            if(col->unit == 'V') {
                value = (_newRec->accum1[col->input] - _oldRec->accum1[col->input]) / elapsedHours;
            } 
            else if(col->unit == 'P') {
                value = (_newRec->accum1[col->input] - _oldRec->accum1[col->input]) / elapsedHours;
            }
            else if(col->unit == 'E') {
                value = (_newRec->accum1[col->input] - (col->delta ? _oldRec->accum1[col->input] : 0)) / 1000.0;
            } 
            _buffer.printf("%.*f", col->decimals, value);
        }

        else if(col->source == 'O'){
            double value = 0.0;
             if(col->unit == 'V') {
                value = col->script->run(_oldRec, _newRec, elapsedHours);
            } 
            else if(col->unit == 'P') {
                value = col->script->run(_oldRec, _newRec, elapsedHours);
            }
            else if(col->unit == 'E') {
                value = col->script->run((col->delta ? _oldRec : nullptr), _newRec, 1000.0);
            }
            else { //if(col->unit == 'O'){
                value = col->script->run(_oldRec, _newRec, elapsedHours);
            }
            _buffer.printf("%.*f", col->decimals, value);
        }

    col = col->next;
    }

}

//*****************************************************************************************
//                  readResult
//*****************************************************************************************
size_t  CSVquery::readResult(uint8_t* buf, int len){
    if( ! _setup) return 0;
    
            // Initialize

    if( ! _oldRec){
        
        if(_header){
            buildHeader();
        //    _header = false;
        } 
        if(_format == formatJson){
            _buffer.print('[');
        }
        _oldRec = new IotaLogRecord;
        _newRec = new IotaLogRecord;
        _newRec->UNIXtime = _begin;
        logReadKey(_newRec);
        _firstLine = true;
        _lastLine = false;
    }

            // Loop to fill caller buf
            // Order is important to avoid boundary conditions.

    int written = 0;
    while(true){

            // Transfer _buffer to caller buffer

        if(_buffer.available()){
            int supply = _buffer.available();
            int demand = len - written;
            if(demand < supply){
                supply = demand;
            }
            _buffer.read(buf+written, supply);
            written += supply;
            if(written == len){
                return written;
            }
        }

            // If ended, just return zero 
        
        else if(_lastLine){
            return written;
        }

            // If at end of range,
            // Finish output stream and break.

        else if(_newRec->UNIXtime >= _end){
            if(_format == formatJson){
                _buffer.print(']');
                if(_header){
                    _buffer.print('}');
                }
            }
            _lastLine = true;
        }

            // Process next group

        else {

                // Age the log record
            
            IotaLogRecord* swapRec = _oldRec;
            _oldRec = _newRec;
            _newRec = swapRec;

                // Read group end record

            _newRec->UNIXtime = (uint32_t)nextGroup((time_t)_oldRec->UNIXtime, _groupUnits, _groupMult);
            if(_newRec->UNIXtime >= histLog.firstKey()){
                logReadKey(_newRec);
            }

                // If there is data or not skipping missing data, 
                // Generate a line.             

            if( ! (_newRec->logHours == _oldRec->logHours && _missingSkip)){

                if( ! _firstLine){
                    if(_format == formatJson){
                        _buffer.print(",\r\n");
                    }
                    if(_format == formatCSV){
                        _buffer.print("\r\n");
                    }
                }

                if(_format == formatJson){
                    _buffer.print('[');
                }

                buildLine();

                if(_format == formatJson){
                    _buffer.print(']');
                }

                _firstLine = false;
            }
        }
    }
}

time_t  CSVquery::nextGroup(time_t time, tUnits units, int32_t inc){
    time_t result;
    if(units == tUnitsAuto){
        int interval = (_end - _begin) / inc;
        if(interval % 5){
            interval += 5 - (interval & 5);
        }
        result = time + interval;
    }
    if(units == tUnitsSeconds){
        result = time + inc;
    }
    else if(units == tUnitsMinutes){
        result = time + (60 * inc) - (time % 60);
    }
    else if(units == tUnitsHours){
        result = time + (3600 * inc) - (time % 3600);
    }
    else {
        time_t local = localTime((uint32_t) time); 
        _tm = gmtime(&local);
        _tm->tm_hour = _tm->tm_min = _tm->tm_sec = 0;
        if(units == tUnitsDays){
            _tm->tm_mday += inc;
        } 
        else if(units == tUnitsWeeks){
            _tm->tm_mday += (7 * inc) - (_tm->tm_wday);
        } 
        else if(units == tUnitsMonths){
            _tm->tm_mon += inc;
            _tm->tm_mday = 1; 
        } 
        else if(units == tUnitsYears){
            _tm->tm_year += inc;
            _tm->tm_mon = 0;
            _tm->tm_mday = 1; 
        }
        result = UTCtime((uint32_t) mktime(_tm));
    }
    return (time_t)MIN(_end, result);
}

//*****************************************************************************************
//
//      parseTimeArg - parse and interpret time argument
//
//      Can be:
//  
//      UNIX time value in seconds (10 char) or ms (13 char).  Interpreted as local time
//
//      Date/time as YYYY [-MM [-DD [THH [:MM [:SS [Z]]]]]]
//                   Interpreted as local time unless ends with "Z"
//      
//      Relative local time expression:
//          base time [modifier [modifier....]]
//          base time is start of current:  y   year
//                                          mo  month
//                                          w   week
//                                          d   day
//                                          m   minute
//                                          s   last 5 seconds (equivalent to "now")
//
//          modifiers are:  +/- N period where:
//                                          y   years
//                                          mo  months   
//                                          w   weeks
//                                          d   days
//                                          m   minutes
//                                          s   seconds (N is mult of 5)    
//
//          examples period begin and end:
//          
//                                      begin       end
//          today                       d           s
//          last hour                   s-1h        s
//          last 12 hours               s-12h       s
//          yesterday                   d-1d        d
//          month to date               m           s
//          last 6 whole months         m-6m        m
//          last 6 months               s-6m        s
//          last day of last month      m-1d        m
//
//          This method can be very powerful and honors DST.  It is also independent of
//          browser time zone, using the IoTaWatt time zone.      
//
//*****************************************************************************************
time_t  CSVquery::parseTimeArg(String timeArg){
    time_t local = localTime();
    char arg[32];
    strncpy(arg, timeArg.c_str(), 31);
    arg[31] = 0;
    char* ptr = arg;

        // Check for UNIXtime.

    uint32_t unixtime = 0;
    int digits = 0;
    while(*ptr >= '0' && *ptr <= '9'){
        if(++digits <= 10){
            unixtime = (unixtime * 10) + (*ptr - '0');
        }
        ptr++;
    }
    if(*ptr == 0 && (digits == 10 || digits == 13)) return unixtime;
    
        // Check for date

    ptr = arg;
     
    int year = parseInt(&ptr);
    if(year >= 2000 && year <= 2099){
        time_t JAN_1_2000 = 946684800UL; 
        _tm = gmtime(&JAN_1_2000);
        _tm->tm_year = year - 1900;
        if(*(ptr++) == '-'){
            _tm->tm_mon = parseInt(&ptr);
            if(_tm->tm_mon >= 1 && _tm->tm_mon <=12 && *(ptr++) == '-'){
                _tm->tm_mon--;
                _tm->tm_mday = parseInt(&ptr);
                if(_tm->tm_mday >= 1 && _tm->tm_mday <=31 && *(ptr++) == 'T'){
                    _tm->tm_hour = parseInt(&ptr);
                    if(_tm->tm_hour >= 0 && _tm->tm_hour <=24 && *(ptr++) == ':'){
                        _tm->tm_min = parseInt(&ptr);
                        if(_tm->tm_min >= 0 && _tm->tm_min <= 59 && *(ptr++) == ':'){
                            _tm->tm_sec = parseInt(&ptr);
                        }
                    }
                }
            }
        }
        ptr--;
        if(*ptr == 0){
            return UTCtime((uint32_t) mktime(_tm));
        }
        else if(*ptr == 'Z' && *(ptr+1) == 0){
            return mktime(_tm);
        }
        else return 0;
    }


        // convert "mo" to 'M'

    char* out = arg;
    while(*ptr != 0){
        if(*ptr == 'm' and *(ptr+1) == 'o'){
            *out++ = 'M';
            ptr += 2;
        } else {
            *out++ = *ptr++;
        }
    }
    *out = 0;

        // Check for starting identifier

    ptr = arg;
    _tm = gmtime(&local);
    _tm->tm_sec -= _tm->tm_sec % 5;
    switch (*ptr){
        default: return 0;
        case 'y': _tm->tm_mon = 0;
        case 'M': _tm->tm_mday = 1;
        case 'd': _tm->tm_hour = 0;
        case 'h': _tm->tm_min = 0;
        case 'm': _tm->tm_sec = 0;
                  break;
        case 's': break;
        case 'w': _tm->tm_mday -= _tm->tm_wday;
                  _tm->tm_hour = 0;
                  _tm->tm_min = 0;
                  _tm->tm_sec = 0;
                  break;
    }
    ptr++;
    
        // Process any modifiers

    while(*ptr != 0){
        if(*ptr != '-' && *ptr != '+') return 0;
        long mult = strtol(ptr, &ptr, 10);
        if(mult == 0) return 0;
        switch (*ptr){
            default: return 0;
            case 'y': _tm->tm_year += mult; break;
            case 'M': _tm->tm_mon += mult; break;
            case 'w': _tm->tm_mday += 7 * mult; break;
            case 'd': _tm->tm_mday += mult; break;
            case 'h': _tm->tm_hour += mult; break;
            case 'm': _tm->tm_min += mult; break;
            case 's': _tm->tm_sec += mult - (mult % 5); break;
        }
        ptr++;
    }
    return UTCtime((uint32_t) mktime(_tm));
}

int    CSVquery::parseInt(char** ptr){
    return strtoul(*ptr, ptr, 10);
}