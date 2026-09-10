#pragma once
#include <stdint.h>

namespace rb {
constexpr uint8_t SetupApChannel = 1;
constexpr uint8_t SetupApMaxClients = 4;
constexpr uint8_t SetupApMaxAttempts = 3;

struct SetupApResult {
    bool apReady;
    bool dnsReady;
};

// Small startup policy shared by the hardware adapter and host tests.
// An invalid IP consumes an attempt just like a failed SoftAP start.
template <typename Adapter>
SetupApResult startSetupAp(Adapter& adapter) {
    adapter.event("SETUP_AP_STARTING");
    for (uint8_t attempt = 1; attempt <= SetupApMaxAttempts; ++attempt) {
        if (adapter.startAp(attempt)) {
            if (adapter.hasValidIp()) {
                adapter.event("SETUP_AP_STARTED");
                const bool dnsReady = adapter.startDns();
                adapter.event(dnsReady ? "SETUP_DNS_STARTED" : "SETUP_DNS_FAILED");
                return {true, dnsReady};
            }
            adapter.event("SETUP_AP_INVALID_IP");
        }
        adapter.stopAp();
    }
    adapter.event("SETUP_AP_FAILED");
    return {false, false};
}
}
