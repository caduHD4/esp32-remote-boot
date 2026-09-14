#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace rb {

constexpr size_t NewCredentialMaxLength = 8;
constexpr uint8_t AuthFailureLimit = 5;
constexpr uint32_t AuthLockoutMs = 30000;

enum class SetupStep {
    Password,
    Computer,
    Boot,
    Integrations,
    Finish,
    Complete,
    Invalid
};

inline const char* setupStepName(SetupStep step) {
    switch (step) {
        case SetupStep::Password: return "password";
        case SetupStep::Computer: return "computer";
        case SetupStep::Boot: return "boot";
        case SetupStep::Integrations: return "integrations";
        case SetupStep::Finish: return "finish";
        case SetupStep::Complete: return "complete";
        default: return "invalid";
    }
}

inline SetupStep parseSetupStep(const char* value) {
    if (!value) return SetupStep::Invalid;
    for (SetupStep step : {
        SetupStep::Password, SetupStep::Computer, SetupStep::Boot,
        SetupStep::Integrations, SetupStep::Finish, SetupStep::Complete
    }) {
        if (std::strcmp(value, setupStepName(step)) == 0) return step;
    }
    return SetupStep::Invalid;
}

inline bool validNewCredential(const char* value, bool optional = false) {
    if (!value) return optional;
    const size_t length = std::strlen(value);
    return (optional && length == 0) || (length >= 1 && length <= NewCredentialMaxLength);
}

inline SetupStep stepAfterComputer(bool agentConfigured) {
    return agentConfigured ? SetupStep::Boot : SetupStep::Integrations;
}

inline bool validSetupTransition(SetupStep current, SetupStep next, bool agentConfigured) {
    switch (current) {
        case SetupStep::Password: return next == SetupStep::Computer;
        case SetupStep::Computer: return next == stepAfterComputer(agentConfigured);
        case SetupStep::Boot: return agentConfigured && next == SetupStep::Integrations;
        case SetupStep::Integrations: return next == SetupStep::Finish;
        case SetupStep::Finish: return next == SetupStep::Complete;
        default: return false;
    }
}

struct AuthThrottle {
    uint8_t failures = 0;
    uint32_t blockedUntil = 0;

    bool allowed(uint32_t now) const {
        return failures < AuthFailureLimit || static_cast<int32_t>(now - blockedUntil) >= 0;
    }

    void success() {
        failures = 0;
        blockedUntil = 0;
    }

    void failure(uint32_t now) {
        if (failures < AuthFailureLimit) ++failures;
        if (failures >= AuthFailureLimit) blockedUntil = now + AuthLockoutMs;
    }

    void refresh(uint32_t now) {
        if (failures >= AuthFailureLimit && static_cast<int32_t>(now - blockedUntil) >= 0)
            success();
    }
};

}
