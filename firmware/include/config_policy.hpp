#pragma once

#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>

#include "computer_policy.hpp"

namespace rb {

inline void migratedComputerId(const char* mac, char (&id)[10]) {
    char normalized[18];
    if (!normalizeComputerMac(mac, normalized)) {
        std::strcpy(id, "pc-primary");
        return;
    }
    std::snprintf(id, sizeof id, "pc-%c%c%c%c%c%c",
        normalized[9], normalized[10], normalized[12],
        normalized[13], normalized[15], normalized[16]);
    for (size_t i = 3; id[i]; ++i)
        if (id[i] >= 'A' && id[i] <= 'F') id[i] = static_cast<char>(id[i] - 'A' + 'a');
}

inline bool jsonComputerIdentitiesValid(JsonArrayConst source) {
    if (source.isNull() || source.size() == 0 || source.size() > MaxComputers) return false;
    ComputerIdentity identities[MaxComputers]{};
    size_t count = 0;
    for (JsonObjectConst computer : source) {
        identities[count++] = {
            computer["id"] | "",
            computer["mac"] | "",
            computer["agent_token"] | ""
        };
    }
    return computerIdentitiesValid(identities, count);
}

// Migration is transactional: unsupported or invalid legacy input never replaces the source document.
inline bool migrateConfig(JsonDocument& document) {
    const int originalVersion = document["config_version"] | 0;
    if (originalVersion == 3) return true;
    if (originalVersion != 1 && originalVersion != 2) return false;

    JsonDocument migrated;
    migrated.set(document);
    if (originalVersion == 1 && !migrated["pending_ttl_s"].is<int>())
        migrated["pending_ttl_s"] = 180;

    char computerId[10];
    migratedComputerId(migrated["mac"] | "", computerId);
    JsonArray computers = migrated["computers"].to<JsonArray>();
    JsonObject computer = computers.add<JsonObject>();
    computer["id"] = computerId;

    struct FieldMove { const char* from; const char* to; };
    constexpr FieldMove fields[] = {
        {"pc_name", "name"},
        {"mac", "mac"},
        {"agent_token", "agent_token"},
        {"wol_port", "wol_port"},
        {"wol_repeat", "wol_repeat"},
        {"wol_interval_ms", "wol_interval_ms"},
        {"pending_ttl_s", "pending_ttl_s"},
        {"physical_boot_behavior", "physical_boot_behavior"},
        {"default_target", "default_target"},
        {"fallback_boot_id", "fallback_boot_id"},
        {"systems", "systems"}
    };
    for (const FieldMove& field : fields) {
        computer[field.to].set(migrated[field.from]);
        migrated.remove(field.from);
    }

    JsonArray slots = migrated["sinric_slots"].as<JsonArray>();
    if (!slots.isNull()) {
        for (JsonObject slot : slots) {
            slot["computer_id"] = computerId;
            const char* legacyBootId = slot["boot_id"] | "default";
            if (std::strcmp(legacyBootId, "shutdown") == 0) {
                slot["action"] = "shutdown";
                slot["boot_id"] = "";
            } else if (std::strcmp(legacyBootId, "default") == 0) {
                slot["action"] = "wake";
                slot["boot_id"] = "";
            } else {
                slot["action"] = "boot";
            }
        }
    }

    migrated["config_version"] = 3;
    if (!jsonComputerIdentitiesValid(migrated["computers"].as<JsonArrayConst>())) return false;
    document.set(migrated);
    return true;
}

inline void redactConfig(const JsonDocument& source, JsonObject destination) {
    destination.set(source.as<JsonObjectConst>());

    constexpr const char* globalSecrets[] = {
        "wifi_password", "admin_token", "sinric_app_secret", "sinric_app_key"
    };
    constexpr const char* globalPresence[] = {
        "wifi_password_set", "admin_token_set", "sinric_app_secret_set", "sinric_app_key_set"
    };
    for (size_t i = 0; i < 4; ++i) {
        destination[globalPresence[i]] = std::strlen(source[globalSecrets[i]] | "") > 0;
        destination.remove(globalSecrets[i]);
    }

    // Also redact the schema-2 location while a stored configuration is being migrated.
    destination["agent_token_set"] = std::strlen(source["agent_token"] | "") > 0;
    destination.remove("agent_token");

    JsonArray computers = destination["computers"].as<JsonArray>();
    if (!computers.isNull()) {
        for (JsonObject computer : computers) {
            computer["agent_token_set"] = std::strlen(computer["agent_token"] | "") > 0;
            computer.remove("agent_token");
        }
    }
}

inline void redactConfig(const JsonDocument& source, JsonDocument& destination) {
    redactConfig(source, destination.to<JsonObject>());
}

}
