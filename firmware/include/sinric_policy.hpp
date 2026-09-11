#pragma once

#include <cstddef>
#include <cstring>

namespace rb {

enum class SinricReadiness { Disabled, Ready, MissingCredentials };

inline SinricReadiness sinricReadiness(bool enabled,const char* appKey,const char* appSecret) {
    if(!enabled) return SinricReadiness::Disabled;
    if(!appKey||!appSecret||std::strlen(appKey)<10||std::strlen(appSecret)<10)
        return SinricReadiness::MissingCredentials;
    return SinricReadiness::Ready;
}

}
