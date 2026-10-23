#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace rb {
constexpr size_t DiscoveryIdentitySize=149;
// One explicit byte encoding: RBI1 || ID16 || private32 || SEC1 public65 || SHA256.
// Storage never repairs or overwrites an existing blob, even a malformed one.
template<class Storage,class Generate,class Validate,class Digest>
bool prepareDiscoveryIdentity(Storage& store,uint8_t* blob,Generate generate,Validate validate,Digest digest) {
    uint8_t hash[32];
    if(store.isKey("identity")) {
        if(store.getBytesLength("identity")!=DiscoveryIdentitySize||store.getBytes("identity",blob,DiscoveryIdentitySize)!=DiscoveryIdentitySize)return false;
        if(memcmp(blob,"RBI1",4)||!digest(blob,117,hash)||memcmp(hash,blob+117,32))return false;
        return validate(blob);
    }
    memset(blob,0,DiscoveryIdentitySize);memcpy(blob,"RBI1",4);
    if(!generate(blob)||!validate(blob)||!digest(blob,117,blob+117))return false;
    if(store.putBytes("identity",blob,DiscoveryIdentitySize)!=DiscoveryIdentitySize)return false;
    uint8_t saved[DiscoveryIdentitySize];
    const bool okay=store.getBytes("identity",saved,sizeof saved)==sizeof saved&&!memcmp(saved,blob,sizeof saved);
    volatile uint8_t* wipe=saved;for(size_t i=0;i<sizeof saved;++i)wipe[i]=0;
    return okay;
}
// Sliding budget: at most four signing attempts in any second.
class DiscoveryBudget {
    uint32_t signedAt[4]{};unsigned used=0,next=0;
public:
    bool take(uint32_t now,size_t heap,size_t largest) {
        if(heap<24000||largest<12000)return false;
        if(used==4&&uint32_t(now-signedAt[next])<1000)return false;
        signedAt[next]=now;next=(next+1)%4;if(used<4)++used;return true;
    }
};
// Registration failure must not allocate/attempt every loop iteration.
class DiscoveryServiceRetry {
    uint32_t last=0;bool scheduled=false;
public:
    void reset(){scheduled=false;}
    bool due(uint32_t now){
        if(scheduled&&uint32_t(now-last)<5000)return false;
        last=now;scheduled=true;return true;
    }
};
}
