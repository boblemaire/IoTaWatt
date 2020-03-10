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
  static uint32_t lastRetry = 0;
  const uint32_t restartInterval = 60;              // Restart ESP if disconnected this many minutes 
  const uint32_t retryInterval = 1; 

  trace(T_WiFi,0);
  if(WiFi.status() == WL_CONNECTED){
    trace(T_WiFi,1);
    if(!wifiConnected){
      trace(T_WiFi,1);
      wifiConnected = true;
      String ip = WiFi.localIP().toString();
      log("WiFi connected. SSID=%s, IP=%s, channel=%d, RSSI %ddb", WiFi.SSID().c_str(), ip.c_str(), WiFi.channel(), WiFi.RSSI());
    }
  }
  else {
    trace(T_WiFi,2);
    if(wifiConnected){
      trace(T_WiFi,2);
      wifiConnected = false;
      lastDisconnect = millis();
      log("WiFi disconnected.");
    }
    else if((millis() - lastDisconnect) >= (60000UL * restartInterval)){
      log("WiFi disconnected more than %d minutes, restarting.", restartInterval);
      delay(500);
      ESP.restart();
    }
    else if((millis() - lastRetry) > (60000UL * retryInterval)){
      lastRetry = millis();
    }
  }

    // Check for degraded heap.

  trace(T_WiFi,10);
  if(ESP.getFreeHeap() < 10000){
    trace(T_WiFi,10);
    log("Heap memory has degraded below safe minimum, restarting.");
    delay(500);
    ESP.restart();
  }

      // Check for expired HTTP request.

  trace(T_WiFi,20);
  for(int i=0; i<HTTPrequestMax; i++){
    trace(T_WiFi,21,i);
    if(HTTPrequestStart[i] && (millis() - HTTPrequestStart[i]) > 900000UL){
      trace(T_WiFi,22,i);
      log("Incomplete HTTP request detected, id %d, restarting.", HTTPrequestId[i]);
      delay(500);
      ESP.restart();
    }
  }    

      // Purge any timed out authorization sessions.

  trace(T_WiFi,30);
  purgeAuthSessions();

      // Temporary addition of time-wait limit code from me-no-dev's fix.
      // Will remove when fix is in general release.
  
  // uint32_t twCount = 0;
  // tcp_pcb* tmp = tcp_tw_pcbs;
  // if(tmp){
  //   while(tmp->next){
  //     if(twCount > 5){
  //       tcp_abort(tmp->next);
  //     } else {
  //       tmp = tmp->next;
  //       twCount++;
  //     }      
  //   }
  // }
 
  trace(T_WiFi,99);
  return UTCtime() + 1;  
}

uint32_t HTTPreserve(uint16_t id, bool lock){
  trace(T_WiFi,100,id);
  if(HTTPrequestFree == 0 || HTTPlock) return 0;
  HTTPrequestFree--;
  for(int i=0; i<HTTPrequestMax; i++){
    trace(T_WiFi,101,i);
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
  trace(T_WiFi,110);
  for(int i=0; i<HTTPrequestMax; i++){
    trace(T_WiFi,110,0);
    if(HTTPrequestStart[i] == HTTPtoken){
      HTTPrequestStart[i] = 0;
      HTTPrequestFree++;
      if(HTTPtoken == HTTPlock){
        HTTPlock = 0;
      }
    }
  }
}