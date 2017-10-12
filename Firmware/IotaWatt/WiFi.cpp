#include "IotaWatt.h"

uint32_t WiFiService(struct serviceBlock* _serviceBlock) {
  const byte DNS_PORT = 53;
  IPAddress apIP(192, 168, 4, 1);
  
  if(WiFi.status() == WL_CONNECTED){
    if(!wifiConnected){
      wifiConnected = true;
      String msg = "WiFi connected. SSID: " + WiFi.SSID() + ", IP: " + WiFi.localIP().toString();
      msgLog(msg);
    }
  }
  else {
    if(wifiConnected){
      wifiConnected = false;
      msgLog(F("WiFi disconnected."));
    }
  }
  return UNIXtime() + 1;  
}

void handleDisconnect(){
  returnOK();
  WiFi.disconnect();
} 
