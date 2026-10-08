#pragma once
#include <Preferences.h>
#include <ArduinoJson.h>
#include <cstdlib>
#include <memory>
#include <cstdio>
#include "multi_pc_policy.hpp"
namespace rb {
// Body blocks are verified before the atomic generation/bank selector changes.
class ConfigStore {
    Preferences& nvs;
    struct Header { uint32_t magic,generation,length,crc; };
    static constexpr uint32_t LegacyMagic=0x52423343, Magic=0x52423443;
    static constexpr size_t MaxLength=20000, ChunkSize=512;
    uint32_t generation=0;
    uint8_t bank=0;
    static const char* bankKey(uint8_t index) { return index?"bank1":"bank0"; }
    static void chunkKey(char* key,uint8_t index,size_t chunk) {
        std::snprintf(key,8,"b%u_%02u",static_cast<unsigned>(index),static_cast<unsigned>(chunk));
    }
    static uint32_t crcUpdate(uint32_t crc,const uint8_t* bytes,size_t size) {
        for(size_t i=0;i<size;++i) {
            crc^=bytes[i];
            for(int bit=0;bit<8;++bit) crc=(crc>>1)^(0xedb88320u & (0u-(crc&1)));
        }
        return crc;
    }
    class ChunkWriter {
        Preferences& nvs;
        uint8_t bank,buffer[ChunkSize];
        size_t used=0,chunk=0,total=0;
        uint32_t crc=0xffffffffu;
        bool good=true;
    public:
        ChunkWriter(Preferences& prefs,uint8_t index):nvs(prefs),bank(index) {}
        bool flush() {
            if(!good) return false;
            if(!used) return true;
            char key[8];chunkKey(key,bank,chunk++);
            good=nvs.putBytes(key,buffer,used)==used;
            used=0;
            return good;
        }
        size_t write(uint8_t value) { return write(&value,1); }
        size_t write(const uint8_t* bytes,size_t length) {
            if(!good||length>MaxLength-total) { good=false;return 0; }
            size_t copied=0;
            while(copied<length) {
                size_t count=ChunkSize-used;
                if(count>length-copied) count=length-copied;
                memcpy(buffer+used,bytes+copied,count);
                crc=crcUpdate(crc,bytes+copied,count);
                used+=count;total+=count;copied+=count;
                if(used==ChunkSize&&!flush()) break;
            }
            return good?copied:0;
        }
        size_t length() const { return total; }
        uint32_t checksum() const { return ~crc; }
    };
    class ChunkReader {
        Preferences& nvs;
        uint8_t bank,buffer[ChunkSize];
        size_t remaining,chunk=0,pos=0,available=0;
        bool good=true;
    public:
        ChunkReader(Preferences& prefs,uint8_t index,size_t length):nvs(prefs),bank(index),remaining(length) {}
        int read() {
            if(!good||!remaining) return -1;
            if(pos==available) {
                available=remaining>ChunkSize?ChunkSize:remaining;
                char key[8];chunkKey(key,bank,chunk++);
                if(nvs.getBytesLength(key)!=available||nvs.getBytes(key,buffer,available)!=available) {
                    good=false;return -1;
                }
                pos=0;
            }
            --remaining;
            return buffer[pos++];
        }
        size_t readBytes(char* output,size_t length) {
            size_t count=0;
            while(count<length) { int value=read();if(value<0) break;output[count++]=static_cast<char>(value); }
            return count;
        }
        bool valid() const { return good; }
    };
    bool readHeader(uint8_t index,Header& h) {
        if(nvs.getBytesLength(bankKey(index))!=sizeof h||nvs.getBytes(bankKey(index),&h,sizeof h)!=sizeof h) return false;
        return h.magic==Magic&&h.length<=MaxLength;
    }
    bool verifyBody(uint8_t index,const Header& h) {
        ChunkReader reader(nvs,index,h.length);
        uint32_t crc=0xffffffffu;
        for(size_t i=0;i<h.length;++i) {
            int value=reader.read();if(value<0) return false;
            uint8_t byte=static_cast<uint8_t>(value);crc=crcUpdate(crc,&byte,1);
        }
        return reader.valid()&&~crc==h.crc;
    }
    bool loadBank(uint8_t index,uint32_t committedGeneration,bool active,JsonDocument& config,uint32_t& gen) {
        const size_t size=nvs.getBytesLength(bankKey(index));
        Header h;
        if(size==sizeof h) {
            if(!readHeader(index,h)) return false;
            if(active?h.generation!=committedGeneration:!generationNewer(committedGeneration,h.generation)) return false;
            if(!verifyBody(index,h)) return false;
            ChunkReader reader(nvs,index,h.length);
            if(deserializeJson(config,reader)||!reader.valid()) return false;
        } else {
            // Existing single-blob banks migrate on their next successful save.
            if(size<sizeof h||size>MaxLength+sizeof h) return false;
            std::unique_ptr<uint8_t,decltype(&std::free)> bytes(static_cast<uint8_t*>(std::malloc(size)),&std::free);
            if(!bytes||nvs.getBytes(bankKey(index),bytes.get(),size)!=size) return false;
            memcpy(&h,bytes.get(),sizeof h);
            if(h.magic!=LegacyMagic||h.length!=size-sizeof h||h.crc!=crc32(bytes.get()+sizeof h,h.length)) return false;
            if(active?h.generation!=committedGeneration:!generationNewer(committedGeneration,h.generation)) return false;
            if(deserializeJson(config,reinterpret_cast<const char*>(bytes.get()+sizeof h),h.length)) return false;
        }
        gen=h.generation;
        return true;
    }
public:
    explicit ConfigStore(Preferences& prefs):nvs(prefs) {}
    bool exists() { return nvs.isKey("commit")||nvs.isKey("bank0")||nvs.isKey("bank1"); }
    bool load(JsonDocument& config) {
        if(!nvs.isKey("commit")) return false;
        const uint64_t committed=nvs.getULong64("commit",0);
        const uint8_t active=committed&1;
        const uint32_t committedGeneration=committed>>1;
        uint32_t gen;
        if(loadBank(active,committedGeneration,true,config,gen)) { bank=active;generation=gen;return true; }
        // A candidate newer than the selector was never committed.
        if(loadBank(1-active,committedGeneration,false,config,gen)) { bank=1-active;generation=gen;return true; }
        return false;
    }
    bool save(JsonDocument& config) {
        if(config.overflowed()) return false;
        const size_t length=measureJson(config);
        if(length>MaxLength) return false;
        const uint8_t next=1-bank;
        ChunkWriter writer(nvs,next);
        if(serializeJson(config,writer)!=length||!writer.flush()||writer.length()!=length) return false;
        Header h{Magic,generation+1,static_cast<uint32_t>(length),writer.checksum()};
        if(nvs.putBytes(bankKey(next),&h,sizeof h)!=sizeof h) return false;
        Header verified;
        if(!readHeader(next,verified)||memcmp(&verified,&h,sizeof h)!=0||!verifyBody(next,h)) return false;
        if(nvs.putULong64("commit",(static_cast<uint64_t>(h.generation)<<1)|next)!=8) return false;
        bank=next;generation=h.generation;
        return true;
    }
};
}
