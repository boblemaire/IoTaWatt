#include "Emoncms_Uploader.h"
Uploader*emoncms = nullptr;

/*****************************************************************************************
 *          handle_query_s()
 * **************************************************************************************/
uint32_t Emoncms_uploader::handle_query_s(){
    trace(T_Emoncms,20);
    String endpoint = "/input/get?node=";
    endpoint += _node;
    _encrypted = false;
    HTTPGet(endpoint.c_str(), checkQuery_s);
    return 1;
}

/*****************************************************************************************
 *          handle_checkQuery_s()
 * **************************************************************************************/
uint32_t Emoncms_uploader::handle_checkQuery_s(){
    trace(T_Emoncms,30);
    if(_request->responseHTTPcode() != 200){
        log("%s: Query failed %d", _id, _request->responseHTTPcode());
        delay(5, query_s);
        return 15;
    }
    String response = _request->responseText();
    if (response.startsWith("\"Node does not exist\"")){
        trace(T_Emoncms,31);
        log("%s: No existing inputs found for node %s.", _id, _node);
        _lastSent = Current_log.lastKey();
        if(_uploadStartDate){
            _lastSent = _uploadStartDate;
        }
        else {
            _lastSent = Current_log.lastKey();
        }
    }

    else {
        trace(T_Emoncms,32);
        int pos = 0;
        _lastSent = _uploadStartDate;
        while((pos = response.indexOf("\"time\":", pos)) > 0) {      
            pos += 7;
            uint32_t _time = (uint32_t)response.substring(pos, response.indexOf(',',pos)).toInt();
            _lastSent = MAX(_lastSent, _time);
        }
    }
    _lastSent = MAX(_lastSent, Current_log.firstKey());
    _lastSent -= _lastSent % _interval;
    log("%s: Start posting at %s", _id, localDateString(_lastSent + _interval).c_str());
    _state = write_s;
    return 1;
}

/*****************************************************************************************
 *          handle_write_s())
 * **************************************************************************************/
