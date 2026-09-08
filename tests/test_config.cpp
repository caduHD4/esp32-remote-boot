#include <cassert>
#include <cstdio>
#include "../firmware/include/config_policy.hpp"
int main() {
    JsonDocument d,out; d["config_version"]=1;d["ssid"]="synthetic";d["admin_token"]="test-secret-value";
    assert(rb::migrateConfig(d));assert(d["config_version"]==2&&d["pending_ttl_s"]==180);assert(d["ssid"]=="synthetic");
    d["pending_ttl_s"]=300;assert(rb::migrateConfig(d));assert(d["pending_ttl_s"]==300);
    for(const char* k:{"wifi_password","agent_token","sinric_app_key","sinric_app_secret"})d[k]="synthetic-secret";
    rb::redactConfig(d,out);
    for(const char* k:{"wifi_password","admin_token","agent_token","sinric_app_key","sinric_app_secret"})assert(out[k].isNull()&&!d[k].isNull());
    assert(out["admin_token_set"]==true&&out["ssid"]=="synthetic");
    d["config_version"]=99;assert(!rb::migrateConfig(d));assert(d["config_version"]==99&&d["admin_token"]=="test-secret-value");
    d.clear();assert(!rb::migrateConfig(d));
    puts("PASS: schema migration, future-schema preservation, credential redaction");
}
