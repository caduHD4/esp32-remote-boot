#pragma once
#include <ArduinoJson.h>
#include <cstring>
namespace rb {
// Full field validation follows migration; unknown schema is never rewritten.
inline bool migrateConfig(JsonDocument& d) {
    int version=d["config_version"]|0;
    if(version!=1&&version!=2) return false;
    if(version==1) { d["config_version"]=2; if(!d["pending_ttl_s"].is<int>()) d["pending_ttl_s"]=180; }
    return true;
}
inline void redactConfig(const JsonDocument& source,JsonDocument& dest) {
    dest.set(source);
    const char* secret[]={"wifi_password","admin_token","agent_token","sinric_app_secret","sinric_app_key"};
    const char* presence[]={"wifi_password_set","admin_token_set","agent_token_set","sinric_app_secret_set","sinric_app_key_set"};
    for(int i=0;i<5;++i) { dest[presence[i]]=std::strlen(source[secret[i]]|"")>0; dest.remove(secret[i]); }
}
}
