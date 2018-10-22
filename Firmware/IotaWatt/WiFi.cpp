#include "IotaWatt.h"

struct tcp_pcb {
  uint32_t ip_pcba;
  uint32_t ip_pcbb;
  uint32_t ip_pcbc;
  tcp_pcb* next;
};
extern struct tcp_pcb* tcp_tw_pcbs;
extern "C" void tcp_abort(struct tcp_pcb* pcb);

uint32_t WiFiService(struct serviceBlock* _serviceBlock) {
  static uint32_t lastDisconnect = millis();          // Time of last disconnect
  const uint32_t restartInterval = 60;              // Restart ESP if disconnected this many minutes  

  if(WiFi.status() == WL_CONNECTED){
    if(!wifiConnected){
      wifiConnected = true;
      String ip = WiFi.localIP().toString();
      log("WiFi connected. SSID=%s, IP=%s, channel=%d, RSSI %ddb", WiFi.SSID().c_str(), ip.c_str(), WiFi.channel(), WiFi.RSSI());
    }
  }
  else {
    if(wifiConnected){
      wifiConnected = false;
      lastDisconnect = millis();
      log("WiFi disconnected.");
    }
    else if((millis() - lastDisconnect) >= (60000UL * restartInterval)){
      log("WiFi disconnected more than %d minutes, restarting.", restartInterval);
      delay(500);
      ESP.restart();
    }
  }

    // Check for degraded heap.

  if(ESP.getFreeHeap() < 10000){
    log("Heap memory has degraded below safe minimum, restarting.");
    delay(500);
    ESP.restart();
  }

      // Check for expired HTTP request.

  for(int i=0; i<HTTPrequestMax; i++){
    if(HTTPrequestStart[i] && (millis() - HTTPrequestStart[i]) > 900000UL){
      log("Incomplete HTTP request detected, id %d, restarting.", HTTPrequestId[i]);
      delay(500);
      ESP.restart();
    }
  }    

      // Purge any timed out authorization sessions.

  purgeAuthSessions();

      // Temporary addition of time-wait limit code from me-no-dev's fix.
      // Will remove when fix is in general release.
  
  uint32_t twCount = 0;
  tcp_pcb* tmp = tcp_tw_pcbs;
  if(tmp){
    while(tmp->next){
      if(twCount > 5){
        tcp_abort(tmp->next);
      } else {
        tmp = tmp->next;
        twCount++;
      }      
    }
  }
 
  return UNIXtime() + 1;  
}

uint32_t HTTPreserve(uint16_t id, bool lock){
  if(HTTPrequestFree == 0 || HTTPlock) return 0;
  HTTPrequestFree--;
  for(int i=0; i<HTTPrequestMax; i++){
    if(HTTPrequestStart[i] == 0){
      HTTPrequestStart[i] = millis();
      HTTPrequestId[i] = id;
      if(lock){
        HTTPlock = HTTPrequestStart[i];
      }
      return HTTPrequestStart[i];
    }
  }
  return 0;
}

void HTTPrelease(uint32_t HTTPtoken){
  for(int i=0; i<HTTPrequestMax; i++){
    if(HTTPrequestStart[i] == HTTPtoken){
      HTTPrequestStart[i] = 0;
      HTTPrequestFree++;
      if(HTTPtoken == HTTPlock){
        HTTPlock = 0;
      }
    }
  }
}