#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WebSocketsServer.h>
#include <SinricPro.h>
#include <SinricProSwitch.h>
#include "sinricpro_interface_compat.hpp"
#include "boot_state.hpp"
#include "power_command.hpp"
#include "config_policy.hpp"
#include "setup_policy.hpp"
#include "sinric_policy.hpp"
#include "microlink_runtime.hpp"
#include "local_wifi.h"
#include "web_asset.h"

constexpr char Version[]="3.0.0-multi-pc-setup";
WebServer server(80); WiFiUDP udp; Preferences nvs;
JsonDocument config; rb::State state;
bool setupMode=false,locked=false,sinricOnline=false,sinricStarted=false,agentRebootEnabled=false,agentShutdownEnabled=false;
String osName,hostName,logs[32],agentSession;
rb::PowerCommand powerCommand;
rb::MicrolinkRuntime microlink;
uint32_t logIndex=0,restartAt=0,lastWol=0,wolAt=0;
uint8_t wolRemaining=0; bool wolSent=false;
String slotIds[8]; uint32_t slotReset[8]{};
uint32_t discoveryGeneration=1;
rb::AuthThrottle authThrottle;
WebSocketsServer agentSocket(81);
int socketAgent=-1;
uint32_t socketOpened[WEBSOCKETS_SERVER_CLIENT_MAX]{};
bool socketWaiting[WEBSOCKETS_SERVER_CLIENT_MAX]{};
String socketSentCommand;
uint32_t socketDiscovery=0;


