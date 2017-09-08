#ifndef msgLog_h
#define msgLog_h

#include <Arduino.h>

void msgLog(String message);
void msgLog(const char* segment1, String segment2);
void msgLog(const char* segment1, uint32_t segment2);
void msgLog(const char* segment1);
void msgLog(const char* segment1, const char* segment2);
void msgLog(const char* segment1, const char* segment2, const char* segment3);

#endif // msgLog_h
