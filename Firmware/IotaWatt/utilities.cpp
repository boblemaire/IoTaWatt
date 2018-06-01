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
char* charstar(const __FlashStringHelper * str){
  if( ! str) return nullptr;
  char* ptr = new char[strlen_P((PGM_P)str)];
  strcpy_P(ptr, (PGM_P)str);
  return ptr;
}

char* charstar(const char* str){
  if( ! str) return nullptr;
  char* ptr = new char[strlen(str)+1];
  strcpy(ptr, str);
  return ptr;
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
  SHA256 sha256;
  uint8_t hash[6];
  sha256.reset();
  sha256.update(name, strlen(name));
  sha256.finalize(hash, 6);
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
  char* base64codes = new char[65];
  strcpy_P(base64codes, base64codes_P);
  size_t supply = buf->available();
  uint8_t in[3];
  uint8_t out[4];
  trace(T_base64,0);
  while(supply >= 3){
    buf->read(in,3);
    out[0] = (uint8_t) base64codes[in[0]>>2];
    out[1] = (uint8_t) base64codes[(in[0]<<4 | in[1]>>4) & 0x3f];
    out[2] = (uint8_t) base64codes[(in[1]<<2 | in[2]>>6) & 0x3f];
    out[3] = (uint8_t) base64codes[in[2] & 0x3f];
    buf->write(out, 4);
    supply -= 3;
  }
  trace(T_base64,1);
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
  delete[] base64codes;
}

String base64encode(const uint8_t* in, size_t len){
  trace(T_base64,1);
  size_t _len = len * 2 + len;
  xbuf work(_len < 64 ? _len : 64);
  work.write(in, len);
  base64encode(&work);
  return work.readString(work.available());
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
    String  JsonOut;

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
                    JsonOut += '[';
                    JsonOut += String(varBeg);
                    JsonOut += ',';
                    JsonOut += String(varLen+1);
                    _char = ']';
                }
            }
        }
        if(level < depth) JsonOut += _char;
    }
    return JsonOut;
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


String dateString(uint32_t UNIXtime){
    DateTime now = DateTime(UNIXtime + (localTimeDiff * 3600));
  
    return String(now.month()) + '/' + String(now.day()) + '/' + String(now.year()%100) + ' ' + 
          timeString(now.hour()) + ':' + timeString(now.minute()) + ':' + timeString(now.second()) + ' ';
}

String timeString(int value){
  if(value < 10) return String("0") + String(value);
  return String(value);
}
