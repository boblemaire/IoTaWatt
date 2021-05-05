#ifndef updater_h
#define updater_h

bool      checkUpdate();
bool      downloadUpdate(String version);
bool      installUpdate(String version);
bool      copyUpdate(String version);
bool      unpackUpdate(String version);
long      getTablesVersion();
bool      updateTablesFile();  

#endif