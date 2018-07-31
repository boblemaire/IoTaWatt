#pragma once

struct authSession {
    authSession*    next;
    IPAddress       IP;
    uint32_t        lastUsed;
    uint32_t        nc;
    uint8_t         nonce[16];
    authSession()
    :next(nullptr)
    ,nc(0)
    ,lastUsed(0)
    {}
};

enum authLevel {authAdmin, authUser, authNone};

bool auth(authLevel);
void requestAuth();
String extractParam(String& authReq,const String& param,const char delimit = '"');
authSession* newAuthSession();
authSession* getAuthSession(const char* nonce, const char* nc);
void  purgeAuthSessions();
void  getNonce(uint8_t* nonce);
String calcH1(const char* username, const char* realm, const char* password);
bool    authSavePwds();
void    authLoadPwds();
