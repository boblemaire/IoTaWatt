#pragma once

/*********************************************************************************************************
 * 
 *      Various utilities that are generally useful but not necessarily part of anything in particular.
 * 
 * *******************************************************************************************************/

int strcmp_ci(const char*, const char*);            // Case insensitive strcmp()

char* charstar(const char* str, const char *str2 = nullptr);                    // Create a new char* array...
char* charstar(String str);                         // copy the argument to it...
char* charstar(const char str);                     // return a pointer.
char* charstar(const __FlashStringHelper *str, const char *str2 = nullptr);

String hashName(const char* name);                  // hash the input string to an eight character base 64 string
String formatHex(uint32_t data);                    // Convert the input to a String of hex digits
String bin2hex(const uint8_t* in, size_t len);
void   hex2bin(uint8_t* out, const char* in, size_t len); 

void   base64encode(xbuf* buf);                     // Convert the contents of an xbuf to base64
String base64encode(const uint8_t* in, size_t len); // Convert the input buffer to a base64 String

String JsonSummary(File file, int depth);           // Read a json file and return a summary Json string              
char*  JsonDetail(File file, JsonArray& locator);   // Read and compress a detail segment of a json file

String localDateString(uint32_t UNIXtime);          // Convert unixtime to a local data/time string
uint32_t Unixtime(int year, uint8_t month, uint8_t day, uint8_t hour=0, uint8_t minute=0, uint8_t second=0); // Convert YMDhms to unixtime
uint32_t YYYYMMDD2Unixtime(const char* YYYYMMDD);   // Convert character string YYYYMMDD to unixtime
String datef(uint32_t unixtime, const char* format = "MM/DD/YY hh:mm:ss");
int32_t HHMMSS2daytime(const char* HHMMSS, const char* format = "%2d:%2d:%2d");

bool    copyFile(const char* dest, const char* source);  // Copy a file

void hashFile(uint8_t* sha, File file);             // Get SHA256 hash of a file

int32_t parseSemanticVersion(const char *);
String   displaySemanticVersion(int32_t);