#pragma once
#include <stddef.h>
#include <stdint.h>
namespace rb {
// Writer returns positive progress, zero for temporary backpressure, negative
// for a terminal failure. Both deadlines use elapsed time, including rollover.
template<class Write,class Clock,class Alive,class Yield>
bool sendAssetBytes(const uint8_t* bytes,size_t length,Write write,Clock now,
                    Alive alive,Yield yield,uint32_t totalMs,uint32_t idleMs) {
    const uint32_t started=now();uint32_t progressed=started;size_t offset=0;
    while(offset<length) {
        const uint32_t time=now();
        if(!alive()||uint32_t(time-started)>=totalMs||uint32_t(time-progressed)>=idleMs)return false;
        const size_t count=length-offset<1024?length-offset:1024;
        const int result=write(bytes+offset,count);
        if(result<0||size_t(result)>count)return false;
        if(result>0){const uint32_t completed=now();if(uint32_t(completed-started)>=totalMs||uint32_t(completed-progressed)>=idleMs)return false;offset+=size_t(result);progressed=completed;}
        if(offset<length)yield();
    }
    return true;
}
}
