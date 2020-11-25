#include "IotaWatt.h"
#include <libb64/cencode.h>
#include "detail/mimetable.h"
#include "auth.h"

static const char AUTHORIZATION_HEADER[] PROGMEM = "Authorization";
static const char qop_auth[] PROGMEM = "qop=auth";
static const char qop_authquote[] PROGMEM = "qop=\"auth\"";
static const char WWW_Authenticate[] PROGMEM = "WWW-Authenticate";
static bool       authDebug = false;

bool auth(authLevel level){

  if(authDebug) Serial.printf_P(PSTR("\nAuth: authenticate %s\n"), level==authAdmin ? "admin" : "user");

        // If no passwords or authorization not required, return true

  if(!adminH1 || level == authNone){
    return true;
  }

        // If no authorization header, return false.

  if( ! server.hasHeader(FPSTR(AUTHORIZATION_HEADER))){
    if(authDebug) Serial.printf_P(PSTR("Auth: no authorization Header."));
    return false;
  }

        // Authorization is required and there is an authorization header.

  String authReq = server.header(FPSTR(AUTHORIZATION_HEADER));
  //authReq.toLowerCase();
  if(authDebug) Serial.printf_P(PSTR("Auth: header %s\n"),authReq.c_str());

        // If authorization is not digest, return false;

  if( ! authReq.startsWith(F("Digest"))) {
    if(authDebug) Serial.printf_P(PSTR("Auth: not digest.\r"));
    return false;
  }

  authReq = authReq.substring(7);
        
        // extract required parameters for RFC 2069 simpler Digest

  String _username = extractParam(authReq, F("username="));
  String _realm    = extractParam(authReq, F("realm="));
  String _nonce    = extractParam(authReq, F("nonce="));
  String _uri      = extractParam(authReq, F("uri="));
  String _response = extractParam(authReq, F("response="));
  String _nc       = extractParam(authReq, F("nc="));
  String _cnonce   = extractParam(authReq, F("cnonce="));
  String _qop      = extractParam(authReq, F("qop="));

        // Validate required parameters present

  if((!_realm.length()) || (!_nonce.length()) || (!_uri.length()) || (!_response.length()) || (!_cnonce.length())) {
    if(authDebug) Serial.printf_P(PSTR("Auth: required parameters missing.\n"));
    return false;
  }

        // If admin level required, validate auth is for admin.

  if(level == authAdmin && (!_username.equals("admin"))){
      if(authDebug) Serial.printf_P(PSTR("Auth: admin level required.\n"));
      return false;
  }

        // See if there is an active session for this user,
        // Return false if not (caller will request authorization)

  authSession* session = getAuthSession(_nonce.c_str(), _nc.c_str());
  if(! session){
      if(authDebug) Serial.printf_P(PSTR("Auth: no active auth session.\n"));
      return false;
  }
  if(authDebug){
    Serial.printf_P(PSTR("Auth: active session nonce=%s, nc=%d, lastUsed=%d\n"), bin2hex(session->nonce,16).c_str(), session->nc, session->lastUsed);
  }

        // Get H1 for specified password

  String _H1 = bin2hex(adminH1, 16); 
  if(_username.equals("user")){
      _H1 = bin2hex(userH1, 16);
  }

        // Check the digest 
        
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

        // complete the digest depending on qop specification.
  
  md5.begin();
  if(_qop.equals("auth")){
    md5.add(_H1 + ':' + _nonce + ':' + _nc + ':' + _cnonce + F(":auth:") + _H2);
  } else {
    md5.add(_H1 + ':' + _nonce + ':' + _H2);
  }

        // Calculate authorized response
  
  md5.calculate();
  String _responsecheck = md5.toString();

        // If authorized response matches caller response,
        // authorize return true
        // Otherwise return false

  if(_response == _responsecheck){
    session->lastUsed = UTCtime();  
    return true;
  } else {
    return false;
  }
}

        // Setup session and send 401 Request Authorization.

void requestAuth() {
  char authHeader[100];
  authSession* auth = newAuthSession();
  String _deviceString = deviceName;
  //_deviceString.toLowerCase();
  snprintf_P(authHeader, 100, PSTR("Digest realm=\"%s\",qop=\"auth\",nonce=\"%s\""), _deviceString.c_str(), bin2hex(auth->nonce,16).c_str());
  server.sendHeader(String(FPSTR(WWW_Authenticate)), authHeader);
  server.send(401, F("text/html"), F("IoTaWatt-Login"));
  if(authDebug) Serial.printf_P(PSTR("Auth: requestAuth %s\n"),authHeader);
}

        // extract a parameter from authorization header String

String extractParam(String& authReq, const String& param, const char delimit){
  int _begin = authReq.indexOf(param);
  if (_begin == -1) return "";
  _begin = _begin + param.length();
  char delim = authReq[_begin];
  if(delim == '\"'){
    _begin++;
  } else {
    delim = ',';
  }
  int _end = authReq.indexOf(delim, _begin);
  if(_end == -1 && delim == ','){
    _end = authReq.length();
  }  
  String result = authReq.substring(_begin, _end);
  //if(authDebug) Serial.printf_P(PSTR("Auth: %s:%s\n"),param.c_str(), result.c_str());
  return result;
}

        // Create a new authorization session

authSession* newAuthSession(){
  authSession* session = (authSession*) &authSessions;
  while(session->next){
      if(session->next->IP == server.client().remoteIP() && 
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
  session->lastUsed = UTCtime();
  getNonce(session->nonce);
    return session;
}

        // Get existing authorization session if it exists.

authSession* getAuthSession(const char* nonce, const char* nc){
    if(!authSessions || strlen(nonce) != 32 || strlen(nc) == 0) return nullptr;
    purgeAuthSessions();
    uint8_t _nonce[16];
    uint32_t _nc;
    hex2bin(_nonce, nonce, 16);
    _nc = strtol(nc, nullptr, 16);
    authSession* session = authSessions;
    while(session){
        if(memcmp(session->nonce, _nonce, 16) == 0  && session->nc < _nc){
            session->nc = _nc;
            return session;
        }
        session = session->next;
    }
    return nullptr;
}

void  purgeAuthSessions(){
    authSession* session = (authSession*)&authSessions;
    while(session->next){
        if((session->next->lastUsed + 600) < UTCtime()){
            authSession* expSession = session->next;
            session->next = expSession->next;
            delete expSession;
        } else {
            session = session->next;
        }
    }
}

void  getNonce(uint8_t* nonce){
    uint32_t* word = (uint32_t*)nonce;
    word[0] = RANDOM_REG32;
    word[1] = RANDOM_REG32;
    word[2] = RANDOM_REG32;
    word[3] = RANDOM_REG32;
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
    pwdsFile.read((uint8_t*)buf, 32);
    adminH1 = new uint8_t[16];
    hex2bin(adminH1, buf, 16);
  }
  if(pwdsFile.available() >= 32){
    pwdsFile.read((uint8_t*)buf, 32);
    userH1 = new uint8_t[16];
    hex2bin(userH1, buf, 16);
  }
  pwdsFile.close();
}

int     authCount(){
  int count = 0;
  authSession* session = (authSession*)&authSessions;
  while(session->next){
    count++;
    session = session->next;
  }
  return count;     
}

