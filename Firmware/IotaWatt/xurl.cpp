#include <xurl.h>

bool    xurl::parse(const char* _url_){

        Serial.println("parseurl");

        delete[] _method;
        _method = nullptr;
        delete[] _domain;
        _domain = nullptr;
        delete[] _port;
        _port = nullptr;
        delete[] _path;
        _path = nullptr;
        delete[] _query;
        _query = nullptr;

        const char* pos = _url_;

            // parse method

        const char* loc = strstr(pos, "://");
        if(loc){
            _method = new char[loc-pos+4];
            memcpy(_method, pos, loc-pos+3);
            _method[loc-pos+3] = 0;
            pos = loc + 3;
        }

            // Check for auth

        loc = strchr(pos,'@');
        if(loc){
            loc++;
            _auth = new char[loc-pos+1];
            memcpy(_auth,pos,loc-pos);
            _auth[loc-pos] = 0;
            pos = loc;
        }    

            // parse domain

        loc = pos;
        while(*loc != 0 && *loc != ':' && *loc != '/'){
            loc++;
        }
        
        if(pos == loc){
            return false;
        }

        _domain = new char[loc-pos+1];
        memcpy(_domain, pos, loc-pos);
        _domain[loc-pos] = 0;
        pos = loc;

            // parse port

        if(*pos == ':'){
            loc = strchr(pos, '/');
            if( !loc){
                loc = pos + strlen(pos);
            }
            _port = new char[loc-pos+1];
            memcpy(_port, pos, loc-pos);
            _port[loc-pos] = 0;
            pos = loc;
        }

            // parse path

        if(*pos == '/'){
            loc = pos;
            while(*loc != 0 && *loc != '?'){
                loc++;
            }
            if(pos == loc){
                return true;
            }
            _path = new char[loc-pos+1];
            memcpy(_path, pos, loc-pos);
            _path[loc-pos] = 0;
            pos = loc;
        }

            // parse query

        if(strlen(pos)){
            _query = new char[strlen(pos)+1];
            strcpy(_query, pos);
        }
        Serial.println("parsed");
        Serial.println(build());
        Serial.println("parseurldone");
        return true;
    }

void    xurl::method(const char *_method_){
        delete[] _method;
        _method = nullptr;
        if(_method_){
            _method = new char[strlen(_method_)+1];
            strcpy(_method, _method_);
        }
    }

void    xurl::auth(const char *_auth_){
        delete[] _auth;
        _auth = nullptr;
        if(_auth_){
            _auth = new char[strlen(_auth_)+1];
            strcpy(_auth, _auth_);
        }
    }

void    xurl::domain(const char *_domain_){
        delete[] _domain;
        _domain = nullptr;
        if(_domain_){
            _domain = new char[strlen(_domain_)+1];
            strcpy(_domain, _domain_);
        }
    }

void    xurl::port(const char *_port_){
        delete[] _port;
        if(_port_){
            _port = new char[strlen(_port_)+1];
            strcpy(_port, _port_);
        }
    }

void    xurl::path(const char *_path_){
        delete[] _path;
        _path = nullptr;
        if(_path_){
            _path = new char[strlen(_path_)+1];
            strcpy(_path, _path_);
        }
    }

void    xurl::query(const char *_query_){
        delete[] _query;
        _query = nullptr;
        if(_query_){
            _query = new char[strlen(_query_)+1];
            strcpy(_query, _query_);
        }
    }

String  xurl::build(){
        //int resultLen = strlen(_method) + strlen(_auth) + strlen(_domain) + strlen(_port) + strlen(_path) + strlen(_query);
        String result;
        //result.reserve(resultLen); 
        result += _method;
        result += _auth;
        result += _domain;
        result += _port;
        result += _path;
        result += _query;
        return result;
    }