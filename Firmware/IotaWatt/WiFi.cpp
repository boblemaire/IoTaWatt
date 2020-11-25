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
  static uint32_t lastDisconnect = UTCtime();       // Time of last disconnect
  const uint32_t restartInterval = 60*60;           // Restart if disconnected this many seconds
  static bool mDNSstarted = false;
  static bool LLMNRstarted = false;

  trace(T_WiFi,0);
  if(WiFi.status() == WL_CONNECTED){
    trace(T_WiFi,1);
    if(!wifiConnectTime){
      trace(T_WiFi,1);
      wifiConnectTime = UTCtime();
      String ip = WiFi.localIP().toString();
      log("WiFi connected. SSID=%s, IP=%s, channel=%d, RSSI %ddb", WiFi.SSID().c_str(), ip.c_str(), WiFi.channel(), WiFi.RSSI());
    }
    if( ! mDNSstarted){
      if (MDNS.begin(deviceName)) {
        MDNS.addService("http", "tcp", 80);
        log("MDNS responder started for hostname %s", deviceName);
        mDNSstarted = true;
      }
    }
    if( ! LLMNRstarted){
      if (LLMNR.begin(deviceName)){
        log("LLMNR responder started for hostname %s", deviceName);
        LLMNRstarted = true;
      } 
    }
  }
  else {
    trace(T_WiFi,2);
    if(wifiConnectTime){
      trace(T_WiFi,2);
      wifiConnectTime = 0;
      lastDisconnect = UTCtime();
      log("WiFi disconnected.");
    }
    else if((UTCtime() - lastDisconnect) >= restartInterval){
      log("WiFi disconnected more than %d minutes, restarting.", restartInterval / 60);
      delay(500);
      ESP.restart();
    }
  }

    // Check for degraded heap.

  trace(T_WiFi,10);
  if(ESP.getFreeHeap() < 7000){
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
    trace(T_WiFi,110,i);
    if(HTTPrequestStart[i] == HTTPtoken){
      HTTPrequestStart[i] = 0;
      HTTPrequestFree++;
      if(HTTPtoken == HTTPlock){
        HTTPlock = 0;
      }
    }
  }
}