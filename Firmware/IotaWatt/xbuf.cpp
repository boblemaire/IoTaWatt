#include <xbuf.h>

xbuf::xbuf()
    :_offset(0)
    ,_used(0)
    ,_free(0)
    ,_head(0)
    ,_tail(0)
    {  }

//*******************************************************************************************************************
xbuf::~xbuf(){
    flush();
}

//*******************************************************************************************************************
size_t      xbuf::write(const uint8_t byte){
    write((uint8_t*) &byte, 1);
}

//*******************************************************************************************************************
size_t      xbuf::write(const char* buf){
    write((uint8_t*)buf, strlen(buf));
}

//*******************************************************************************************************************
size_t      xbuf::write(String string){
    write((uint8_t*)string.c_str(), string.length());
}

//*******************************************************************************************************************
size_t      xbuf::write(const uint8_t* buf, const size_t len){
    size_t supply = len;
    while(supply){
        if(!_free){
            addSeg();
        }
        size_t demand = _free < supply ? _free : supply;
        memcpy(_tail->data + ((_offset + _used) % SEGMENT_SIZE), buf + (len - supply), demand);
        _free -= demand;
        _used += demand;
        supply -= demand;
    }
}

//*******************************************************************************************************************
uint8_t     xbuf::read(){
    uint8_t byte = 0;
    read((uint8_t*) &byte, 1);
    return byte;
}

//*******************************************************************************************************************
size_t      xbuf::read(uint8_t* buf, size_t len){
    size_t read = 0;
    while(read < len && _used){
        size_t supply = (_offset + _used) > SEGMENT_SIZE ? SEGMENT_SIZE - _offset : _used;
        size_t demand = len - read;
        size_t chunk = supply < demand ? supply : demand;
        memcpy(buf + read, _head->data + _offset, chunk);
        _offset += chunk;
        _used -= chunk;
        read += chunk;
        if(_offset == SEGMENT_SIZE){
            remSeg();
            _offset = 0;        
        }
    }
    if( ! _used){
        flush();
    }
    return read;

}

//*******************************************************************************************************************
size_t      xbuf::available(){
    return _used;
}

//*******************************************************************************************************************
int      xbuf::indexOf(const char target, const size_t begin){
    char targetstr[2] = " ";
    targetstr[0] = target;
    return indexOf(targetstr, begin);
}

//*******************************************************************************************************************
int      xbuf::indexOf(const char* target, const size_t begin){
    size_t targetLen = strlen(target);
    if(targetLen > SEGMENT_SIZE) return -1;
    size_t searchPos = _offset + begin;
    size_t searchEnd = _offset + _used - targetLen;
    if(searchPos > searchEnd) return -1;
    size_t searchSeg = searchPos / SEGMENT_SIZE;
    xseg* seg = _head;
    while(searchSeg){
        seg = seg->next;
        searchSeg --;
    }
    size_t segPos = searchPos % SEGMENT_SIZE;
    while(searchPos <= searchEnd){
        size_t compLen = targetLen;
        if(compLen <= (SEGMENT_SIZE - segPos)){
            if(memcmp(target,seg->data+segPos,compLen) == 0){
                return searchPos - _offset;
            }
        }
        else {
            size_t compLen = SEGMENT_SIZE - segPos;
            if(memcmp(target,seg->data+segPos,compLen) == 0){
                compLen = targetLen - compLen;
                if(memcmp(target+targetLen-compLen, seg->next->data, compLen) == 0){
                    return searchPos - _offset;
                }
            }  
        }
        searchPos++;
        segPos++;
        if(segPos == SEGMENT_SIZE){
            seg = seg->next;
            segPos = 0;
        } 
    }
    return -1;
}

//*******************************************************************************************************************
String      xbuf::readStringUntil(const char target){
    return readString(indexOf(target)+1);
}

//*******************************************************************************************************************
String      xbuf::readStringUntil(const char* target){
    int index = indexOf(target);
    if(index < 0) return String();
    return readString(index + strlen(target));
}

//*******************************************************************************************************************
String      xbuf::readString(int endPos){
    String result;
    if(endPos > _used){
        endPos = _used;
    }
    if(endPos > 0 && result.reserve(endPos+1)){
        while(endPos--){
            result += (char)_head->data[_offset++];
            _used--;
            if(_offset >= SEGMENT_SIZE){
                remSeg();
            }
        }
    }   
    return result;
}

//*******************************************************************************************************************
void        xbuf::flush(){
    while(_head) remSeg();
    _tail = nullptr;
    _offset = 0;
    _used = 0;
    _free = 0;
}

//*******************************************************************************************************************
void        xbuf::addSeg(){
    if(_tail){
        _tail->next = new xseg;
        _tail = _tail->next;
    }
    else {
        _tail = _head = new xseg;
    }
    _free += SEGMENT_SIZE;
}

//*******************************************************************************************************************
void        xbuf::remSeg(){
    if(_head){
        xseg *next = _head->next;
        delete _head;
        _head = next;
        if( ! _head){
            _tail = nullptr;
        }
    }   
    _offset = 0;
}

