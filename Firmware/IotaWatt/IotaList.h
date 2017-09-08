#ifndef IotaList_h
#define IotaList_h

class IotaList {
  
public:
  int16_t insertHead(void* object, char* objectName){
    listEntry* newEntry = new listEntry;
    newEntry->object = object;
    newEntry->name = objectName;
    newEntry->next = head.next;
    head.next = newEntry;
    return ++head.size;
  }
  
  int16_t insertTail(void* object, char* objectName){
    listEntry* entry = &head;
    while(entry->next != NULL) entry = entry->next;
    entry->next = new listEntry;
    entry->next->object = object;
    entry->next->name = objectName;
    return ++head.size;
  }
	
	void* removeFirst(){
    if(head.next == NULL) return NULL;
		void* object = head.next->object;
		remove(object);
		return object;	
	}
  
  int16_t remove(void* object){
    listEntry* entry = &head;
    while(entry->next != NULL){
      if(entry->next->object == object){
        listEntry* remEntry = entry->next;
        entry->next = remEntry->next;
        delete remEntry;
        return --head.size;
      }
      entry = entry->next;
    }
    return -1;
  }
  
  int16_t removeByName(const char* name){
    return remove(findByName(name));
  }
	
	void* findFirst(){
    if(head.next == NULL) return NULL;
		return head.next->object;
	}
	
	void* findNext(void* object){
    if(head.next == NULL) return NULL;
		listEntry* entry = head.next;
		while(entry->next != NULL){
      if(entry->object == object)return entry->next->object;
			entry = entry->next;
		}
		return NULL;	
	}
  
  void* findByName(const char* name){
    listEntry* entry = &head;
    while(entry->next != NULL){
      if(!strcmp(entry->next->name, name)){
        return entry->next->object;
      }
      entry = entry->next;
    }
    return NULL;
  } 
	
	int16_t size(){return head.size;}
  
private:
  
  struct listEntry {
    listEntry* next;
    union{
      void*   object;
      uint32_t size;
    };
    char*   name;
    listEntry(){next = NULL; object = NULL; name = NULL;}
  } head;
};

#endif // !IotaList_h
