#include "microlink_runtime.hpp"

#include <Arduino.h>
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "../../components/microlink/src/ml_transport_policy.h"

#if defined(REMOTE_BOOT_ENABLE_MICROLINK) && REMOTE_BOOT_ENABLE_MICROLINK
#include "microlink_config.generated.h"
#endif

namespace rb {

void MicrolinkRuntime::begin(bool configLocked, bool setupMode, bool wifiConnected) {
    wifiConnected_ = wifiConnected;
    configLocked_ = configLocked;
    setupMode_ = setupMode;
    tryStart(wifiConnected);
}

void MicrolinkRuntime::tick(bool wifiConnected) {
#if defined(REMOTE_BOOT_ENABLE_MICROLINK) && REMOTE_BOOT_ENABLE_MICROLINK
    if (wifiConnected_ && !wifiConnected) {
        if (handle_) microlink_destroy(handle_);
        handle_ = nullptr;
        lifecycle_.resetAfterWiFiLoss();
        Serial.println("MicroLink: Wi-Fi lost; runtime stopped");
    }
#endif
    wifiConnected_ = wifiConnected;
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

bool MicrolinkRuntime::beginSinricHandle(bool connected) {
#if defined(REMOTE_BOOT_ENABLE_MICROLINK) && REMOTE_BOOT_ENABLE_MICROLINK
    if (connected || !handle_) return true; // Never suspend an established session.
    uint32_t now = millis();
    // WebSocket upgrade needs subsequent loop() calls after synchronous TLS.
    // A bounded lease permits this progress without overlapping DERP handshakes.
    if (sinricLease_) return true;
    if (!ml_tls_admit(now, lastSinricAttempt_, esp_get_free_heap_size(),
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT))) return false;
    if (!microlink_tls_try_acquire()) return false;
    sinricLease_ = true;
    sinricServiceStarted_ = 0;
    lastSinricAttempt_ = now;
#else
    (void)connected;
#endif
    return true;
}
void MicrolinkRuntime::endSinricHandle(bool connected, void (*abortPending)()) {
#if defined(REMOTE_BOOT_ENABLE_MICROLINK) && REMOTE_BOOT_ENABLE_MICROLINK
    if (!sinricLease_) return;
    // Start AFTER synchronous TLS returns; it may itself take several seconds.
    if (!sinricServiceStarted_) sinricServiceStarted_ = millis();
    if (connected || !ml_tls_service_window(millis(), sinricServiceStarted_)) {
        // Abort an incomplete upgrade while still holding the admission gate.
        // Never leave it suspended through the disconnected retry cooldown.
        if (!connected) abortPending();
        lastSinricAttempt_ = millis();
        microlink_tls_release();
        sinricLease_ = false;
    }
#else
    (void)connected;
    (void)abortPending;
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
        microlink_diagnostics_t diagnostics{};
        microlink_get_diagnostics(handle_, &diagnostics);
        result.controlOnline = wifiConnected_ && diagnostics.control_online;
        result.derpOnline = wifiConnected_ && diagnostics.derp_online;
        result.serverInfo = result.derpOnline && diagnostics.derp_server_info;
        result.mapUpdates = diagnostics.map_updates;
        result.reconnects = diagnostics.reconnects;
        result.tlsDeferred = diagnostics.tls_deferred;
        result.encryptedRx = diagnostics.wg_encrypted_rx;
        result.authenticatedRx = diagnostics.wg_authenticated_rx;
        result.authenticatedAgeMs = diagnostics.last_authenticated_ms ? millis()-diagnostics.last_authenticated_ms : 0;
        result.connected = ml_remote_available(wifiConnected_, result.controlOnline, diagnostics.last_authenticated_ms, millis());
        if (!wifiConnected_) result.state = "wifi_offline";
        else if (result.controlOnline && !result.connected) result.state = "peer_wait";
        const uint32_t vpnIp = microlink_get_vpn_ip(handle_);
        if (vpnIp) microlink_ip_to_str(vpnIp, result.ip);
        result.peers = diagnostics.peers;
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
