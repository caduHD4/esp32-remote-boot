#pragma once
#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
class Preferences {
public:
    std::map<std::string,std::vector<uint8_t>> data;
    bool failBlob=false,failSelector=false,corruptReadback=false;
    std::string failKey,shortReadKey;size_t largestWrite=0,largestRead=0,writes=0,failWriteAt=0;
    bool isKey(const char* key){return data.count(key);}
    size_t getBytesLength(const char* key){return data[key].size();}
    size_t getBytes(const char* key,void* dest,size_t size){if(size>largestRead)largestRead=size;if(shortReadKey==key||data[key].size()!=size)return 0;std::memcpy(dest,data[key].data(),size);return size;}
    size_t putBytes(const char* key,const void* src,size_t size){++writes;if(size>largestWrite)largestWrite=size;if(failBlob||failKey==key||(failWriteAt&&writes==failWriteAt))return 0;auto* bytes=static_cast<const uint8_t*>(src);data[key]=std::vector<uint8_t>(bytes,bytes+size);if(corruptReadback)data[key].back()^=1;return size;}
    uint64_t getULong64(const char* key,uint64_t fallback){if(data[key].size()!=8)return fallback;uint64_t value;std::memcpy(&value,data[key].data(),8);return value;}
    size_t putULong64(const char* key,uint64_t value){if(failSelector)return 0;auto* bytes=reinterpret_cast<const uint8_t*>(&value);data[key]=std::vector<uint8_t>(bytes,bytes+8);return 8;}
};
