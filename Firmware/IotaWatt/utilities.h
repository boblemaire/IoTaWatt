#pragma once

/*********************************************************************************************************
 * 
 *      Various utilities that are generally useful but not necessarily part of anything in particular.
 * 
 * *******************************************************************************************************/

int strcmp_ci(const char*, const char*);            // Case insensitive strcmp()

char* charstar(const char* str);                    // Create a new char* array...
char* charstar(String str);                         // copy the argument to it...
char* charstar(const char str);                     // return a pointer.

String hashName(const char* name);                  // hash the input string to an eight character base 64 string
String formatHex(uint32_t data);                    // Convert the input to a String of hex digits
String bin2hex(const uint8_t* in, size_t len);
void   hex2bin(uint8_t* out, const char* in, size_t len); 

void base64encode(xbuf* buf);                       // Convert the contents of an xbuf to base64
String base64encode(const uint8_t* in, size_t len); // Convert the input buffer to a base64 String

String  JsonSummary(File file, int depth);          // Read a json file and return a summary Json string              
char*  JsonDetail(File file, JsonArray& locator);   // Read and compress a detail segment of a json file

String dateString(uint32_t UNIXtime);               // Convert unixtime to a data/time string
String timeString(int value);                       // Convert time to String(HH:MM:SS)