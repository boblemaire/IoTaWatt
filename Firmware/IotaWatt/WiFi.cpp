#include "IotaWatt.h"

uint32_t WiFiService(struct serviceBlock* _serviceBlock) {
  const byte DNS_PORT = 53;
  IPAddress apIP(192, 168, 4, 1);
  
  if(WiFi.status() == WL_CONNECTED){
    if(!wifiConnected){
      wifiConnected = true;
      String ip = WiFi.localIP().toString();
      log("WiFi connected. SSID: %s, IP: %s", WiFi.SSID().c_str(), ip.c_str());
    }
  }
  else {
    if(wifiConnected){
      wifiConnected = false;
      log("WiFi disconnected.");
    }
  }
  return UNIXtime() + 1;  
}

void handleDisconnect(){
  returnOK();
  WiFi.disconnect();
} 
