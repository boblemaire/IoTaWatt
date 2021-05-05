#include "IotaWatt.h"

/**************************************************************************************************
 * Case insensitive string compare.  Works just like strcmp() just case insensitive 
 * ************************************************************************************************/
int strcmp_ci(const char* str1, const char* str2){
    const char* char1 = str1;
    const char* char2 = str2;
    while(*char1 || *char2){
        if(*(char1++) != *(char2++)){
            if(toupper(*(char1-1)) > toupper(*(char2-1))) return +1;
            if(toupper(*(char1-1)) < toupper(*(char2-1))) return -1;
        }
    }
    return 0;
}

/**************************************************************************************************
 * allocate a char* array, copy the argument data to it, and return the pointer. 
 * ************************************************************************************************/
char* charstar(const __FlashStringHelper * str1, const char *str2){
  int len1 = strlen_P((PGM_P)str1);
  int len2 = str2 ? strlen(str2) : 0;
  char* ptr = new char[len1 + len2 +1];
  strcpy_P(ptr, (PGM_P)str1);
  if(str2){
      strcpy(ptr + len1, str2);
  }  
  return ptr;
}

char* charstar(const char* str1, const char *str2){
  int len1 = str1 ? strlen(str1) : 0;
  int len2 = str2 ? strlen(str2) : 0;
  if(len1 + len2){
    char* ptr = new char[len1 + len2 +1];
    if(str1){
        strcpy(ptr, str1);
    }
    if(str2){
        strcpy(ptr + len1, str2);
    }  
    return ptr;
  }
  return nullptr;
}

char* charstar(String str){
  return charstar(str.c_str());
}

char* charstar(const char str){
  char* ptr = new char[2];
  ptr[0] = str;
  ptr[1] = 0;
  return ptr;
}

/**************************************************************************************************
 * Hash the input string to an eight character base64 String 
 * ************************************************************************************************/
String hashName(const char* name){
  trace(T_utility,10);  
  SHA256 sha256;
  uint8_t hash[6];
  sha256.reset();
  sha256.update(name, strlen(name));
  sha256.finalize(hash, 6);
  trace (T_utility,11);
  return base64encode(hash, 6);
}

/**************************************************************************************************
 * Convert the input to a String of hex digits.
 * ************************************************************************************************/
String formatHex(uint32_t data){
  String str;
  uint32_t _data = data;
  for(int i=7; i>=0; i--){
    str[i] = pgm_read_byte(hexcodes_P + (_data % 16));
    _data /= 16;
  }
  return str;
}

String bin2hex(const uint8_t* in, size_t len){
  char* hexcodes = new char[17];
  strcpy_P(hexcodes, hexcodes_P);
  String out;
  for(int i=0; i<len; i++){
    out += hexcodes[*in >> 4];
    out += hexcodes[*in++ & 0x0f];
  }
  delete[] hexcodes;
  return out;
}

void   hex2bin(uint8_t* out, const char* in, size_t len){
    String hexchars(FPSTR(hexcodes_P));
    for(int i=0; i<len; i++){
        out[i] = hexchars.indexOf(in[i*2]) * 16 + hexchars.indexOf(in[i*2+1]);
    }
}

/**************************************************************************************************
 * Convert the contents of an xbuf to base64
 * ************************************************************************************************/
void base64encode(xbuf* buf){ 
  trace(T_base64,10);  
  char* base64codes = new char[72];
  if( ! base64codes){
      trace(T_base64,11);
  }
  strcpy_P(base64codes, base64codes_P);
  size_t supply = buf->available();
  uint8_t in[3];
  uint8_t out[4];
  trace(T_base64,14,supply);
  while(supply >= 3){
    buf->read(in,3);
    out[0] = (uint8_t) base64codes[in[0]>>2];
    out[1] = (uint8_t) base64codes[(in[0]<<4 | in[1]>>4) & 0x3f];
    out[2] = (uint8_t) base64codes[(in[1]<<2 | in[2]>>6) & 0x3f];
    out[3] = (uint8_t) base64codes[in[2] & 0x3f];
    buf->write(out, 4);
    supply -= 3;
  }
  trace(T_base64,15,supply);
  if(supply > 0){
    in[0] = in[1] = in[2] = 0;
    buf->read(in,supply);
    out[0] = (uint8_t) base64codes[in[0]>>2];
    out[1] = (uint8_t) base64codes[(in[0]<<4 | in[1]>>4) & 0x3f];
    out[2] = (uint8_t) base64codes[(in[1]<<2 | in[2]>>6) & 0x3f];
    out[3] = (uint8_t) base64codes[in[2] & 0x3f];
    if(supply == 1) {
      out[2] = out[3] = (uint8_t) '=';
    }
    else if(supply == 2){
      out[3] = (uint8_t) '=';
    }
    buf->write(out, 4);
  }
  trace(T_base64,16);
  delete[] base64codes;
}

