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
// Browsers open speculative connections that may never send a request. Hold
// accepted connections aside and serve whichever sends its request first, so
// one idle socket cannot stall the single-client server for its full timeout.
template<class Client,size_t N>
class PendingConnections {
public:
    // Accept returns a falsy client once the listen queue is empty. When full,
    // the oldest idle connection yields its slot to the newest one.
    template<class Accept> void admit(Accept accept,uint32_t now) {
        for(;;) {
            Client client=accept();if(!client)return;
            if(count_==N) {
                size_t victim=0;while(victim<count_&&slots_[victim].client.available())++victim;
                if(victim==count_){client.stop();return;}
                slots_[victim].client.stop();erase(victim);
            }
            slots_[count_++]=Slot{client,now};
        }
    }
    // Moves out the oldest connection with request bytes; stops closed or expired ones.
    bool take(Client& out,uint32_t now,uint32_t idleMs) {
        for(size_t i=0;i<count_;) {
            Client& client=slots_[i].client;
            if(client.available()){out=client;erase(i);return true;}
            if(!client.connected()||uint32_t(now-slots_[i].since)>=idleMs){client.stop();erase(i);continue;}
            ++i;
        }
        return false;
    }
    size_t size() const {return count_;}
private:
    struct Slot {Client client;uint32_t since;};
    void erase(size_t index) {for(;index+1<count_;++index)slots_[index]=slots_[index+1];slots_[--count_]=Slot{};}
    Slot slots_[N]{};size_t count_=0;
};
}
