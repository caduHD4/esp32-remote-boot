#pragma once

#include "microlink_policy.hpp"

namespace rb {

struct MicrolinkStartContext {
    bool built;
    bool configured;
    bool configLocked;
    bool setupMode;
    bool wifiConnected;
};

class MicrolinkLifecycle {
public:
    bool shouldStart(const MicrolinkStartContext& context) {
        if (attempted_) return false;
        decision_ = microlinkDecision(
            context.built,
            context.configured,
            context.configLocked,
            context.setupMode,
            context.wifiConnected
        );
        if (decision_ != MicrolinkDecision::Ready) return false;
        attempted_ = true;
        return true;
    }

    void recordStartResult(bool success) {
        failed_ = !success;
    }

    const char* state() const {
        return failed_ ? "error" : microlinkDecisionName(decision_);
    }

private:
    MicrolinkDecision decision_ = MicrolinkDecision::Disabled;
    bool attempted_ = false;
    bool failed_ = false;
};

}