String base64encode(const uint8_t* in, size_t len){
  trace(T_base64,0,len);
  if(len <= 0){
     trace(T_base64,1);
     return String("");
  }
  xbuf work(128);
  work.write(in, len);
  trace(T_base64,3,len);
  base64encode(&work);
  trace(T_base64,4);
  //Serial.printf("base64 %s %d\n",work.peekString().c_str(), ESP.getFreeHeap());
  String result = work.readString(work.available());
  return result;
}

/**************************************************************************************************
 * JsonSummary, JsonDetail
 * 
 * The config file can be too big to parse into Json dynamically.
 * JsonSummary produces a summary json file to the depth specified. Objects or arrays at greater 
 * depth are represented by a JsonArray with the position and condensed (no whitespace) length.
 * JsonDetail will return the condensed object or array in a char*.
 * ************************************************************************************************/
String  JsonSummary(File file, int depth){
    int     level = -1;
    char    delim[20];
    char    _char;
    bool    string = false;
    bool    escape = false;
    int     varBeg = 0;
    int     varLen = 0;
    xbuf    JsonOut;
    
    while(file.available()){
        _char = file.read();
        if(escape){
          varLen++;
          escape = false;
        }
        else {
            varLen++;
            if(string){
                if(_char == '\"'){
                    string = false;
                }
                else if(_char == '\\'){
                    escape = true;
                }
            }
            else if(isspace(_char)){
                varLen--;
                continue;
            } 
            else if(_char == '\"'){
                string = true;
            }
            else if(_char == '{'){
                delim[++level] = '}';
                if(level == depth){
                    varBeg = file.position()-1;
                    varLen = 0;
                }
            }
            else if(_char == '['){
                delim[++level] = ']';
                if(level == depth){
                    varBeg = file.position()-1;
                    varLen = 0;
                }
            }
            else if(_char == delim[level]){
                level--;
                if(level == depth - 1){
                    JsonOut.printf_P(PSTR("[%d,%d"), varBeg, varLen+1);
                    _char = ']';
                }
            }
        }
        if(level < depth) JsonOut.print(_char);
        if(level < 0){
            break;
        }
    }
    return JsonOut.readString();
}

char*  JsonDetail(File file, JsonArray& locator){
    bool    string = false;
    bool    escape = false;
    int segLen = locator[1].as<int>();
    char* out = new char[segLen+1];
    char _char;
    file.seek(locator[0].as<int>());
    for(int i=0; i<segLen;){
        _char = file.read();
        if( ! string && isspace(_char)) continue;
        if(escape){
            escape = false;
        }
        else if(string){
            if(_char == '\"'){
                string = false;
            }
            else if(_char == '\\'){
                escape = true;
            }
        }
        else if(_char == '\"'){
            string = true;
        }
        out [i++] = _char;
    }
    out[segLen] = 0;
    return out;
}

/**************************************************************************************************
 *     Date/Time conversions between unixtime and YYYYMMDD format                                                                 *  
 * ************************************************************************************************/

/**************************************************************************************************
 *     Unixtime() - Convert year,month,day [,hour [,min [,sec]]] to Unixtime                                                                 *  
 * ************************************************************************************************/
uint32_t Unixtime(int year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second){
    if(year < 1970 || month > 12 || day > 31 || hour > 23 || minute > 59 || second > 59 ) return 0;
    uint16_t month2date[] = {0,31,59,90,120,151,181,212,243,273,304,334,365};
    uint32_t unixdays = (year-1970) * 365 + ((year-1969)/4);
    unixdays += month2date[month-1] + (day-1) + ((year % 4 == 0 && month > 2) ? 1 : 0);
    return unixdays * 86400 + hour * 3600 + minute * 60 + second;
}

/**************************************************************************************************
 *     datef(unixtime, format) Generate formatted date/time string.                                                               *  
 * ************************************************************************************************/
