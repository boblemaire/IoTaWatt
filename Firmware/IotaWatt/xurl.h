#pragma once

// simple xurl parse construct class.

#include <Arduino.h>

class xurl {
    public: 

        xurl()
        :_method(0)
        ,_auth(0)
        ,_domain(0)
        ,_port(0)
        ,_path(0)
        ,_query(0)
        {};
        

        ~xurl(){
            delete[] _method;
            delete[] _auth;
            delete[] _domain;
            delete[] _port;   
            delete[] _path;
            delete[] _query;
        };

            // set methods

        bool    parse(const char *_url_);
        void    method(const char *_method_);
        void    auth(const char *_auth_);
        void    domain(const char *_domain_);
        void    port(const char *_port_);
        void    path(const char *_path_);
        void    query(const char *_query_);

            // get methods

        const char*  method(){return _method;}
        const char*  auth(){return _auth;}
        const char*  domain(){return _domain;}
        const char*  port(){return _port;}
        const char*  path(){return _path;}
        const char*  query(){return _query;}

        String  build();

    private:

        char*   _method;
        char*   _auth;
        char*   _domain;
        char*   _port;
        char*   _path;
        char*   _query;
        
};