#pragma once

#include <cstdint>

namespace rb {
class WiFiReconnectPolicy {
public:
    void reset(uint32_t now) {
        connected_ = false;
        attempts_ = 0;
        nextAttemptAt_ = now;
        delayMs_ = InitialDelayMs;
    }

    void observe(bool connected, uint32_t now) {
        if (connected) {
            connected_ = true;
            attempts_ = 0;
            delayMs_ = InitialDelayMs;
            return;
        }
        if (connected_) {
            connected_ = false;
            attempts_ = 0;
            nextAttemptAt_ = now;
            delayMs_ = InitialDelayMs;
        }
    }

    bool due(uint32_t now) const {
        return !connected_ && static_cast<int32_t>(now - nextAttemptAt_) >= 0;
    }

    void recordAttempt(uint32_t now) {
        ++attempts_;
        nextAttemptAt_ = now + AssociationWindowMs;
    }

    bool connected() const { return connected_; }
    uint32_t attempts() const { return attempts_; }
    uint32_t retryInMs(uint32_t now) const {
        if (connected_ || static_cast<int32_t>(now - nextAttemptAt_) >= 0) return 0;
        return nextAttemptAt_ - now;
    }

private:
    static constexpr uint32_t InitialDelayMs = 1000;
    static constexpr uint32_t AssociationWindowMs = 20000;
    bool connected_ = false;
    uint32_t attempts_ = 0;
    uint32_t nextAttemptAt_ = 0;
    uint32_t delayMs_ = InitialDelayMs;
};
}
