#pragma once

bool    spiffsFormat();
bool    spiffsBegin();
bool    spiffsFileExists(const char* path);
size_t  spiffsFileSize(const char* path);
bool    spiffsRemove(const char* path);
size_t  spiffsWrite(const char* path, String contents, bool append = false);
size_t  spiffsWrite(const char* path, uint8_t* buf, size_t len, bool append = false);
String  spiffsRead(const char* path);
String  spiffsDirectory(String path);
