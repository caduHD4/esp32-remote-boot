#include <cassert>
#include <cstdio>
#include <cstring>

#include "../firmware/include/config_policy.hpp"

static JsonDocument legacyConfig() {
    JsonDocument document;
    document["config_version"] = 2;
    document["ssid"] = "synthetic";
    document["wifi_password"] = "wifi-secret";
    document["admin_token"] = "admin-secret-value";
    document["pc_name"] = "Desktop";
    document["mac"] = "02:00:00:AA:B1:C2";
    document["agent_token"] = "agent-secret-value";
    document["wol_port"] = 7;
    document["wol_repeat"] = 4;
    document["wol_interval_ms"] = 250;
    document["pending_ttl_s"] = 300;
    document["physical_boot_behavior"] = "last_selected";
    document["default_target"] = "0001";
    document["fallback_boot_id"] = "0002";
    document["systems"].add<JsonObject>()["id"] = "0001";
    JsonObject wake = document["sinric_slots"].add<JsonObject>();
    wake["device_id"] = "0123456789abcdef01234567";
    wake["boot_id"] = "default";
    JsonObject shutdown = document["sinric_slots"].add<JsonObject>();
    shutdown["device_id"] = "aaaaaaaaaaaaaaaaaaaaaaaa";
    shutdown["boot_id"] = "shutdown";
    return document;
}

int main() {
    JsonDocument document = legacyConfig();
    assert(rb::migrateConfig(document));
    assert(document["config_version"] == 3);
    assert(document["ssid"] == "synthetic");
    assert(document["wifi_password"] == "wifi-secret");
    assert(document["admin_token"] == "admin-secret-value");
    assert(document["pc_name"].isNull());
    assert(document["agent_token"].isNull());
    assert(document["systems"].isNull());

    JsonObject computer = document["computers"][0];
    assert(computer["id"] == "pc-aab1c2");
    assert(computer["name"] == "Desktop");
    assert(computer["mac"] == "02:00:00:AA:B1:C2");
    assert(computer["agent_token"] == "agent-secret-value");
    assert(computer["wol_port"] == 7);
    assert(computer["wol_repeat"] == 4);
    assert(computer["wol_interval_ms"] == 250);
    assert(computer["pending_ttl_s"] == 300);
    assert(computer["physical_boot_behavior"] == "last_selected");
    assert(computer["default_target"] == "0001");
    assert(computer["fallback_boot_id"] == "0002");
    assert(computer["systems"].size() == 1);

    assert(document["sinric_slots"][0]["computer_id"] == "pc-aab1c2");
    assert(document["sinric_slots"][0]["action"] == "wake");
    assert(document["sinric_slots"][0]["boot_id"] == "");
    assert(document["sinric_slots"][1]["action"] == "shutdown");
    assert(document["sinric_slots"][1]["boot_id"] == "");

    JsonDocument redacted;
    rb::redactConfig(document, redacted);
    assert(redacted["wifi_password"].isNull());
    assert(redacted["admin_token"].isNull());
    assert(redacted["computers"][0]["agent_token"].isNull());
    assert(redacted["wifi_password_set"] == true);
    assert(redacted["admin_token_set"] == true);
    assert(redacted["computers"][0]["agent_token_set"] == true);
    assert(!computer["agent_token"].isNull());

    JsonDocument versionOne = legacyConfig();
    versionOne["config_version"] = 1;
    versionOne.remove("pending_ttl_s");
    assert(rb::migrateConfig(versionOne));
    assert(versionOne["config_version"] == 3);
    assert(versionOne["computers"][0]["pending_ttl_s"] == 180);

    JsonDocument invalid = legacyConfig();
    invalid["mac"] = "invalid";
    JsonDocument original;
    original.set(invalid);
    assert(!rb::migrateConfig(invalid));
    assert(invalid["config_version"] == original["config_version"]);
    assert(invalid["mac"] == original["mac"]);
    assert(invalid["computers"].isNull());

    JsonDocument future = legacyConfig();
    future["config_version"] = 99;
    assert(!rb::migrateConfig(future));
    assert(future["config_version"] == 99);
    assert(future["agent_token"] == "agent-secret-value");

    JsonDocument current = document;
    assert(rb::migrateConfig(current));
    assert(current["computers"].size() == 1);

    puts("PASS: schema v1/v2 migration, field preservation, Sinric conversion and redaction");
}