uint32_t Emoncms_uploader::handle_write_s(){
    trace(T_Emoncms,60);
    
    // This is the predominant state of the uploader
    // It will wait until there is enough data for the next post
    // then build the payload and initiate the post operation.

    if(_stop){
        stop();
        return 1;
    }

    // If not enough data to post, set wait and return.

    if(Current_log.lastKey() < (_lastSent + _interval + (_interval * _bulkSend))){
        return UTCtime() + 1;
    }

    // If datalog buffers not allocated, do so now and prime latest.

    trace(T_Emoncms,60);
    if(! oldRecord){
        trace(T_Emoncms,61);
        oldRecord = new IotaLogRecord;
        newRecord = new IotaLogRecord;
        newRecord->UNIXtime = _lastSent + _interval;
        Current_log.readKey(newRecord);
    }

    // Build post transaction from datalog records.

    while(reqData.available() < uploaderBufferLimit && newRecord->UNIXtime < Current_log.lastKey()){

        if(micros() > bingoTime){
            return 15;
        }

        // Swap newRecord top oldRecord, read next into newRecord.

        trace(T_Emoncms,60);
        IotaLogRecord *swap = oldRecord;
        oldRecord = newRecord;
        newRecord = swap;
        newRecord->UNIXtime = oldRecord->UNIXtime + _interval;
        Current_log.readKey(newRecord);

        // Compute the time difference between log entries.
        // If zero, don't bother.

        trace(T_Emoncms,62);    
        double elapsedHours = newRecord->logHours - oldRecord->logHours;
        if(elapsedHours == 0){
            trace(T_Emoncms,63);
            if((newRecord->UNIXtime + _interval) <= Current_log.lastKey()){
                return 1;
            }
            return UTCtime() + 1;
        }

        // If first frame, add preamble.

        trace(T_Emoncms,62);
        if(reqData.available() == 0){
            reqData.printf_P(PSTR("time=%d&data=["), _lastSent);
        }
        else {
            reqData.write(',');
        }
        reqData.printf_P(PSTR("[%d,\"%s\""), oldRecord->UNIXtime - _lastSent, _node);
            
                // Build measurements for this interval

        if( ! _outputs){  
            trace(T_Emoncms, 63);
            for (int i = 0; i < maxInputs; i++) {
                IotaInputChannel *_input = inputChannel[i];
                double value1 = (newRecord->accum1[i] - oldRecord->accum1[i]) / elapsedHours;
                if( ! _input || value1 != value1){
                    reqData.write(",null");
                }
                else if(_input->_type == channelTypeVoltage){
                    reqData.printf(",%.1f", value1);
                }
                else if(_input->_type == channelTypePower){
                    reqData.printf(",%.1f", value1);
                }
                else{
                    reqData.printf(",%.0f", value1);
                }
            }
        }
        else {
            trace(T_Emoncms,64);
            Script* script = _outputs->first();
            int index=1;
            while(script){
                while(index++ < String(script->name()).toInt()) reqData.write(",null");
                double value1 = script->run(oldRecord, newRecord);
                if(value1 == value1){
                    if(script->precision()){
                        char valstr[20];
                        int end = sprintf(valstr,",%.*f", script->precision(), value1);
                        while(valstr[--end] == '0'){
                            valstr[end] = 0;
                        }
                        if(valstr[end] == '.'){
                            valstr[end] = 0;
                        }
                        reqData.write(valstr);
                    }
                    else {
                        reqData.printf(",%.*f", script->precision(), value1);
                    }
                } else {
                    reqData.write(",null");
                }
                script = script->next();
            }
        }
        trace(T_Emoncms,65);
        reqData.write(']');
    }
    reqData.write(']');
    _lastPost = oldRecord->UNIXtime;

    // Free the buffers

    delete oldRecord;
    oldRecord = nullptr;
    delete newRecord;
    newRecord = nullptr;

    // if not encrypted protocol, send plaintext payload.

    if(!_encrypt){
        _encrypted = false;
        HTTPPost("/input/bulk", checkWrite_s, "application/x-www-form-urlencoded");
        return 1;
    }

    // Encrypted protocol, encrypt the payload
    // Note: beginning with firmware 02_09_00, the Crypto library no longer provides AES128CBC.
    // The code here has been changed to blockchain the input and use AES128 to produce AES128CBC output.

    trace(T_Emoncms,70);  
    
    uint8_t IV[16];
    for (int i = 0; i < 16; i++){
        IV[i] = random(256);
    }

        // Initialize sha256, shaHMAC and cypher

    trace(T_Emoncms, 70);  
    SHA256* sha256 = new SHA256;
    sha256->reset();
    SHA256* shaHMAC = new SHA256;
    shaHMAC->resetHMAC(_cryptoKey,16);
    AES128* cypher = new AES128;
    cypher->setKey(_cryptoKey, 16);
    size_t supply = reqData.available();
    reqData.write(IV, 16);

    // Now chain the blocks and encrypt with AES128
    
    trace(T_Emoncms,70);     
    uint8_t* temp = new uint8_t[16+16];
    while(supply){
        size_t len = MIN(supply, 16);
        reqData.read(temp, len);
        supply -= len;
        sha256->update(temp, len);
        shaHMAC->update(temp, len);
        size_t padlen = 0;
        
            // if end of input,
            // add padding.

        if(!supply){
            padlen = 16 - (len % 16);
            for(int i=0; i<padlen; i++){
                temp[len+i] = padlen;
            }
            len += padlen;
        }
        for (int i = 0; i < 16; i++){
            IV[i] ^= temp[i];
        }

            // encrypt and output.
            
        cypher->encryptBlock(IV, IV);
        reqData.write(IV, 16);

            // if full block of padding,
            // add the extra block.

        if(padlen == 16){
            for (int i = 0; i < 16; i++){
                IV[i] ^= temp[16 + i];
            }
            cypher->encryptBlock(IV, IV);
            reqData.write(IV, 16);
        }
    }
    trace(T_Emoncms,70);
    delete[] temp;
    delete cypher;
    
    // finalize the Sha256 and shaHMAC

    trace(T_Emoncms,71); 
    sha256->finalize(_sha256, 32);
    delete[] _base64Sha;
    _base64Sha = charstar(base64encode(_sha256, 32).c_str());
    shaHMAC->finalizeHMAC(_cryptoKey, 16, _sha256, 32);
    delete sha256;
    delete shaHMAC;

    // Now base64 encode and send

    trace(T_Emoncms,71); 
    base64encode(&reqData); 
    trace(T_Emoncms,71);
    _encrypted = true;
    HTTPPost("/input/bulk", checkWrite_s, "aes128cbc");
    return 1;
}

