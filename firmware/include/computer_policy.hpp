#pragma once

#include <cstddef>
#include <cstring>

#include "boot_state.hpp"

namespace rb {

constexpr size_t MaxComputers = 4;
constexpr size_t MaxComputerIdLength = 32;
constexpr int ComputerNotFound = -1;
constexpr int ComputerIdRequired = -2;
constexpr int ComputerIdentityAmbiguous = -3;

struct ComputerIdentity {
    const char* id;
    const char* mac;
    const char* agentToken;
};

inline int computerHex(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

inline bool normalizeComputerMac(const char* input, char (&output)[18]) {
    if (!input) return false;
    const size_t length = std::strlen(input);
    if (length != 12 && length != 17) return false;

    char separator = 0;
    if (length == 17) {
        separator = input[2];
        if (separator != ':' && separator != '-') return false;
        for (size_t i = 2; i < 17; i += 3)
            if (input[i] != separator) return false;
    }

    size_t source = 0;
    for (size_t byte = 0; byte < 6; ++byte) {
        const int high = computerHex(input[source++]);
        const int low = computerHex(input[source++]);
        if (high < 0 || low < 0) return false;
        output[byte * 3] = "0123456789ABCDEF"[high];
        output[byte * 3 + 1] = "0123456789ABCDEF"[low];
        if (byte != 5) output[byte * 3 + 2] = ':';
        if (separator && byte != 5) ++source;
    }
    output[17] = '\0';

    bool nonzero = false;
    for (size_t i = 0; i < 17; ++i)
        if (output[i] != ':' && output[i] != '0') nonzero = true;
    return nonzero && computerHex(output[1]) % 2 == 0;
}

inline bool validComputerId(const char* id) {
    if (!id) return false;
    const size_t length = std::strlen(id);
    if (length == 0 || length > MaxComputerIdLength) return false;
    for (size_t i = 0; i < length; ++i) {
        const char value = id[i];
        const bool alpha = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
        const bool digit = value >= '0' && value <= '9';
        if (!alpha && !digit && value != '-' && value != '_') return false;
    }
    return true;
}

inline bool computerIdentitiesValid(const ComputerIdentity* computers, size_t count) {
    if (!computers || count == 0 || count > MaxComputers) return false;
    for (size_t i = 0; i < count; ++i) {
        char currentMac[18];
        if (!validComputerId(computers[i].id) ||
            !normalizeComputerMac(computers[i].mac, currentMac)) return false;

        for (size_t j = 0; j < i; ++j) {
            char previousMac[18];
            if (std::strcmp(computers[i].id, computers[j].id) == 0) return false;
            if (!normalizeComputerMac(computers[j].mac, previousMac)) return false;
            if (std::strcmp(currentMac, previousMac) == 0) return false;
            if (computers[i].agentToken && *computers[i].agentToken &&
                computers[j].agentToken && *computers[j].agentToken &&
                tokenEqual(computers[i].agentToken, computers[j].agentToken)) return false;
        }
    }
    return true;
}

inline int computerById(const ComputerIdentity* computers, size_t count, const char* id) {
    if (!computers || !id || !*id) return ComputerNotFound;
    int match = ComputerNotFound;
    for (size_t i = 0; i < count; ++i) {
        if (computers[i].id && std::strcmp(computers[i].id, id) == 0) {
            if (match != ComputerNotFound) return ComputerIdentityAmbiguous;
            match = static_cast<int>(i);
        }
    }
    return match;
}

inline int resolveAdminComputer(const ComputerIdentity* computers, size_t count, const char* requestedId) {
    if (requestedId && *requestedId) return computerById(computers, count, requestedId);
    return count == 1 ? 0 : ComputerIdRequired;
}

inline int computerByMac(const ComputerIdentity* computers, size_t count, const char* mac) {
    char requested[18];
    if (!computers || !normalizeComputerMac(mac, requested)) return ComputerNotFound;
    int match = ComputerNotFound;
    for (size_t i = 0; i < count; ++i) {
        char configured[18];
        if (normalizeComputerMac(computers[i].mac, configured) &&
            std::strcmp(configured, requested) == 0) {
            if (match != ComputerNotFound) return ComputerIdentityAmbiguous;
            match = static_cast<int>(i);
        }
    }
    return match;
}

inline int computerByAgentToken(const ComputerIdentity* computers, size_t count, const char* token) {
    if (!computers || !token || !*token) return ComputerNotFound;
    int match = ComputerNotFound;
    for (size_t i = 0; i < count; ++i) {
        if (tokenEqual(token, computers[i].agentToken)) {
            if (match != ComputerNotFound) return ComputerIdentityAmbiguous;
            match = static_cast<int>(i);
        }
    }
    return match;
}

}
