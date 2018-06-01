#include "IotaWatt.h"
#include <libb64/cencode.h>
#include "detail/mimetable.h"
#include "auth.h"

static const char AUTHORIZATION_HEADER[] PROGMEM = "Authorization";
static const char qop_auth[] PROGMEM = "qop=auth";
static const char qop_authquote[] PROGMEM = "qop=\"auth\"";
static const char WWW_Authenticate[] PROGMEM = "WWW-Authenticate";

bool auth(authLevel level){
  if(!adminH1){
    return true;
  }
  if(level == authNone){
      Serial.println("\r\nNew Auth Request None");  
      return true;  
    } 
  if(server.hasHeader(FPSTR(AUTHORIZATION_HEADER))) {
    String authReq = server.header(FPSTR(AUTHORIZATION_HEADER));
    if(level == authNone){
      Serial.println("\r\nNew Auth Request None");  
      return true;  
    } 
    Serial.printf("\r\nNew Auth Request %s:", level == authAdmin ? "admin " : "user ");
    Serial.println(authReq);
    
    if(authReq.startsWith(F("Digest"))) {
      authReq = authReq.substring(7);
           
      // extracting required parameters for RFC 2069 simpler Digest

      String _username = extractParam(authReq, F("username=\""));
      String _realm    = extractParam(authReq, F("realm=\""));
      String _nonce    = extractParam(authReq, F("nonce=\""));
      String _uri      = extractParam(authReq, F("uri=\""));
      String _response = extractParam(authReq, F("response=\""));
      String _nc       = extractParam(authReq, F("nc="), ',');
      String _cnonce   = extractParam(authReq, F("cnonce=\""));

      if((!_realm.length()) || (!_nonce.length()) || (!_uri.length()) || (!_response.length()) || (!_cnonce.length())) {
        authReq = "";
        return false;
      }
      if(level == authAdmin && !_username.equals("admin"))  {
          authReq = "";
          return false;
      }

      authSession* session = getAuthSession(_nonce.c_str(), _nc.c_str());
      if(! session){
          Serial.println("no authSession");
          return false;
      }

      String _H1 = bin2hex(adminH1, 16); 
      if(_username.equals("user")){
          Serial.println("checking user");
          _H1 = bin2hex(userH1, 16);
      }
            
      MD5Builder md5;
      md5.begin();
      if(server.method() == HTTP_GET){
        md5.add(String(F("GET:")) + _uri);
      }else if(server.method() == HTTP_POST){
        md5.add(String(F("POST:")) + _uri);
      }else if(server.method() == HTTP_PUT){
        md5.add(String(F("PUT:")) + _uri);
      }else if(server.method() == HTTP_DELETE){
        md5.add(String(F("DELETE:")) + _uri);
      }else{
        md5.add(String(F("GET:")) + _uri);
      }
      md5.calculate();
      String _H2 = md5.toString(); 
      
      md5.begin();
      if(authReq.indexOf(FPSTR(qop_auth)) != -1 || authReq.indexOf(FPSTR(qop_authquote)) != -1) {
        md5.add(_H1 + ':' + _nonce + ':' + _nc + ':' + _cnonce + F(":auth:") + _H2);
      } else {
        md5.add(_H1 + ':' + _nonce + ':' + _H2);
      }
      
      md5.calculate();
      String _responsecheck = md5.toString();
      if(_response == _responsecheck){
        session->lastUsed = UNIXtime();  
        authReq = "";
        Serial.println("authorized");
        return true;
      } 
    }
    authReq = "";
  }
  return false;
}

void requestAuth() {
  authSession* auth = newAuthSession();
  String authHeader = String("Digest realm=\"IoTaWatt\", qop=\"auth\", nonce=\"") + bin2hex(auth->nonce, 16) + "\"";
  server.sendHeader(String(FPSTR(WWW_Authenticate)), authHeader);
  Serial.printf("Auth Request: %s, uri: %s\r\n", authHeader.c_str(), server.uri().c_str());
  using namespace mime;
  server.send(401, String(FPSTR(mimeTable[html].mimeType)), F("IoTaWatt-Login"));
}

