#pragma once

#include <cstddef>
#include <cstring>

namespace rb {
inline bool validCredential(const char* value) {
    if (!value) return false;
    const size_t length = std::strlen(value);
    if (length < 8 || length > 128) return false;
    for (size_t index = 0; index < length; ++index) {
        const unsigned char character = static_cast<unsigned char>(value[index]);
        if (character < 32 || character == 127) return false;
    }
    return true;
}

inline bool credentialsDistinct(const char* admin, const char* agent) {
    return admin && agent && std::strcmp(admin, agent) != 0;
}
}
