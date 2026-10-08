#pragma once
#include <cstdint>
#include <cstring>
#include <cstddef>
namespace rb {
constexpr unsigned MaxPcs=4, MaxAgents=8, MaxPairings=3;
inline bool validOpaqueId(const char* s) {
    if(!s || std::strlen(s)!=32) return false;
    for(unsigned i=0;i<32;++i) if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f'))) return false;
    return true;
}
inline uint32_t crc32(const void* data,size_t length) {
    uint32_t crc=0xffffffffu; const auto* bytes=static_cast<const unsigned char*>(data);
    for(size_t i=0;i<length;++i) { crc^=bytes[i]; for(int bit=0;bit<8;++bit) crc=(crc>>1)^(0xedb88320u & (0u-(crc&1))); }
    return ~crc;
}
inline bool generationNewer(uint32_t a,uint32_t b) { return static_cast<int32_t>(a-b)>0; }
struct PairingLimits {
    uint32_t opened=0,startAt=0,lookupAt=0,blockedAt=0;
    unsigned starts=0,lookups=0; bool enabled=false,blocked=false;
    void enable(uint32_t now) { enabled=true;opened=now; }
    bool open(uint32_t now) const { return enabled&&uint32_t(now-opened)<300000; }
    bool startAllowed(uint32_t now) {
        if(uint32_t(now-startAt)>=60000) { startAt=now;starts=0; }
        if(starts>=6) return false;
        ++starts;return true;
    }
    bool lookupAllowed(uint32_t now) {
        if(blocked) { if(uint32_t(now-blockedAt)<60000) return false; blocked=false;lookups=0;lookupAt=now; }
        if(uint32_t(now-lookupAt)>=60000) { lookupAt=now;lookups=0; }
        if(lookups>=5) { blocked=true;blockedAt=now;return false; }
        ++lookups;return true;
    }
};
}
