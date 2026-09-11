#pragma once
#include <cstdint>

namespace rb {
class SetupNetworkPolicy {
public:
    void accessPointStarted() { accessPointActive_=true; wifiLostAt_=0; }
    bool shouldProcessDns() const { return accessPointActive_; }
    void observeWiFi(bool connected,uint32_t now) {
        if(accessPointActive_) return;
        if(connected) wifiLostAt_=0;
        else if(!wifiLostAt_) wifiLostAt_=now;
    }
    bool shouldStartRecoveryAp(bool connected,uint32_t now) {
        observeWiFi(connected,now);
        return !accessPointActive_&&!connected&&wifiLostAt_&&uint32_t(now-wifiLostAt_)>60000;
    }
private:
    bool accessPointActive_=false;
    uint32_t wifiLostAt_=0;
};
}
