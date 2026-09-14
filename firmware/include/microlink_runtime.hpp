#pragma once

#include <cstdint>
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
    const char* state = "disabled";
    char ip[16]{};
    int peers = 0;
    uint32_t heapFree = 0;
    uint32_t heapMinimum = 0;
    uint32_t largestBlock = 0;
};

class MicrolinkRuntime {
public:
    void begin(bool configLocked, bool setupMode, bool wifiConnected);
    void tick(bool wifiConnected);
    MicrolinkSnapshot snapshot() const;

private:
    void tryStart(bool wifiConnected);

    bool configLocked_ = false;
    bool setupMode_ = false;
    MicrolinkLifecycle lifecycle_;
#if defined(REMOTE_BOOT_ENABLE_MICROLINK) && REMOTE_BOOT_ENABLE_MICROLINK
    microlink_t* handle_ = nullptr;
#endif
};

}
