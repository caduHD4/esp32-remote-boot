#pragma once

#include <cstdint>
#include <string>

#include "boot_state.hpp"
#include "power_command.hpp"

namespace rb {

struct WolQueue {
    static constexpr uint32_t CooldownMs = 3000;

    uint8_t configuredRepeat = 1;
    uint8_t remaining = 0;
    uint16_t port = 9;
    uint32_t intervalMs = 100;
    uint32_t dueAt = 0;
    uint32_t lastAttemptAt = 0;
    bool attempted = false;

    void configure(uint16_t nextPort, uint8_t repeat, uint32_t interval) {
        port = nextPort;
        configuredRepeat = repeat;
        intervalMs = interval;
        cancel();
        attempted = false;
        lastAttemptAt = 0;
    }

    bool enqueue(uint32_t now) {
        if (remaining || (attempted && uint32_t(now - lastAttemptAt) < CooldownMs)) return false;
        remaining = configuredRepeat;
        dueAt = now;
        return true;
    }

    bool due(uint32_t now) const {
        return remaining && static_cast<int32_t>(now - dueAt) >= 0;
    }

    void recordAttempt(uint32_t now) {
        if (!remaining) return;
        --remaining;
        lastAttemptAt = now;
        attempted = true;
        dueAt = now + intervalMs;
    }

    void cancel() {
        remaining = 0;
        dueAt = 0;
    }
};

struct ComputerRuntime {
    std::string id;
    State boot;
    WolQueue wol;
    PowerCommand power;
    std::string hostname;
    std::string os;
    std::string agentSession;
    std::string socketSentCommand;
    uint32_t discoveryGeneration = 1;
    uint32_t socketDiscoveryGeneration = 0;
    int socket = -1;
    bool rebootEnabled = false;
    bool shutdownEnabled = false;

    void reset(const char* computerId) {
        id = computerId ? computerId : "";
        boot = State{};
        wol = WolQueue{};
        power.clear();
        hostname.clear();
        os.clear();
        agentSession.clear();
        socketSentCommand.clear();
        discoveryGeneration = 1;
        socketDiscoveryGeneration = 0;
        socket = -1;
        rebootEnabled = false;
        shutdownEnabled = false;
    }

    void heartbeat(uint32_t now, int bootId, const char* nextHostname, const char* nextOs,
                   const char* session, bool canReboot, bool canShutdown) {
        boot.heartbeat(now, bootId);
        hostname = nextHostname ? nextHostname : "";
        os = nextOs ? nextOs : "";
        agentSession = session ? session : "";
        rebootEnabled = canReboot;
        shutdownEnabled = canShutdown;
    }

    void disconnectAgent() {
        socket = -1;
        boot.heartbeatSeen = false;
        power.clear();
        hostname.clear();
        os.clear();
        agentSession.clear();
        socketSentCommand.clear();
        rebootEnabled = false;
        shutdownEnabled = false;
    }

    uint32_t requestDiscovery() {
        ++discoveryGeneration;
        if (!discoveryGeneration) discoveryGeneration = 1;
        return discoveryGeneration;
    }
};

}
