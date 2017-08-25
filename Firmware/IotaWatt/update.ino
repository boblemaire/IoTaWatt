void updateFromSD(char* filename){
  uint32_t startTime = millis();
  uint32_t buffSize = 1024;
  uint8_t* buff; 
  Serial.print("Update from SD file: ");
  Serial.println(filename);
  File objFile = SD.open(filename, FILE_READ);
  UpdaterClass update;
  MD5Builder md5;
  md5.begin(); 
  if( ! objFile){
    Serial.println("open failed");
    return;
  }
  Serial.print("Object file size: ");
  Serial.println(objFile.size());
  buff = new uint8_t[buffSize];
  update.begin(objFile.size());
  for(uint32_t pos=0; pos < objFile.size(); ){
    int chunkSize = objFile.size() - pos;
    if (chunkSize > buffSize) chunkSize = buffSize;
    objFile.read(buff, chunkSize);
    md5.add(buff, chunkSize);
    int writeSize = update.write(buff, chunkSize);
    if(writeSize != chunkSize){
      Serial.print("write size: ");
      Serial.print(writeSize);
      Serial.print(", chunkSize: ");
      Serial.println(chunkSize);
    }
    pos += buffSize;
  }
  Serial.print("update complete. Time: ");
  Serial.println(millis() - startTime);
  md5.calculate();
  char md5char[33];
  md5.getChars(md5char);
  Serial.print("MD5: ");
  Serial.print(md5.toString());
  update.setMD5(md5char);
  if( ! update.end()) {
    Serial.print("Update end failed: ");
    Serial.println(update.getError());
  }
  
  
}

