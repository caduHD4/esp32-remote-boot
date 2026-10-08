#pragma once
#include <Preferences.h>
#include <ArduinoJson.h>
#include <memory>
#include "multi_pc_policy.hpp"
namespace rb {
// A failed write cannot replace the active bank. CRC and read-back catch partial blobs.
class ConfigStore {
    Preferences& nvs;
    struct Header { uint32_t magic,generation,length,crc; };
    static constexpr uint32_t Magic=0x52423343;
    uint32_t generation=0; uint8_t bank=0;
    bool read(uint8_t index,String& value,uint32_t& gen) {
        const char* key=index?"bank1":"bank0"; size_t size=nvs.getBytesLength(key);
        if(size<sizeof(Header)||size>20000+sizeof(Header)) return false;
        std::unique_ptr<uint8_t[]> bytes(new(std::nothrow) uint8_t[size]); if(!bytes) return false;
        if(nvs.getBytes(key,bytes.get(),size)!=size) return false;
        Header h; memcpy(&h,bytes.get(),sizeof h);
        if(h.magic!=Magic||h.length!=size-sizeof h||h.crc!=crc32(bytes.get()+sizeof h,h.length)) return false;
        value=""; if(!value.reserve(h.length+1)) return false;
        value.concat(reinterpret_cast<const char*>(bytes.get()+sizeof h),h.length);gen=h.generation;return true;
    }
public:
    explicit ConfigStore(Preferences& prefs):nvs(prefs) {}
    bool exists() { return nvs.isKey("commit")||nvs.isKey("bank0")||nvs.isKey("bank1"); }
    bool load(JsonDocument& config) {
        if(!nvs.isKey("commit"))return false;
        const uint64_t committed=nvs.getULong64("commit",0);const uint8_t active=committed&1;const uint32_t committedGeneration=committed>>1;String value;uint32_t gen;
        if(read(active,value,gen)&&gen==committedGeneration&&!deserializeJson(config,value)) { bank=active;generation=gen;return true; }
        // Without a committed selector an interrupted first write is not a saved configuration.
        if(read(1-active,value,gen)&&generationNewer(committedGeneration,gen)&&!deserializeJson(config,value)) { bank=1-active;generation=gen;return true; }
        return false;
    }
    bool save(JsonDocument& config) {
        const size_t length=measureJson(config); if(length>20000) return false;
        std::unique_ptr<uint8_t[]> bytes(new(std::nothrow) uint8_t[sizeof(Header)+length+1]);if(!bytes) return false;
        serializeJson(config,reinterpret_cast<char*>(bytes.get()+sizeof(Header)),length+1);
        Header h{Magic,generation+1,static_cast<uint32_t>(length),crc32(bytes.get()+sizeof(Header),length)};
        memcpy(bytes.get(),&h,sizeof h);const uint8_t next=1-bank;
        if(nvs.putBytes(next?"bank1":"bank0",bytes.get(),sizeof h+length)!=sizeof h+length) return false;
        String check;uint32_t gen;
        if(!read(next,check,gen)||gen!=h.generation||check.length()!=length||crc32(check.c_str(),length)!=h.crc) return false;
        if(nvs.putULong64("commit",(static_cast<uint64_t>(h.generation)<<1)|next)!=8) return false;
        bank=next;generation=h.generation;return true;
    }
};
}
