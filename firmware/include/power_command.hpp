#pragma once
#include <stdint.h>
#include <string>

namespace rb {
// RAM-only, single-consumer command. A process restart changes the session.
struct PowerCommand {
    std::string id, action, target, session;
    uint32_t created=0;
    static constexpr uint32_t Ttl=30000;
    void clear() { id.clear(); action.clear(); target.clear(); session.clear(); }
    bool active(uint32_t now) const { return !id.empty() && uint32_t(now-created)<Ttl; }
    bool enqueue(const char* nextId,const char* nextAction,const char* nextTarget,
                 const char* nextSession,uint32_t now) {
        if(active(now)) return false;
        id=nextId; action=nextAction; target=nextTarget; session=nextSession; created=now;
        return true;
    }
    bool heartbeat(const char* currentSession,const char* ack,bool shutdownAllowed,uint32_t now) {
        if(!active(now) || session!=currentSession || (action=="shutdown"&&!shutdownAllowed)) {
            clear(); return false;
        }
        if(!id.empty() && id==ack) { clear(); return true; }
        return false;
    }
};
}
