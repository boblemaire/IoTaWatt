#include "IotaWatt.h"

uint32_t WiFiService(struct serviceBlock* _serviceBlock) {
  static uint32_t lastDisconnect = millis();          // Time of last disconnect
  const uint32_t disconnectRestart = 2;               // Restart ESP if disconnected this many hours  

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
      lastDisconnect = millis();
      log("WiFi disconnected.");
    }
    else if((millis() - lastDisconnect) >= (3600000UL * disconnectRestart)){
      log("WiFi disconnected more than %d hours, restarting.", disconnectRestart);
      delay(500);
      ESP.restart();
    }
  }
  return UNIXtime() + 1;  
}