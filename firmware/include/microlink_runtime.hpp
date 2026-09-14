#pragma once

#include <cstdint>
#include <string>
#include "microlink_lifecycle.hpp"

#if defined(REMOTE_BOOT_ENABLE_MICROLINK) && REMOTE_BOOT_ENABLE_MICROLINK
extern "C" {
#include "microlink.h"
}
#endif

namespace rb {

struct MicrolinkSnapshot {
    bool built = false;
    bool configured = false;
    bool connected = false;
    bool controlOnline = false, derpOnline = false, serverInfo = false;
    uint32_t mapUpdates = 0, reconnects = 0, tlsDeferred = 0;
    uint32_t encryptedRx = 0, authenticatedRx = 0, authenticatedAgeMs = 0;
    const char* state = "disabled";
    char ip[16]{};
    int peers = 0;
    uint32_t heapFree = 0;
    uint32_t heapMinimum = 0;
    uint32_t largestBlock = 0;
};

class MicrolinkRuntime {
public:
    void begin(bool configLocked, bool setupMode, bool wifiConnected, const char* authKey, const char* deviceName);
    void tick(bool wifiConnected);
    MicrolinkSnapshot snapshot() const;
    bool beginSinricHandle(bool connected);
    void endSinricHandle(bool connected, void (*abortPending)());

private:
    void tryStart(bool wifiConnected);

    bool wifiConnected_ = false, sinricLease_ = false;
    uint32_t lastSinricAttempt_ = 0;
    uint32_t sinricServiceStarted_ = 0;
    bool configLocked_ = false;
    bool setupMode_ = false;
    std::string authKey_, deviceName_;
    MicrolinkLifecycle lifecycle_;
#if defined(REMOTE_BOOT_ENABLE_MICROLINK) && REMOTE_BOOT_ENABLE_MICROLINK
    microlink_t* handle_ = nullptr;
#endif
};

}
