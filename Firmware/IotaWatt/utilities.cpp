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
char* charstar(const char* str){
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
  const char* hexDigits = "0123456789ABCDEF";
  String str = "00000000";
  uint32_t _data = data;
  for(int i=7; i>=0; i--){
    str[i] = hexDigits[_data % 16];
    _data /= 16;
  }
  return str;
}

String bin2hex(const uint8_t* in, size_t len){
  static const char* hexcodes = "0123456789abcdef";
  String out = "";
  for(int i=0; i<len; i++){
    out += hexcodes[*in >> 4];
    out += hexcodes[*in++ & 0x0f];
  }
  return out;
}

/**************************************************************************************************
 * Convert the contents of an xbuf to base64
 * ************************************************************************************************/
void base64encode(xbuf* buf){
  const char* base64codes = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
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
  trace(T_base64,0);
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
}

String base64encode(const uint8_t* in, size_t len){
  trace(T_base64,1);
  xbuf work;
  work.write(in, len);
  base64encode(&work);
  return work.readString(work.available());
}

