#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace rb {
constexpr int None = -1;
constexpr size_t MaxEntries = 24;
constexpr uint32_t DispatchGuardMs = 60000;
struct Entry { uint16_t id; char name[64]; bool hidden; bool blocked; };
inline bool parseId(const char* s, int& id) {
    if (!s || strlen(s) != 4) return false;
    id = 0;
    for (int i=0;i<4;++i) {
        char c=s[i]; int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
        if(n<0) return false;
        id=id*16+n;
    }
    return true;
}
inline bool tokenEqual(const char* a,const char* b) {
    if(!a || !b || !*b) return false;
    size_t n=strlen(a),m=strlen(b); unsigned diff=static_cast<unsigned>(n^m);
    for(size_t i=0;i<m;++i) diff|=static_cast<unsigned char>((i<n?a[i]:0)^b[i]);
    return diff==0;
}
struct State {
    Entry entries[MaxEntries]{}; size_t count=0;
    int defaultTarget=None,lastSelected=None,pending=None,fallback=None,dispatchedTarget=None;
    uint32_t created=0,ttl=180000,heartbeatAt=0,heartbeatExpiry=45000,dispatchedAt=0;
    bool heartbeatSeen=false,dispatchSeen=false;
    enum Behavior { Default, Last, Exit } behavior=Default;
    int entryIndex(int id) const {
        for(size_t i=0;i<count;++i) if(entries[i].id==id) return static_cast<int>(i);
        return None;
    }
    bool append(const Entry& entry) {
        if(count>=MaxEntries||entryIndex(entry.id)!=None) return false;
        entries[count++]=entry;
        return true;
    }
    bool valid(int id) const { int index=entryIndex(id); return index!=None&&!entries[index].blocked; }
    bool online(uint32_t now) const { return heartbeatSeen && uint32_t(now-heartbeatAt)<heartbeatExpiry; }
    bool pendingValid(uint32_t now) const { return valid(pending) && uint32_t(now-created)<ttl; }
    int selected(uint32_t now) const {
        if(pendingValid(now)) return pending;
        int id=behavior==Default?defaultTarget:behavior==Last?lastSelected:None;
        return valid(id)?id:None;
    }
    int dispatch(uint32_t now) {
        int target=selected(now);
        if(target==None) return None;
        if(dispatchSeen && target==dispatchedTarget && uint32_t(now-dispatchedAt)<DispatchGuardMs) return None;
        dispatchedTarget=target; dispatchedAt=now; dispatchSeen=true;
        return target;
    }
    int request(int id,uint32_t now,bool force=false) {
        if(!valid(id)) return 400;
        if(online(now)&&!force) return 409;
        pending=lastSelected=id; created=now; dispatchSeen=false; return 202;
    }
    void heartbeat(uint32_t now,int id) {
        heartbeatAt=now; heartbeatSeen=true;
        if(pendingValid(now)&&id==pending) pending=None;
    }
    void reconcile() {
        if(!valid(defaultTarget)) defaultTarget=None;
        if(!valid(lastSelected)) lastSelected=None;
        if(!valid(pending)) pending=None;
        if(!valid(fallback)) fallback=None;
    }
};
}