String datef(uint32_t unixtime, const char* format){
    const uint16_t month2date[] = {0,31,59,90,120,151,181,212,243,273,304,334,365};
    const uint16_t month2leapdate[] = {0,31,60,91,121,152,182,213,244,274,305,335,366};
    const char formatChar[] = {"YMDhms"};
    int value[6];                                           // YMDHMS
    uint32_t daytime = unixtime % 86400;                    // Get time of day
    value[3] = daytime / 3600;                              // Extract hour
    value[4] = (daytime % 3600) / 60;                       // Extract minute
    value[5] = daytime % 60;                                // extract second
    unixtime /= 86400;                                      // Convert from seconds to days
    unixtime += 365;                                        // Relative to 1969 - start of quadrenial ending with leap year       
    value[0] = 4 * (unixtime / 1461) + 1969;                // Absolute year at end of last whole quadrenial
    unixtime = unixtime % 1461;                             // Days after last whole quadrenial (-1)
    int month = 0;
    if(unixtime < 1095){                                    // Ends in one of first three non-leapyears
        value[0] += unixtime / 365;                             // Add whole years
        unixtime = unixtime % 365;                          // Days in last year (-1)
        while(unixtime >= month2date[++month]);              // Lookup month
        value[2] = unixtime - month2date[month-1] + 1;      // Compute residual days
    } else {                                                // Ends in leap year
        value[0] += 3;                                      // Count three good years    
        unixtime -= 1095;                                   // Days in last year (-1)    
        while(unixtime >= month2leapdate[++month]);          // Lookup month in leapyear
        value[2] = unixtime - month2leapdate[month-1] + 1;  // Compute residual days
    }
    value[1] = month;

        // Construct output string

    char* str = new char[30]; 
    char* out = str;
    const char* in = format;
    char* f;
    while(*in){
        if(char* f = strchr(formatChar, *in)){
            int ndx = f - formatChar;
            int len = 0;
            while(*in == *(in+1+len++));
            if(len == 1){
                out += sprintf(out, "%d", value[ndx] % 100);
            }
            else if(len == 2){
                out += sprintf(out, "%02d", value[ndx] % 100);
            } 
            else {
                out += sprintf(out, "%0*d", len, value[ndx]);
            }
            in += len;
        } else {
            *(out++) = *(in++);
        }
    }
    *out = 0;
    String result(str);
    delete[] str;
    return result;
}

String localDateString(uint32_t UNIXtime){
    return datef(UTC2Local(UNIXtime), "MM/DD/YY hh:mm:ss");
}

uint32_t YYYYMMDD2Unixtime(const char* YYYYMMDD){
    int year, month, day;
    if(sscanf(YYYYMMDD, "%4d%2d%2d", &year, &month, &day) == 3){
        return Unixtime(year, month, day);
    }
    return 0; 
}

int32_t HHMMSS2daytime(const char* HHMMSS, const char* format){
    int hour(0), minute(0), second(0);
    if(sscanf(HHMMSS, format, &hour, &minute, &second)){
        return hour * 3600 + minute * 60 + second;
    }
    return -1;
}

/**************************************************************************************************
 *     Get SHA256 hash of a file.                                                                 *  
 * ************************************************************************************************/
void hashFile(uint8_t* sha, File file){
  SHA256 sha256;
  int buffSize = 256;
  uint8_t* buff = new uint8_t[buffSize];
  size_t pos = file.position();
  file.seek(0);
  sha256.reset();
  while(file.available()){
    int bytesRead = file.read(buff,MIN(file.available(),buffSize));
    sha256.update(buff, bytesRead); 
  }
  delete[] buff;
  sha256.finalize(sha,32);
  file.seek(pos);
}

/**************************************************************************************************
 *     copyFile(dest, source) Make a copy of a file                                               *  
 * ***********************************************************************************************/
bool copyFile(const char* dest, const char* source){
    bool outSPIFFS = false;
    File outFile;
    String sourcePath = source;
    sourcePath.toLowerCase();
    File inFile = SD.open(sourcePath, FILE_READ);
    if( ! inFile) return false;
    String destPath = dest;
    destPath.toLowerCase();
    if(destPath.startsWith(F("/esp_spiffs/"))){
        outSPIFFS = true;
        spiffsWrite(destPath.substring(11).c_str(), "", false);        // Create a null file
    } else {
        if(SD.exists(dest)) SD.remove(dest);
        File outFile = SD.open(destPath, FILE_WRITE);
        if( ! outFile){
            inFile.close();
            return false;
        }
    }
    uint8_t* buff = new uint8_t[512];
    int read = 0;
    while(int read = inFile.read(buff, 512)) {
        if(outSPIFFS){
            spiffsWrite(destPath.substring(11).c_str(), buff, read, true);     // append to the file
        } else {
            outFile.write(buff, read);
        }
    }
    delete[] buff;
    inFile.close();
    if( ! outSPIFFS){
        outFile.close();
    }
    return true;
}

int32_t parseSemanticVersion(const char * ver){
    if(! ver){
        return -1;
    }
    char * ptr;
    long result = strtol(ver, &ptr, 10) << 16;
    if(*ptr == '.' || *ptr == '_'){
        long node = strtol(++ptr, &ptr, 10);
        result += node << 8;
        if(*ptr == '.' || *ptr == '_'){
            long node = strtol(++ptr, &ptr, 10);
            result += node;
        }
    }
    return result;
}

String   displaySemanticVersion(int32_t ver){
    if(ver < 0){
        return "invalid";
    }
    uint8_t *node = (uint8_t *)&ver;  // little endian
    String result(node[2]);             
    result += '.';
    result += node[1];
    result += '.';
    result += node[0];
    return result;
}