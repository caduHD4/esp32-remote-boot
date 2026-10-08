#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace rb {

enum class SinricReadiness { Disabled, Ready, MissingCredentials };

inline SinricReadiness sinricReadiness(bool enabled,const char* appKey,const char* appSecret) {
    if(!enabled) return SinricReadiness::Disabled;
    if(!appKey||!appSecret||std::strlen(appKey)<10||std::strlen(appSecret)<10)
        return SinricReadiness::MissingCredentials;
    return SinricReadiness::Ready;
}

inline int sinricHex(char c) {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return c-'a'+10;
    if(c>='A'&&c<='F') return c-'A'+10;
    return -1;
}

inline bool normalizeSinricDeviceId(const char* input,char (&output)[25]) {
    if(!input||std::strlen(input)!=24) return false;
    for(size_t i=0;i<24;++i) {
        int digit=sinricHex(input[i]);
        if(digit<0) return false;
        output[i]="0123456789abcdef"[digit];
    }
    output[24]='\0';
    return true;
}

inline bool sinricDeviceIdEqual(const char* left,const char* right) {
    char a[25],b[25];
    return normalizeSinricDeviceId(left,a)&&normalizeSinricDeviceId(right,b)&&std::strcmp(a,b)==0;
}

inline int sinricSlotForDevice(const char* const* ids,size_t count,const char* deviceId) {
    for(size_t i=0;i<count;++i) if(sinricDeviceIdEqual(ids[i],deviceId)) return static_cast<int>(i);
    return -1;
}

inline bool acceptSinricDispatch(int status,uint32_t now,uint32_t& resetAt) {
    if(status!=202) return false;
    resetAt=now+1000;
    return true;
}

inline bool sinricResetDue(uint32_t due,uint32_t now) {
    return due&&static_cast<int32_t>(now-due)>=0;
}

inline uint32_t nextSinricReset(uint32_t now,bool sent) {
    return sent?0:now+1000;
}

}