String extractParam(String& authReq, const String& param, const char delimit){
  int _begin = authReq.indexOf(param);
  if (_begin == -1) 
    return "";
  return authReq.substring(_begin+param.length(),authReq.indexOf(delimit,_begin+param.length()));
}

authSession* newAuthSession(){
    authSession* session = (authSession*) &authSessions;
    while(session->next){
        if(session->next->IP == server.client().remoteIP() && 
           UNIXtime() - session->next->lastUsed > 10 &&
           session->next->nc > 0 ){
            authSession* oldSession = session->next;
            session->next = oldSession->next;
            delete oldSession;
        } else {
            session = session->next;
        }
    }
    session->next = new authSession;
    session = session->next;
    session->IP = server.client().remoteIP();
    session->lastUsed = UNIXtime();
    getNonce(session->nonce);
    return session;
}

authSession* getAuthSession(const char* nonce, const char* nc){
    uint8_t _nonce[16];
    uint32_t _nc;
    authSession* session = (authSession*)&authSessions;
    while(session->next){
        if((session->next->lastUsed + authTimeout) < UNIXtime()){
            authSession* expSession = session->next;
            session->next = expSession->next;
            delete expSession;
        } else {
            session = session->next;
        }
    }
    if(!authSessions || strlen(nonce) != 32 || strlen(nc) == 0) return nullptr;
    hex2bin(_nonce, nonce, 16);
    _nc = strtol(nc, nullptr, 16);
    session = authSessions;
    while(session){
        if(memcmp(session->nonce, _nonce, 16) == 0  && session->nc < _nc){
            session->nc = _nc;
            return session;
        }
        session = session->next;
    }
    return nullptr;
}

void  getNonce(uint8_t* nonce){
    uint32_t* word = (uint32_t*)nonce;
    word[0] = UNIXtime();
    word[1] = RANDOM_REG32;
    word[2] = RANDOM_REG32;
    word[3] = RANDOM_REG32;
}  

String  authSetPwdH1(const char* username, String password){
    MD5Builder md5;
    md5.begin();
    md5.add(username);
    md5.add(":");
    md5.add(AUTH_REALM);
    md5.add(":");
    md5.add(password);  
    md5.calculate();
    String H1 = md5.toString();
}

void  createAdminH1(){
    adminH1 = new uint8_t[16];
    userH1 = new uint8_t[16];
    hex2bin(adminH1,calcH1("admin", AUTH_REALM, "admin").c_str(), 16);
    hex2bin(userH1,calcH1("user", AUTH_REALM, "user").c_str(), 16);
}

String calcH1(const char* username, const char* realm, const char* password){
  MD5Builder md5;
  md5.begin();
  md5.add(username);
  md5.add(":");
  md5.add(realm);
  md5.add(":");
  md5.add(password);
  md5.calculate();
  return md5.toString();
}

static const char authFilePath[] PROGMEM = "/IoTaWatt/auth.txt";

bool  authSavePwds(){
  String pwdsFilePath(FPSTR(authFilePath));
  SD.remove(pwdsFilePath.c_str()); 
  if( ! adminH1){    
    return true;
  }
  File pwdsFile = SD.open(pwdsFilePath.c_str(), FILE_WRITE);
  if( ! pwdsFile){
    return false;
  }
  pwdsFile.write(bin2hex(adminH1,16).c_str());
  if(userH1){
    pwdsFile.write(bin2hex(userH1,16).c_str());
  }
  pwdsFile.close();
  return true; 
}

void authLoadPwds(){
  char buf[32];
  String pwdsFilePath(FPSTR(authFilePath));
  delete[] adminH1;
  adminH1 = nullptr;
  delete[] userH1;
  userH1 = nullptr;
  File pwdsFile = SD.open(pwdsFilePath.c_str(), FILE_READ);
  if( ! pwdsFile){
    return;
  }
  if(pwdsFile.available() >= 32){
    pwdsFile.read(buf, 32);
    adminH1 = new uint8_t[16];
    hex2bin(adminH1, buf, 16);
  }
  if(pwdsFile.available() >= 32){
    pwdsFile.read(buf, 32);
    userH1 = new uint8_t[16];
    hex2bin(userH1, buf, 16);
  }
  pwdsFile.close();
}