String idText(int id) { if(id<0) return ""; char b[5]; snprintf(b,sizeof b,"%04X",id); return b; }
int idValue(JsonVariantConst v) { int id=-1; rb::parseId(v.as<const char*>(),id); return id; }
void logEvent(const char* text) { logs[logIndex++%32]=String(millis()/1000)+" "+text; }
void jsonReply(int code,JsonDocument& doc) { String out; serializeJson(doc,out); server.sendHeader("Cache-Control","no-store"); server.send(code,"application/json",out); }
void errorReply(int code,const char* error) { JsonDocument d; d["error"]=error; jsonReply(code,d); }
JsonObject primaryComputer() {
    JsonArray computers=config["computers"].as<JsonArray>();
    return computers.isNull()||computers.size()==0?JsonObject():computers[0].as<JsonObject>();
}
bool auth(bool agent=false) {
    authThrottle.refresh(millis());
    if(!authThrottle.allowed(millis())) { errorReply(429,"AUTH_RATE_LIMIT"); return false; }
    String supplied=server.header("Authorization");
    if(!supplied.startsWith("Bearer ")) { authThrottle.failure(millis()); errorReply(401,"AUTH_REQUIRED"); return false; }
    supplied.remove(0,7);
    const char* admin=config["admin_token"]|"";
    JsonObject computer=primaryComputer();
    const char* agentToken=computer.isNull()?"":computer["agent_token"]|"";
    if(rb::tokenEqual(supplied.c_str(),admin) || (agent && rb::tokenEqual(supplied.c_str(),agentToken))) {
        authThrottle.success();
        return true;
    }
    authThrottle.failure(millis()); errorReply(403,"FORBIDDEN"); return false;
}
bool body(JsonDocument& doc) {
    if(server.arg("plain").length()>12000) { errorReply(413,"BODY_TOO_LARGE"); return false; }
    if(deserializeJson(doc,server.arg("plain")) || !doc.is<JsonObject>()) { errorReply(400,"INVALID_JSON"); return false; }
    return true;
}
String randomToken() { char b[33]; for(int i=0;i<4;++i) snprintf(b+i*8,9,"%08lx",static_cast<unsigned long>(esp_random())); return b; }
void defaults(JsonDocument& d) {
    d["config_version"]=3; d["setup_complete"]=false; d["setup_step"]="password";
    d["dhcp"]=true; d["sinric_enabled"]=false;
    d["tailscale_auth_key"]=""; d["tailscale_device_name"]="remote-boot-esp32";
    d["computers"].to<JsonArray>(); d["sinric_slots"].to<JsonArray>();
}
bool validStoredCredential(const char* value,bool optional=false) {
    if(!value||!*value) return optional;
    size_t length=strlen(value);
    if(length<=rb::NewCredentialMaxLength) return true;
    if(length<24||length>128) return false;
    for(size_t i=0;i<length;++i)
        if(!isalnum(static_cast<unsigned char>(value[i]))&&value[i]!='_'&&value[i]!='-') return false;
    return true;
}
bool parseMac(const char* text,uint8_t* mac) {
    if(!text||strlen(text)!=17) return false;
    auto hex=[](char c)->int { return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1; };
    bool nonzero=false;
    for(int i=0;i<6;++i) { int high=hex(text[i*3]),low=hex(text[i*3+1]); if(high<0||low<0||(i<5&&text[i*3+2]!=':')) return false; mac[i]=high*16+low; nonzero|=mac[i]!=0; }
    return nonzero && !(mac[0]&1);
}
bool validateComputer(JsonObject computer) {
    const char* name=computer["name"]|"";
    if(!*name||strlen(name)>63||!validStoredCredential(computer["agent_token"]|"",true)) return false;
    int port=computer["wol_port"],repeat=computer["wol_repeat"],interval=computer["wol_interval_ms"],ttl=computer["pending_ttl_s"];
    if(port<1||port>65535||repeat<1||repeat>10||interval<20||interval>1000||ttl<30||ttl>3600) return false;
    String behavior=computer["physical_boot_behavior"]|"";
    if(behavior!="default_target"&&behavior!="last_selected"&&behavior!="exit_to_firmware") return false;
    JsonArray systems=computer["systems"].as<JsonArray>();
    if(systems.isNull()||systems.size()>rb::MaxEntries) return false;
    for(size_t i=0;i<systems.size();++i) {
        int id=idValue(systems[i]["id"]); const char* entryName=systems[i]["name"];
        if(id<0||!entryName||!*entryName||strlen(entryName)>63) return false;
        for(size_t j=0;j<i;++j) if(idValue(systems[j]["id"])==id) return false;
        String lower=entryName; lower.toLowerCase();
        if(lower.indexOf("remote boot")>=0||lower.indexOf("ipxe")>=0) systems[i]["blocked"]=true;
    }
    auto exists=[&](int id){ for(JsonObject entry:systems) if(idValue(entry["id"])==id&&!entry["blocked"].as<bool>()) return true; return false; };
    for(const char* key:{"default_target","fallback_boot_id"})
        if(strlen(computer[key]|"")&&!exists(idValue(computer[key]))) return false;
    return true;
}
bool validate(JsonDocument& d) {
    if(d["config_version"].as<int>()!=3||!d["ssid"].is<const char*>()||
       strlen(d["ssid"].as<const char*>())<1||strlen(d["ssid"].as<const char*>())>32) return false;
    for(const char* key:{"wifi_password","sinric_app_key","sinric_app_secret","tailscale_device_name"})
        if(d[key].is<const char*>()&&strlen(d[key])>128) return false;
    rb::SetupStep step=rb::parseSetupStep(d["setup_step"]|"");
    bool complete=d["setup_complete"]|false;
    if(step==rb::SetupStep::Invalid||(complete!=(step==rb::SetupStep::Complete))) return false;
    if(step!=rb::SetupStep::Password&&!validStoredCredential(d["admin_token"]|"")) return false;
    if(!d["dhcp"].as<bool()) { IPAddress address; for(const char* key:{"ip","subnet","gateway","dns"}) if(!address.fromString(d[key]|"")) return false; }

    JsonArray computers=d["computers"].as<JsonArray>();
    const bool allowEmpty=!complete&&(step==rb::SetupStep::Password||step==rb::SetupStep::Computer);
    if(!rb::jsonComputerIdentitiesValid(d["computers"].as<JsonArrayConst>(),allowEmpty)) return false;
    for(JsonObject computer:computers) if(!validateComputer(computer)) return false;
    if((step==rb::SetupStep::Boot||step==rb::SetupStep::Integrations||step==rb::SetupStep::Finish||complete)&&computers.size()==0) return false;

    const char* tailscaleKey=d["tailscale_auth_key"]|"";
    if(*tailscaleKey&&(strlen(tailscaleKey)<20||strlen(tailscaleKey)>256||strncmp(tailscaleKey,"tskey-auth-",11)!=0)) return false;

    JsonArray slots=d["sinric_slots"].as<JsonArray>(); if(slots.isNull()||slots.size()>8) return false;
    for(size_t i=0;i<slots.size();++i) {
        char deviceId[25]; if(!rb::normalizeSinricDeviceId(slots[i]["device_id"]|"",deviceId)) return false;
        slots[i]["device_id"]=deviceId;
        for(size_t j=0;j<i;++j) if(rb::sinricDeviceIdEqual(deviceId,slots[j]["device_id"]|"")) return false;
        JsonObject computer;
        const char* computerId=slots[i]["computer_id"]|"";
        for(JsonObject candidate:computers) if(strcmp(candidate["id"]|"",computerId)==0) computer=candidate;
        if(computer.isNull()) return false;
        String action=slots[i]["action"]|"";
        if(action!="wake"&&action!="boot"&&action!="shutdown"&&action!="reboot") return false;
        String bootId=slots[i]["boot_id"]|"";
        if(action=="boot"&&bootId.length()) {
            bool exists=false;
            for(JsonObject entry:computer["systems"].as<JsonArray>())
                if(idValue(entry["id"])==idValue(slots[i]["boot_id"])&&!entry["blocked"].as<bool>()) exists=true;
            if(!exists) return false;
        }
    }
    if(rb::sinricReadiness(d["sinric_enabled"].as<bool>(),d["sinric_app_key"]|"",d["sinric_app_secret"]|"")==rb::SinricReadiness::MissingCredentials) return false;
    return true;
}
void reloadState() {
    state=rb::State{};
    JsonObject computer=primaryComputer();
    if(computer.isNull()) return;
    for(JsonObject entry:computer["systems"].as<JsonArray>()) {
        rb::Entry next{};
        next.id=idValue(entry["id"]); strlcpy(next.name,entry["name"]|"",sizeof next.name);
        next.hidden=entry["hidden"]|false; next.blocked=entry["blocked"]|false;
        state.append(next);
    }
    state.defaultTarget=idValue(computer["default_target"]); state.fallback=idValue(computer["fallback_boot_id"]);
    state.ttl=computer["pending_ttl_s"].as<uint32_t>()*1000;
    String behavior=computer["physical_boot_behavior"]|"default_target";
    state.behavior=behavior=="last_selected"?rb::State::Last:behavior=="exit_to_firmware"?rb::State::Exit:rb::State::Default;
    state.reconcile();
}
bool persist(JsonDocument& d) { String serialized; serializeJson(d,serialized); return nvs.putString("config",serialized)==serialized.length(); }
int requestWake() {
    if(locked||setupMode||WiFi.status()!=WL_CONNECTED) return 503;
    if(wolRemaining||(wolSent&&uint32_t(millis()-lastWol)<3000)) return 429;
    wolRemaining=primaryComputer()["wol_repeat"]; wolAt=millis(); logEvent("WAKE_QUEUED"); return 202;
}
int requestBoot(int target,bool force) {
    if(locked||setupMode||WiFi.status()!=WL_CONNECTED) return 503;
    if(wolRemaining||(wolSent&&uint32_t(millis()-lastWol)<3000)) return 429;
    int status=state.request(target,millis(),force);
    if(status!=202) return status;
    nvs.putInt("last",state.lastSelected);
    wolRemaining=primaryComputer()["wol_repeat"]; wolAt=millis(); logEvent("BOOT_QUEUED"); return 202;
}
int requestShutdown() {
    if(locked||setupMode) return 503;
    if(!state.online(millis())) return 409;
    if(!agentShutdownEnabled || agentSession.length()<16) return 403;
    if(!powerCommand.enqueue(randomToken().c_str(),"shutdown","",agentSession.c_str(),millis())) return 409;
    state.pending=rb::None; wolRemaining=0; logEvent("SHUTDOWN_QUEUED"); return 202;
}
void wolTick() {
    if(!wolRemaining||static_cast<int32_t>(millis()-wolAt)<0) return;
    uint8_t mac[6],packet[102]; if(!parseMac(primaryComputer()["mac"],mac)) { wolRemaining=0; return; }
    memset(packet,255,6); for(int i=0;i<16;++i) memcpy(packet+6+i*6,mac,6);
    IPAddress local=WiFi.localIP(),mask=WiFi.subnetMask(),broadcast;
    for(int i=0;i<4;++i) broadcast[i]=local[i]|~mask[i];
    if(udp.beginPacket(broadcast,primaryComputer()["wol_port"].as<uint16_t>())) { udp.write(packet,sizeof packet); logEvent(udp.endPacket()?"WOL_SENT":"WOL_FAILED"); }
    else logEvent("WOL_FAILED");
    --wolRemaining; lastWol=millis(); wolSent=true; wolAt=millis()+primaryComputer()["wol_interval_ms"].as<uint32_t>();
}
bool connectWiFi(const char* ssid,const char* password) {
    WiFi.begin(ssid,password);
    uint32_t start=millis(); while(WiFi.status()!=WL_CONNECTED&&millis()-start<20000) delay(50);
    return WiFi.status()==WL_CONNECTED;
}
void appendStatus(JsonObject d) {
    d["version"]=Version; d["setup_mode"]=setupMode; d["setup_complete"]=config["setup_complete"]|false; d["setup_step"]=config["setup_step"]|""; d["config_locked"]=locked; d["online"]=state.online(millis());
    d["os"]=osName; d["hostname"]=hostName; d["ip"]=WiFi.localIP().toString(); d["rssi"]=WiFi.RSSI();
    d["sinric_online"]=sinricOnline; d["uptime"]=millis()/1000; d["heap"]=ESP.getFreeHeap();
    d["agent_transport"]=socketAgent>=0?"websocket":"http-legacy";
    d["reboot_enabled"]=agentRebootEnabled && state.online(millis());
    d["shutdown_enabled"]=agentShutdownEnabled && agentSession.length()>=16 && state.online(millis());
    d["pending_target"]=idText(state.pendingValid(millis())?state.pending:-1); d["default_target"]=idText(state.defaultTarget);
    d["last_selected_target"]=idText(state.lastSelected); d["pending_created_at_ms"]=state.created; d["pending_ttl_s"]=state.ttl/1000;
    const rb::MicrolinkSnapshot tail=microlink.snapshot(); JsonObject tailscale=d["tailscale"].to<JsonObject>();
    tailscale["built"]=tail.built; tailscale["configured"]=tail.configured; tailscale["connected"]=tail.connected;
    tailscale["state"]=tail.state; tailscale["ip"]=tail.ip; tailscale["peers"]=tail.peers;
    tailscale["control_online"]=tail.controlOnline; tailscale["derp_online"]=tail.derpOnline; tailscale["derp_server_info"]=tail.serverInfo;
    tailscale["map_updates"]=tail.mapUpdates; tailscale["reconnects"]=tail.reconnects; tailscale["tls_deferred"]=tail.tlsDeferred;
    tailscale["wg_encrypted_rx"]=tail.encryptedRx; tailscale["wg_authenticated_rx"]=tail.authenticatedRx; tailscale["authenticated_age_ms"]=tail.authenticatedAgeMs;
    tailscale["heap_free"]=tail.heapFree; tailscale["heap_minimum"]=tail.heapMinimum; tailscale["largest_block"]=tail.largestBlock;
}
void routes() {
    const char* headers[]={"Authorization","If-None-Match"}; server.collectHeaders(headers,2);
    server.on("/api/v1/setup/state",HTTP_GET,[]{
        JsonDocument response;
        response["setup_complete"]=config["setup_complete"]|false;
        response["step"]=config["setup_step"]|"password";
        response["password_set"]=strlen(config["admin_token"]|"")>0;
        response["computer_count"]=config["computers"].size();
        response["tailscale_available"]=microlink.snapshot().built;
        jsonReply(200,response);
    });
    server.on("/api/v1/setup/password",HTTP_POST,[]{
        if((config["setup_complete"]|false)||rb::parseSetupStep(config["setup_step"]|"")!=rb::SetupStep::Password||
           strlen(config["admin_token"]|"")) { errorReply(409,"SETUP_STAGE_MISMATCH"); return; }
        JsonDocument input,next; if(!body(input)) return;
        const char* password=input["password"]|"";
        if(!rb::validNewCredential(password)) { errorReply(400,"PASSWORD_LENGTH"); return; }
        next.set(config); next["admin_token"]=password; next["setup_step"]="computer";
        if(!validate(next)) { errorReply(400,"INVALID_CONFIG"); return; }
        if(!persist(next)) { errorReply(500,"NVS_WRITE_FAILED"); return; }
        config.set(next); JsonDocument response; response["saved"]=true; response["restarting"]=true;
        jsonReply(200,response); restartAt=millis()+1200;
    });
    server.on("/api/v1/setup/computer",HTTP_POST,[]{
        if(!auth()) return;
        if((config["setup_complete"]|false)||rb::parseSetupStep(config["setup_step"]|"")!=rb::SetupStep::Computer||
           config["computers"].size()!=0) { errorReply(409,"SETUP_STAGE_MISMATCH"); return; }
        JsonDocument input,next; if(!body(input)) return;
        const char* name=input["name"]|"";
        const char* mac=input["mac"]|"";
        const char* agentPassword=input["agent_password"]|"";
        if(!*name||strlen(name)>63||!rb::validNewCredential(agentPassword,true)) { errorReply(400,"INVALID_COMPUTER"); return; }
        char normalizedMac[18]; if(!rb::normalizeComputerMac(mac,normalizedMac)) { errorReply(400,"INVALID_MAC"); return; }
        char computerId[10]; rb::migratedComputerId(normalizedMac,computerId);
        next.set(config); JsonObject computer=next["computers"].add<JsonObject>();
        computer["id"]=computerId; computer["name"]=name; computer["mac"]=normalizedMac;
        computer["agent_token"]=agentPassword; computer["wol_port"]=input["wol_port"]|9;
        computer["wol_repeat"]=input["wol_repeat"]|3; computer["wol_interval_ms"]=input["wol_interval_ms"]|100;
        computer["pending_ttl_s"]=180; computer["physical_boot_behavior"]="exit_to_firmware";
        computer["default_target"]=""; computer["fallback_boot_id"]=""; computer["systems"].to<JsonArray>();
        next["setup_step"]=strlen(agentPassword)?"boot":"integrations";
        if(!validate(next)) { errorReply(400,"INVALID_CONFIG"); return; }
        if(!persist(next)) { errorReply(500,"NVS_WRITE_FAILED"); return; }
        config.set(next); reloadState(); JsonDocument response;
        response["saved"]=true; response["computer_id"]=computerId; response["next_step"]=next["setup_step"];
        jsonReply(200,response);
    });
    server.on("/api/v1/setup/boot",HTTP_POST,[]{
        if(!auth()) return;
        if((config["setup_complete"]|false)||rb::parseSetupStep(config["setup_step"]|"")!=rb::SetupStep::Boot) {
            errorReply(409,"SETUP_STAGE_MISMATCH"); return;
        }
        JsonDocument input,next; if(!body(input)) return; next.set(config);
        JsonObject computer=next["computers"][0];
        if(strlen(computer["agent_token"]|"")==0) { errorReply(409,"AGENT_NOT_CONFIGURED"); return; }
        for(const char* key:{"default_target","fallback_boot_id","physical_boot_behavior"})
            if(!input[key].isNull()) computer[key]=input[key];
        if(!input["pending_ttl_s"].isNull()) computer["pending_ttl_s"]=input["pending_ttl_s"];
        next["setup_step"]="integrations";
        if(!validate(next)) { errorReply(400,"INVALID_BOOT_CONFIG"); return; }
        if(!persist(next)) { errorReply(500,"NVS_WRITE_FAILED"); return; }
        config.set(next); reloadState(); JsonDocument response; response["saved"]=true; response["next_step"]="integrations";
        jsonReply(200,response);
    });
    server.on("/api/v1/setup/integrations",HTTP_POST,[]{
        if(!auth()) return;
        if((config["setup_complete"]|false)||rb::parseSetupStep(config["setup_step"]|"")!=rb::SetupStep::Integrations) {
            errorReply(409,"SETUP_STAGE_MISMATCH"); return;
        }
        JsonDocument input,next; if(!body(input)) return; next.set(config);
        next["sinric_enabled"]=input["sinric_enabled"]|false;
        if(!input["sinric_app_key"].isNull()) next["sinric_app_key"]=input["sinric_app_key"];
        if(!input["sinric_app_secret"].isNull()) next["sinric_app_secret"]=input["sinric_app_secret"];
        if(!input["sinric_slots"].isNull()) next["sinric_slots"]=input["sinric_slots"];
        const char* tailscaleKey=input["tailscale_auth_key"]|"";
        if(*tailscaleKey&&!microlink.snapshot().built) { errorReply(400,"TAILSCALE_UNAVAILABLE"); return; }
        next["tailscale_auth_key"]=tailscaleKey;
        if(!input["tailscale_device_name"].isNull()) next["tailscale_device_name"]=input["tailscale_device_name"];
        next["setup_step"]="finish";
        if(!validate(next)) { errorReply(400,"INVALID_INTEGRATIONS"); return; }
        if(!persist(next)) { errorReply(500,"NVS_WRITE_FAILED"); return; }
        config.set(next); JsonDocument response; response["saved"]=true; response["next_step"]="finish";
        jsonReply(200,response);
    });
    server.on("/api/v1/setup/finish",HTTP_POST,[]{
        if(!auth()) return;
        if((config["setup_complete"]|false)||rb::parseSetupStep(config["setup_step"]|"")!=rb::SetupStep::Finish) {
            errorReply(409,"SETUP_STAGE_MISMATCH"); return;
        }
        JsonDocument next; next.set(config); next["setup_complete"]=true; next["setup_step"]="complete";
        if(!validate(next)) { errorReply(400,"INVALID_CONFIG"); return; }
        if(!persist(next)) { errorReply(500,"NVS_WRITE_FAILED"); return; }
        config.set(next); setupMode=false; JsonDocument response; response["saved"]=true; response["restarting"]=true;
        jsonReply(200,response); restartAt=millis()+1200;
    });
    server.on("/",HTTP_GET,[]{ String etag=String('"')+webAssetEtag+'"'; server.sendHeader("ETag",etag); server.sendHeader("Cache-Control","public, max-age=0, must-revalidate"); if(server.header("If-None-Match")==etag) { server.send(304); return; } server.sendHeader("Content-Encoding","gzip"); server.send_P(200,"text/html",reinterpret_cast<const char*>(webAsset),sizeof webAsset); });
    server.on("/boot.ipxe",HTTP_GET,[]{
        int target=locked?-1:state.dispatch(millis()); String script="#!ipxe\n";
        if(target>=0) { script+="imgexec RemoteBoot.efi boot="+idText(target); if(state.valid(state.fallback)&&state.fallback!=target) script+=" fallback="+idText(state.fallback); script+=" || goto failed\nexit\n:failed\necho RemoteBoot failed\nexit 1\n"; }
        else script+="exit\n";
        server.sendHeader("Cache-Control","no-store"); server.send(200,"text/plain",script);
    });
    server.on("/api/v1/status",HTTP_GET,[]{ if(!auth()) return; JsonDocument d; appendStatus(d.to<JsonObject>()); jsonReply(200,d); });
    server.on("/api/v1/config",HTTP_GET,[]{ if(!auth()) return; JsonDocument d; rb::redactConfig(config,d);
        jsonReply(200,d);
    });
    server.on("/api/v1/bootstrap",HTTP_GET,[]{ if(!auth()) return; JsonDocument d; rb::redactConfig(config,d["config"].to<JsonObject>()); appendStatus(d["status"].to<JsonObject>()); jsonReply(200,d); });
    server.on("/api/v1/config",HTTP_PUT,[]{ if(!auth()) return; if(locked) { errorReply(409,"SCHEMA_LOCKED"); return; }
        JsonDocument patch,next; if(!body(patch)) return; next.set(config);
        const char* allowed="|ssid|wifi_password|admin_token|dhcp|ip|subnet|gateway|dns|sinric_enabled|sinric_app_key|sinric_app_secret|sinric_slots|computers|tailscale_auth_key|tailscale_device_name|";
        for(JsonPair p:patch.as<JsonObject>()) { if(!strstr(allowed,(String("|")+p.key().c_str()+"|").c_str())) { errorReply(400,"UNKNOWN_FIELD"); return; } next[p.key()]=p.value(); }
        if(rb::sinricReadiness(next["sinric_enabled"].as<bool>(),next["sinric_app_key"]|"",next["sinric_app_secret"]|"")==rb::SinricReadiness::MissingCredentials) { errorReply(400,"SINRIC_CREDENTIALS_REQUIRED"); return; }
        if(!validate(next)) { errorReply(400,"INVALID_CONFIG"); return; }
        if(!persist(next)) { errorReply(500,"NVS_WRITE_FAILED"); return; }
        config.set(next); reloadState(); JsonDocument d; d["saved"]=true; d["restarting"]=true; jsonReply(200,d); restartAt=millis()+1500;
    });
    server.on("/api/v1/systems",HTTP_GET,[]{ if(!auth(true)) return; JsonDocument d; d["systems"]=primaryComputer()["systems"]; jsonReply(200,d); });
    server.on("/api/v1/systems/sync",HTTP_POST,[]{ if(!auth(true)) return; if(locked) { errorReply(409,"SCHEMA_LOCKED"); return; }
        JsonDocument input,next; if(!body(input)) return; next.set(config); next["computers"][0]["systems"]=input["systems"];
        if(!next["computers"][0]["systems"].is<JsonArray>()||next["computers"][0]["systems"].size()>rb::MaxEntries) { errorReply(400,"CATALOG_LIMIT"); return; }
        // Preserve user visibility and display order on subsequent scans.
        JsonDocument merged; auto list=merged.to<JsonArray>();
        for(JsonObject old:config["computers"][0]["systems"].as<JsonArray>()) for(JsonObject fresh:next["computers"][0]["systems"].as<JsonArray>()) if(idValue(old["id"])==idValue(fresh["id"])) { fresh["hidden"]=old["hidden"]; list.add(fresh); }
        for(JsonObject fresh:next["computers"][0]["systems"].as<JsonArray>()) { bool found=false; for(JsonObject e:list) if(idValue(e["id"])==idValue(fresh["id"])) found=true; if(!found) list.add(fresh); }
        if(list.size()!=next["computers"][0]["systems"].size()) { errorReply(400,"DUPLICATE_ID"); return; }
        next["computers"][0]["systems"]=list;
        auto exists=[&](int id){ for(JsonObject e:list) if(idValue(e["id"])==id&&!e["blocked"].as<bool>()) return true; return false; };
        for(const char* k:{"default_target","fallback_boot_id"}) if(!exists(idValue(next["computers"][0][k]))) next["computers"][0][k]="";
        JsonArray slots=next["sinric_slots"].as<JsonArray>();
        const char* computerId=next["computers"][0]["id"]|"";
        for(int i=int(slots.size())-1;i>=0;--i)
            if(String(slots[i]["computer_id"]|"")==computerId&&String(slots[i]["action"]|"")=="boot"&&
               strlen(slots[i]["boot_id"]|"")&&!exists(idValue(slots[i]["boot_id"]))) slots.remove(i);
        if(!validate(next)) { errorReply(400,"INVALID_CATALOG"); return; }
        String before,after; serializeJson(config,before); serializeJson(next,after);
        if(before!=after&&!persist(next)) { errorReply(500,"NVS_WRITE_FAILED"); return; }
        config.set(next); reloadState(); JsonDocument d; d["synced"]=state.count; jsonReply(200,d);
    });
    server.on("/api/v1/boot",HTTP_POST,[]{ if(!auth()) return; JsonDocument d; if(!body(d)) return;
        bool force=d["force"]|false; if(force&&String(d["confirm"]|"")!="FORCE_BOOT") { errorReply(400,"CONFIRM_REQUIRED"); return; }
        int code=requestBoot(idValue(d["boot_id"]),force); if(code!=202) { errorReply(code,code==409?"PC_ALREADY_ON":code==429?"RATE_LIMIT":"BOOT_REJECTED"); return; }
        d.clear(); d["queued"]=true; jsonReply(202,d);
    });
    server.on("/api/v1/discovery/request",HTTP_POST,[]{ if(!auth()) return; ++discoveryGeneration; JsonDocument d; d["generation"]=discoveryGeneration; jsonReply(202,d); });
    server.on("/api/v1/shutdown",HTTP_POST,[]{ if(!auth()) return; JsonDocument d; if(!body(d)) return;
        if(String(d["confirm"]|"")!="SHUTDOWN") { errorReply(400,"CONFIRM_REQUIRED"); return; }
        int code=requestShutdown(); if(code!=202) { errorReply(code,code==403?"AGENT_SHUTDOWN_DISABLED":code==409?"AGENT_OFFLINE_OR_COMMAND_PENDING":"SHUTDOWN_UNAVAILABLE"); return; }
        d.clear(); d["queued"]=true; jsonReply(202,d);
    });
    server.on("/api/v1/reboot",HTTP_POST,[]{ if(!auth()) return; JsonDocument d; if(!body(d)) return;
        int id=idValue(d["boot_id"]); if(!state.online(millis())) { errorReply(409,"AGENT_OFFLINE"); return; }
        if(!agentRebootEnabled) { errorReply(409,"AGENT_REBOOT_DISABLED"); return; }
        if(!state.valid(id)||String(d["confirm"]|"")!="REBOOT") { errorReply(400,"CONFIRM_AND_VALID_TARGET_REQUIRED"); return; }
        if(!powerCommand.enqueue(randomToken().c_str(),"reboot",idText(id).c_str(),agentSession.c_str(),millis())) { errorReply(409,"COMMAND_PENDING"); return; }
        d.clear(); d["queued"]=true; jsonReply(202,d);
    });
    server.on("/api/v1/heartbeat",HTTP_POST,[]{ if(!auth(true)) return;
        if(socketAgent>=0) { errorReply(409,"WEBSOCKET_AGENT_ACTIVE"); return; }
        state.heartbeatExpiry=45000; JsonDocument d; if(!body(d)) return;
        hostName=String(d["hostname"]|"").substring(0,63); osName=String(d["os"]|"").substring(0,63);
        state.heartbeat(millis(),idValue(d["boot_id"]));
        agentRebootEnabled=d["reboot_enabled"]|false;
        agentShutdownEnabled=d["shutdown_enabled"]|false;
        agentSession=String(d["session_id"]|"").substring(0,64);
        bool accepted=powerCommand.heartbeat(agentSession.c_str(),d["ack"]|"",agentShutdownEnabled,millis());
        if(powerCommand.action=="reboot"&&!agentRebootEnabled) powerCommand.clear();
        d.clear(); d["discovery_generation"]=discoveryGeneration; d["ack_accepted"]=accepted;
        if(powerCommand.active(millis())) { d["command"]["id"]=powerCommand.id.c_str(); d["command"]["boot_id"]=powerCommand.target.c_str(); d["command"]["action"]=powerCommand.action.c_str(); d["command"]["session_id"]=powerCommand.session.c_str(); }
        jsonReply(200,d);
    });
    server.on("/api/v1/logs",HTTP_GET,[]{ if(!auth()) return; JsonDocument d; auto arr=d["logs"].to<JsonArray>(); uint32_t n=logIndex<32?logIndex:32; for(uint32_t i=0;i<n;++i) arr.add(logs[(logIndex-n+i)%32]); jsonReply(200,d); });
    server.on("/api/v1/system/reboot",HTTP_POST,[]{ if(!auth()) return; JsonDocument d; d["restarting"]=true; jsonReply(202,d); restartAt=millis()+1000; });
    server.on("/api/v1/system/reset",HTTP_POST,[]{ if(!auth()) return; JsonDocument d; if(!body(d)) return; if(String(d["confirm"]|"")!="FACTORY_RESET") { errorReply(400,"CONFIRM_REQUIRED"); return; } if(!nvs.clear()) { errorReply(500,"NVS_WRITE_FAILED"); return; } d.clear(); d["reset"]=true; jsonReply(200,d); restartAt=millis()+1000; });
    server.onNotFound([]{ errorReply(404,"NOT_FOUND"); }); server.begin();
}
void socketReply(uint8_t num,JsonDocument& d) {
    String data; serializeJson(d,data); agentSocket.sendTXT(num,data);
}
void startAgentSocket() {
    agentSocket.begin(); agentSocket.enableHeartbeat(60000,15000,2);
    agentSocket.onEvent([](uint8_t num,WStype_t type,uint8_t* payload,size_t length) {
        if(num>=WEBSOCKETS_SERVER_CLIENT_MAX) return;
        if(type==WStype_CONNECTED) {
            socketWaiting[num]=true; socketOpened[num]=millis();
            if(length!=6||memcmp(payload,"/agent",6)!=0) agentSocket.disconnect(num);
            return;
        }
        if(type==WStype_DISCONNECTED) {
            socketWaiting[num]=false;
            if(socketAgent==num) {
                socketAgent=-1; powerCommand.clear(); socketSentCommand=""; agentSession="";
                agentShutdownEnabled=false; agentRebootEnabled=false; state.heartbeatSeen=false;
                logEvent("AGENT_DISCONNECTED");
            }
            return;
        }
        if(type==WStype_PONG) {
            if(socketAgent==num)state.heartbeatAt=millis();
            return;
        }
        if(type==WStype_PING) return;
        if(type!=WStype_TEXT||length>12000) { agentSocket.disconnect(num); return; }
        JsonDocument d;
        if(deserializeJson(d,payload,length)||!d.is<JsonObject>()) { agentSocket.disconnect(num); return; }
        String kind=d["type"]|"";
        if(socketAgent!=num) {
            String session=d["session_id"]|"";
            bool validSession=session.length()==32;
            for(char ch:session) if(!isxdigit(ch))validSession=false;
            if(socketAgent>=0||kind!="hello"||locked||!validSession||
               !rb::tokenEqual(d["token"]|"",primaryComputer()["agent_token"]|"")) { agentSocket.disconnect(num); return; }
            socketAgent=num;socketWaiting[num]=false;socketSentCommand="";powerCommand.clear();
            agentSession=session;agentShutdownEnabled=d["shutdown_enabled"]|false;agentRebootEnabled=d["reboot_enabled"]|false;
            hostName=String(d["hostname"]|"").substring(0,63);osName=String(d["os"]|"").substring(0,63);
            state.heartbeatExpiry=90000;state.heartbeat(millis(),idValue(d["boot_id"]));
            socketDiscovery=discoveryGeneration;
            d.clear();d["type"]="ready";d["session_id"]=agentSession;socketReply(num,d);logEvent("AGENT_CONNECTED");
        } else if(kind=="ack") {
            String id=d["id"]|""; bool accepted=false;
            if(String(d["session_id"]|"")==agentSession) {
                if(powerCommand.action=="reboot"&&!agentRebootEnabled)powerCommand.clear();
                accepted=powerCommand.heartbeat(agentSession.c_str(),id.c_str(),agentShutdownEnabled,millis());
            }
            d.clear();d["type"]="ack";d["id"]=id;d["session_id"]=agentSession;d["accepted"]=accepted;socketReply(num,d);
            logEvent(accepted?"POWER_ACK_ACCEPTED":"POWER_ACK_REJECTED");
        } else if(kind=="result")logEvent(d["requested"].as<bool>()?"OS_POWER_REQUESTED":"OS_POWER_REFUSED");
        else agentSocket.disconnect(num);
    });
}
void agentSocketTick() {
    agentSocket.loop();
    for(uint8_t num=0;num<WEBSOCKETS_SERVER_CLIENT_MAX;++num)
        if(socketWaiting[num]&&uint32_t(millis()-socketOpened[num])>=5000)agentSocket.disconnect(num);
    if(socketAgent<0) return;
    if(!state.online(millis())) { agentSocket.disconnect(socketAgent);return; }
    if(powerCommand.active(millis())&&socketSentCommand!=powerCommand.id.c_str()) {
        JsonDocument d;d["type"]="command";d["id"]=powerCommand.id.c_str();d["action"]=powerCommand.action.c_str();
        d["boot_id"]=powerCommand.target.c_str();d["session_id"]=agentSession;
        socketReply(socketAgent,d);socketSentCommand=powerCommand.id.c_str();
    }
    if(socketDiscovery!=discoveryGeneration) {
        JsonDocument d;d["type"]="discover";socketReply(socketAgent,d);socketDiscovery=discoveryGeneration;
    }
}
void startSinric() {
    if(setupMode||locked) return;
    rb::SinricReadiness readiness=rb::sinricReadiness(config["sinric_enabled"].as<bool>(),config["sinric_app_key"]|"",config["sinric_app_secret"]|"");
    if(readiness==rb::SinricReadiness::Disabled) return;
    if(readiness==rb::SinricReadiness::MissingCredentials) { logEvent("SINRIC_CONFIG_INCOMPLETE"); return; }
    size_t i=0;
    for(JsonObject slot:config["sinric_slots"].as<JsonArray>()) {
        slotIds[i]=slot["device_id"].as<String>(); size_t index=i++;
        SinricProSwitch& device=SinricPro[slotIds[index]];
        device.onPowerState([](const String& deviceId,bool& on){
            if(!on) return true;
            logEvent("SINRIC_COMMAND_RECEIVED");
            const char* currentIds[8]{}; size_t currentCount=0;
            for(JsonObject current:config["sinric_slots"].as<JsonArray>()) currentIds[currentCount++]=current["device_id"]|"";
            int currentIndex=rb::sinricSlotForDevice(currentIds,currentCount,deviceId.c_str());
            if(currentIndex<0) { logEvent("SINRIC_DEVICE_NOT_CONFIGURED"); return false; }
            JsonObject slot=config["sinric_slots"][currentIndex];
            String action=slot["action"]|"wake";
            int code=503;
            if(action=="shutdown") { logEvent("SINRIC_SHUTDOWN_REQUESTED"); code=requestShutdown(); }
            else if(action=="wake") { logEvent("SINRIC_WAKE_REQUESTED"); code=requestWake(); }
            else if(action=="boot") {
                logEvent("SINRIC_BOOT_REQUESTED");
                int id=strlen(slot["boot_id"]|"")?idValue(slot["boot_id"]):state.defaultTarget;
                code=requestBoot(id,false);
            } else if(action=="reboot") {
                int id=strlen(slot["boot_id"]|"")?idValue(slot["boot_id"]):state.defaultTarget;
                if(!state.online(millis())) code=409;
                else if(!agentRebootEnabled) code=403;
                else if(!state.valid(id)) code=400;
                else code=powerCommand.enqueue(randomToken().c_str(),"reboot",idText(id).c_str(),agentSession.c_str(),millis())?202:409;
            }
            const char* registeredIds[8]{}; for(int j=0;j<8;++j) registeredIds[j]=slotIds[j].c_str();
            int registered=rb::sinricSlotForDevice(registeredIds,8,deviceId.c_str());
            if(registered<0) { logEvent("SINRIC_DEVICE_NOT_REGISTERED"); return false; }
            if(!rb::acceptSinricDispatch(code,millis(),slotReset[registered])) { logEvent(code==403?"SINRIC_REJECTED_DISABLED":code==409?"SINRIC_REJECTED_STATE":code==429?"SINRIC_REJECTED_RATE":"SINRIC_REJECTED_UNAVAILABLE"); return false; }
            return true;
        });
    }
    SinricPro.onConnected([]{sinricOnline=true;}); SinricPro.onDisconnected([]{sinricOnline=false;});
    SinricPro.begin(config["sinric_app_key"].as<const char*>(),config["sinric_app_secret"].as<const char*>()); sinricStarted=true;
}
void setup() {
    Serial.begin(115200); delay(300);
    if(!nvs.begin("remote-boot-v2",false)) locked=true;
    String stored=nvs.getString("config",""); defaults(config);
    if(stored.length()) {
        JsonDocument loaded;
        if(deserializeJson(loaded,stored)) locked=true;
        else {
            int schema=loaded["config_version"]|0;
            if(!rb::migrateConfig(loaded)||!validate(loaded)) locked=true;
            else { config.set(loaded); if(schema!=3&&!persist(config)) locked=true; }
        }
    }
    setupMode=!(config["setup_complete"]|false);
    state.lastSelected=nvs.getInt("last",-1); reloadState();
    WiFi.persistent(false); WiFi.mode(WIFI_STA); WiFi.setSleep(false); WiFi.setAutoReconnect(true);
    if(stored.length()&&!locked) {
        if(!config["dhcp"].as<bool>()) {
            IPAddress ip,mask,gateway,resolver;
            ip.fromString(config["ip"]|""); mask.fromString(config["subnet"]|"");
            gateway.fromString(config["gateway"]|""); resolver.fromString(config["dns"]|"");
            WiFi.config(ip,gateway,mask,resolver);
        }
        connectWiFi(config["ssid"]|"",config["wifi_password"]|"");
    } else if(strlen(REMOTE_BOOT_LOCAL_WIFI_SSID)) {
        config["ssid"]=REMOTE_BOOT_LOCAL_WIFI_SSID;
        config["wifi_password"]=REMOTE_BOOT_LOCAL_WIFI_PASSWORD;
        if(connectWiFi(config["ssid"]|"",config["wifi_password"]|"")) {
            Serial.println("Local Wi-Fi connected: "+WiFi.localIP().toString());
            Serial.println("Open the dashboard to complete first-run setup.");
            logEvent("LOCAL_WIFI_CONNECTED");
        } else {
            Serial.println("Local Wi-Fi connection failed; SoftAP is disabled. Retrying STA.");
            logEvent("LOCAL_WIFI_FAILED");
        }
    } else {
        Serial.println("No Wi-Fi credentials configured; SoftAP is disabled.");
        logEvent("WIFI_CREDENTIALS_MISSING");
    }
    routes(); startAgentSocket(); startSinric();
    microlink.begin(locked,setupMode,WiFi.status()==WL_CONNECTED,
        config["tailscale_auth_key"]|"",config["tailscale_device_name"]|"");
    logEvent(locked?"CONFIG_LOCKED_PRESERVED":"READY");
}
void loop() {
    if(sinricStarted && microlink.beginSinricHandle(sinricOnline)) {
        SinricPro.handle();
        microlink.endSinricHandle(sinricOnline,[]{
            SinricPro.stop();
            SinricPro.begin(config["sinric_app_key"].as<const char*>(),config["sinric_app_secret"].as<const char*>());
        });
        for(int i=0;i<8;++i) if(rb::sinricResetDue(slotReset[i],millis())) {
            SinricProSwitch& device=SinricPro[slotIds[i]];
            slotReset[i]=rb::nextSinricReset(millis(),device.sendPowerStateEvent(false));
        }
    }
    server.handleClient(); agentSocketTick(); wolTick(); microlink.tick(WiFi.status()==WL_CONNECTED);
    if(WiFi.status()!=WL_CONNECTED) WiFi.reconnect();
    if(restartAt&&static_cast<int32_t>(millis()-restartAt)>=0) ESP.restart();
    delay(1);
}
