#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include "discovery_identity_store.hpp"
namespace rb {
class DiscoveryRuntime {
    uint8_t identity[DiscoveryIdentitySize]{};
    bool attempted=false,identityReady=false,listening=false,mdnsReady=false;
    IPAddress activeIp;
    WiFiUDP socket;
    DiscoveryBudget budget;
    DiscoveryServiceRetry services;
    String deviceId,publicKey,fingerprint,host;
    const char* state="not_prepared";
    void prepare();
    bool sign(uint8_t* reply);
public:
    bool ready() const{return identityReady;}
    void tick(bool connected);
    void status(JsonObject out) const;
    bool profile(JsonObject out,JsonObjectConst pc) const;
};
}
