#pragma once

namespace rb {

enum class MicrolinkDecision {
    Disabled,
    NotConfigured,
    ConfigLocked,
    SetupMode,
    WifiOffline,
    Ready,
};

inline MicrolinkDecision microlinkDecision(
    bool enabled,
    bool configured,
    bool configLocked,
    bool setupMode,
    bool wifiConnected
) {
    if (!enabled) return MicrolinkDecision::Disabled;
    if (!configured) return MicrolinkDecision::NotConfigured;
    if (configLocked) return MicrolinkDecision::ConfigLocked;
    if (setupMode) return MicrolinkDecision::SetupMode;
    if (!wifiConnected) return MicrolinkDecision::WifiOffline;
    return MicrolinkDecision::Ready;
}

inline const char* microlinkDecisionName(MicrolinkDecision decision) {
    switch (decision) {
        case MicrolinkDecision::Disabled: return "disabled";
        case MicrolinkDecision::NotConfigured: return "not_configured";
        case MicrolinkDecision::ConfigLocked: return "config_locked";
        case MicrolinkDecision::SetupMode: return "setup_mode";
        case MicrolinkDecision::WifiOffline: return "wifi_offline";
        case MicrolinkDecision::Ready: return "starting";
    }
    return "unknown";
}

}
