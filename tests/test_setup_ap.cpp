#include "../firmware/include/setup_ap.hpp"
#include <cassert>
#include <string>
#include <vector>
#include <iostream>

struct FakeSetup {
    std::vector<bool> starts;
    std::vector<bool> ips;
    bool dnsSucceeds=true;
    unsigned attempts=0, stops=0, dnsCalls=0, ipCalls=0;
    std::vector<std::string> events{};
    void event(const char* name) { events.emplace_back(name); }
    bool startAp(uint8_t attempt) {
        assert(attempt==++attempts);
        assert(attempt<=rb::SetupApMaxAttempts);
        return starts.at(attempt-1);
    }
    bool hasValidIp() { ++ipCalls; return ips.at(attempts-1); }
    void stopAp() { ++stops; }
    bool startDns() {
        assert(starts.at(attempts-1) && ips.at(attempts-1));
        ++dnsCalls; return dnsSucceeds;
    }
};

int main() {
    {
        FakeSetup a{{false,false,false},{}};
        const auto r=rb::startSetupAp(a);
        assert(!r.apReady && !r.dnsReady && a.attempts==3 && a.stops==3);
        assert(a.dnsCalls==0 && a.ipCalls==0);
        assert((a.events==std::vector<std::string>{"SETUP_AP_STARTING","SETUP_AP_FAILED"}));
    }
    for(unsigned success=1;success<=3;++success) {
        FakeSetup a{std::vector<bool>(success,false),std::vector<bool>(success,true)};
        a.starts.back()=true;
        const auto r=rb::startSetupAp(a);
        assert(r.apReady && r.dnsReady && a.attempts==success && a.stops==success-1);
        assert(a.dnsCalls==1);
        assert((a.events==std::vector<std::string>{"SETUP_AP_STARTING","SETUP_AP_STARTED","SETUP_DNS_STARTED"}));
    }
    {
        FakeSetup a{{true,true,true},{false,false,false}};
        const auto r=rb::startSetupAp(a);
        assert(!r.apReady && !r.dnsReady && a.attempts==3 && a.stops==3 && a.dnsCalls==0);
        assert((a.events==std::vector<std::string>{"SETUP_AP_STARTING","SETUP_AP_INVALID_IP",
            "SETUP_AP_INVALID_IP","SETUP_AP_INVALID_IP","SETUP_AP_FAILED"}));
    }
    {
        FakeSetup a{{true,false,true},{false,false,true}};
        const auto r=rb::startSetupAp(a);
        assert(r.apReady && r.dnsReady && a.attempts==3 && a.stops==2 && a.dnsCalls==1);
        assert((a.events==std::vector<std::string>{"SETUP_AP_STARTING","SETUP_AP_INVALID_IP",
            "SETUP_AP_STARTED","SETUP_DNS_STARTED"}));
    }
    {
        FakeSetup a{{true},{true}}; a.dnsSucceeds=false;
        const auto r=rb::startSetupAp(a);
        assert(r.apReady && !r.dnsReady && a.attempts==1 && a.stops==0 && a.dnsCalls==1);
        assert((a.events==std::vector<std::string>{"SETUP_AP_STARTING","SETUP_AP_STARTED","SETUP_DNS_FAILED"}));
    }
    std::cout << "Setup AP policy: 7 scenarios passed\n";
}
