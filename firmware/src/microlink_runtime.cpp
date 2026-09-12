#include "microlink_runtime.hpp"

#include <Arduino.h>
#include "esp_heap_caps.h"
#include "esp_system.h"

#if defined(REMOTE_BOOT_ENABLE_MICROLINK) && REMOTE_BOOT_ENABLE_MICROLINK
#include "microlink_config.generated.h"
#endif

namespace rb {

void MicrolinkRuntime::begin(bool configLocked, bool setupMode, bool wifiConnected) {
    configLocked_ = configLocked;
    setupMode_ = setupMode;
    tryStart(wifiConnected);
}

void MicrolinkRuntime::tick(bool wifiConnected) {
    tryStart(wifiConnected);
}

void MicrolinkRuntime::tryStart(bool wifiConnected) {
#if defined(REMOTE_BOOT_ENABLE_MICROLINK) && REMOTE_BOOT_ENABLE_MICROLINK
    const MicrolinkStartContext context{
        true,
        REMOTE_BOOT_MICROLINK_CONFIGURED == 1,
        configLocked_,
        setupMode_,
        wifiConnected,
    };
    if (!lifecycle_.shouldStart(context)) return;

    microlink_config_t configuration{};
    configuration.auth_key = REMOTE_BOOT_MICROLINK_AUTH_KEY;
    configuration.device_name = REMOTE_BOOT_MICROLINK_DEVICE_NAME[0]
        ? REMOTE_BOOT_MICROLINK_DEVICE_NAME
        : microlink_default_device_name();
    configuration.enable_derp = true;
    configuration.enable_stun = true;
    configuration.enable_disco = true;
    configuration.max_peers = 8;

    handle_ = microlink_init(&configuration);
    const bool started = handle_ && microlink_start(handle_) == ESP_OK;
    lifecycle_.recordStartResult(started);
    if (!started) {
        if (handle_) microlink_destroy(handle_);
        handle_ = nullptr;
        Serial.println("MicroLink: initialization failed; LAN dashboard preserved");
        return;
    }
    Serial.println("MicroLink: connecting to Tailscale");
#else
    const MicrolinkStartContext context{false, false, configLocked_, setupMode_, wifiConnected};
    lifecycle_.shouldStart(context);
#endif
}

MicrolinkSnapshot MicrolinkRuntime::snapshot() const {
    MicrolinkSnapshot result;
#if defined(REMOTE_BOOT_ENABLE_MICROLINK) && REMOTE_BOOT_ENABLE_MICROLINK
    result.built = true;
    result.configured = REMOTE_BOOT_MICROLINK_CONFIGURED == 1;
    result.state = lifecycle_.state();
    if (handle_) {
        const microlink_state_t state = microlink_get_state(handle_);
        static const char* names[] = {
            "idle", "wifi_wait", "connecting", "registering",
            "connected", "reconnecting", "error"
        };
        if (state >= ML_STATE_IDLE && state <= ML_STATE_ERROR) result.state = names[state];
        result.connected = state == ML_STATE_CONNECTED;
        const uint32_t vpnIp = microlink_get_vpn_ip(handle_);
        if (vpnIp) microlink_ip_to_str(vpnIp, result.ip);
        result.peers = microlink_get_peer_count(handle_);
    }
#else
    result.state = lifecycle_.state();
#endif
    result.heapFree = esp_get_free_heap_size();
    result.heapMinimum = esp_get_minimum_free_heap_size();
    result.largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return result;
}

}