/*****************************************************************************************
 *          handle_checkWrite_s()
 * **************************************************************************************/
uint32_t Emoncms_uploader::handle_checkWrite_s(){
    trace(T_Emoncms,91);

    // Check the result of a write transaction.
    // Usually success (204) just note the lastSent and
    // return to buildPost.

    if(_request->responseHTTPcode() == 200){
        delete[] _statusMessage;
        _statusMessage = nullptr;
        String response = _request->responseText();
        if((!_encrypted && response.startsWith("ok")) || (_encrypted && response.startsWith(_base64Sha))){
            _lastSent = _lastPost; 
            _state = write_s;
            trace(T_Emoncms,93);
            return 1;
        }
        Serial.println(response.substring(0, 80));
        String msg = "Invalid response: " + response.substring(0, 80);
        _statusMessage = charstar(msg.c_str());
    }
    
    // Deal with failure.

    else {
        trace(T_Emoncms,92);
        char msg[100];
        sprintf_P(msg, PSTR("Post failed %d"), _request->responseHTTPcode());
        delete[] _statusMessage;
        _statusMessage = charstar(msg);
        delete _request;
        _request = nullptr;
    }

    // Try it again in awhile;

    delay(10, write_s);
    return 1;
}

/*****************************************************************************************
 *          setRequestHeaders()
 * **************************************************************************************/
void Emoncms_uploader::setRequestHeaders(){
    trace(T_Emoncms,95);
    if(_encrypted){
        String auth(_userID);
        auth += ':' + bin2hex(_sha256, 32);
        _request->setReqHeader("Authorization", auth.c_str());
    }
    else {
        String auth("Bearer ");
        auth += bin2hex(_cryptoKey, 16).c_str();
        _request->setReqHeader("Authorization", auth.c_str());
    }
    trace(T_Emoncms,95);
}

//********************************************************************************************************************
//
//               CCC     OOO    N   N   FFFFF   III    GGG    CCC   BBBB
//              C   C   O   O   NN  N   F        I    G      C   C  B   B
//              C       O   O   N N N   FFF      I    G  GG  C      BBBB
//              C   C   O   O   N  NN   F        I    G   G  C   C  B   B
//               CCC     OOO    N   N   F       III    GGG    CCC   B BBB
//
//********************************************************************************************************************
bool Emoncms_uploader::configCB(JsonObject& config){
    
    trace(T_Emoncms,100);
    const char *apikey = config["apikey"].as<char *>();
    if(!apikey){
        log("%s, apikey not specified.", _id);
        return false;
    }
    hex2bin(_cryptoKey, apikey, 16);
    delete[] _node;
    _node = charstar(config["node"].as<char*>());
    if( ! _node){
        log("%s, node not specified.", _id);
        return false; 
    }
    delete[] _userID;
    _userID = charstar(config["userid"].as<char*>());
    if(!_userID || strlen(_userID) == 0){
        _encrypt = false;
        delete[] _sha256;
        _sha256 = nullptr;
    }
    else {
        _encrypt = true;
        _sha256 = new uint8_t[32];
    }
    trace(T_Emoncms,101);
    Script* script = _outputs->first();
    int index = 0;
    while(script){
        if(String(script->name()).toInt() <= index){
            log("%s: output sequence error, stopping.");
            return false;
        }
        else {
            index = String(script->name()).toInt();
        }
        script = script->next();
    }
    trace(T_Emoncms,102);
    return true; 
}
