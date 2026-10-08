#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WebSocketsServer.h>
#include "boot_state.hpp"
#include "power_command.hpp"
#include "credential_policy.hpp"
#include "wifi_reconnect_policy.hpp"
#include "sinric_policy.hpp"
#include "microlink_runtime.hpp"
#include "config_store.hpp"
#include "document_transaction.hpp"
#include "local_wifi.h"
#include "web_asset.h"

constexpr char Version[]="3.0.0-alpha.1";
WebServer server(80); WiFiUDP udp; Preferences nvs; rb::ConfigStore store(nvs);
WebSocketsServer agentSocket(81); JsonDocument config;
rb::MicrolinkRuntime microlink;rb::WiFiReconnectPolicy wifiReconnect;
bool locked=false,setupMode=true,sinricOnline=false,sinricStarted=false,wifiAddressReported=false;
uint32_t restartAt=0,logIndex=0;String logs[32],slotIds[8];uint32_t slotReset[8]{};
struct PcRuntime {
    String id,session,agentId,hostname,os,sentCommand,acceptedCommand;
    rb::PcState state;rb::PowerCommand power;
    int socket=-1;bool reboot=false,shutdown=false,wolSent=false;
    uint32_t lastWol=0,wolAt=0,discovery=1,sentDiscovery=0,sentAt=0,acceptedAt=0;
    uint8_t wolRemaining=0;
};
PcRuntime pcs[rb::MaxPcs];
struct AgentPresence {String id;uint32_t seen=0;bool reboot=false,shutdown=false;};
AgentPresence agentPresence[rb::MaxAgents];
JsonObject findAgent(const String& id);
AgentPresence* presence(const String& id) {for(auto& a:agentPresence)if(a.id==id)return &a;for(auto& a:agentPresence)if(!a.id.length()||findAgent(a.id).isNull()){a=AgentPresence();a.id=id;return &a;}return nullptr;}
struct PendingPair {
    String id,secret,code,hostname,os,agentId,pcId,token;
    uint32_t created=0;bool canceled=false,confirmed=false;
    bool active(uint32_t now) const { return id.length()&&!canceled&&!confirmed&&uint32_t(now-created)<300000; }
};
PendingPair pairings[rb::MaxPairings];rb::PairingLimits pairingLimits;
bool socketWaiting[WEBSOCKETS_SERVER_CLIENT_MAX]{};uint32_t socketOpened[WEBSOCKETS_SERVER_CLIENT_MAX]{};

String idText(int id) { if(id<0) return "";char text[5];snprintf(text,sizeof text,"%04X",id);return text; }
int idValue(JsonVariantConst value) { int id=-1;rb::parseId(value.as<const char*>(),id);return id; }
String randomHex(unsigned bytes=16) { String value;value.reserve(bytes*2);for(unsigned i=0;i<bytes;i+=4) { char b[9];snprintf(b,sizeof b,"%08lx",static_cast<unsigned long>(esp_random()));value+=b; }return value; }
void logEvent(const char* text) { logs[logIndex++%32]=String(millis()/1000)+" "+text; }
void jsonReply(int code,JsonDocument& doc) {
    server.sendHeader("Cache-Control","no-store");String data;size_t expected=measureJson(doc);
    if(doc.overflowed()||!data.reserve(expected+1)||serializeJson(doc,data)!=expected||data.length()!=expected) {
        server.send(503,"application/json","{\"error\":\"MEMORY_BUSY\"}");return;
    }
    server.send(code,"application/json",data);
}
void errorReply(int code,const char* error) { JsonDocument doc;doc["error"]=error;jsonReply(code,doc); }
void savedReply(int code=200,bool restarting=false) { JsonDocument doc;doc["saved"]=true;doc["restarting"]=restarting;jsonReply(code,doc); }
bool body(JsonDocument& doc) {
    if(server.arg("plain").length()>12000) { errorReply(413,"BODY_TOO_LARGE");return false; }
    if(deserializeJson(doc,server.arg("plain"))||!doc.is<JsonObject>()) { errorReply(400,"INVALID_JSON");return false; }return true;
}
String bearer() { String value=server.header("Authorization");if(!value.startsWith("Bearer ")) return "";value.remove(0,7);return value; }
bool auth() {String supplied=bearer();if(rb::tokenEqual(supplied.c_str(),config["admin_token"]|""))return true;errorReply(supplied.length()?403:401,supplied.length()?"FORBIDDEN":"AUTH_REQUIRED");return false;}
JsonObject findPc(JsonDocument& doc,const String& id) { for(JsonObject pc:doc["pcs"].as<JsonArray>()) if(id==String(pc["pc_id"]|"")) return pc;return JsonObject(); }
JsonObject findAgent(const String& id) { for(JsonObject a:config["agents"].as<JsonArray>()) if(id==String(a["agent_id"]|"")) return a;return JsonObject(); }
PcRuntime* runtime(const String& id) { for(auto& pc:pcs) if(pc.id==id&&id.length()) return &pc;return nullptr; }
PcRuntime* socketPc(uint8_t num) { for(auto& pc:pcs) if(pc.socket==num) return &pc;return nullptr; }
void detach(PcRuntime& pc) { int old=pc.socket;pc.socket=-1;pc.power.clear();pc.acceptedCommand="";pc.session="";pc.agentId="";pc.reboot=pc.shutdown=false;pc.state.heartbeatSeen=false;if(old>=0) agentSocket.disconnect(old); }
bool parseMac(const char* text,uint8_t* mac) {
    if(!text||strlen(text)!=17) return false;
    auto hex=[](char c)->int { return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1; };bool nonzero=false;
    for(int i=0;i<6;++i) { int a=hex(text[i*3]),b=hex(text[i*3+1]);if(a<0||b<0||(i<5&&text[i*3+2]!=':')) return false;mac[i]=a*16+b;nonzero|=mac[i]!=0; }return nonzero&&!(mac[0]&1);
}
void defaults(JsonDocument& doc) {
    doc["config_version"]=3;doc["dhcp"]=true;doc["sinric_enabled"]=false;doc["sinric_pc_id"]="";
    doc["tailscale_auth_key"]="";doc["tailscale_device_name"]="ESP32C3";
    doc["pcs"].to<JsonArray>();doc["agents"].to<JsonArray>();doc["sinric_slots"].to<JsonArray>();
}
void pcDefaults(JsonObject pc) {
    pc["pc_id"]=randomHex();pc["name"]="PC";pc["mac"]="";pc["wol_port"]=9;pc["wol_repeat"]=5;pc["wol_interval_ms"]=100;
    pc["pending_ttl_s"]=180;pc["physical_boot_behavior"]="default_target";pc["default_target"]="";pc["fallback_boot_id"]="";
    pc["systems"].to<JsonArray>();pc["catalog_generation"]=1;
}
bool hasSystem(JsonObjectConst pc,int id) { if(id<0) return false;for(JsonObjectConst e:pc["systems"].as<JsonArrayConst>()) if(idValue(e["id"])==id&&!e["blocked"].as<bool>()) return true;return false; }
bool validatePc(JsonObject pc) {
    uint8_t mac[6];const char* name=pc["name"]|"";
    if(!rb::validOpaqueId(pc["pc_id"])||!*name||strlen(name)>63||!parseMac(pc["mac"],mac)) return false;
    int port=pc["wol_port"],repeat=pc["wol_repeat"],interval=pc["wol_interval_ms"],ttl=pc["pending_ttl_s"];
    if(port<1||port>65535||repeat<1||repeat>10||interval<20||interval>1000||ttl<30||ttl>3600) return false;
    String behavior=pc["physical_boot_behavior"]|"";if(behavior!="default_target"&&behavior!="last_selected"&&behavior!="exit_to_firmware") return false;
    JsonArray systems=pc["systems"].as<JsonArray>();if(systems.isNull()||systems.size()>rb::MaxEntries) return false;
    for(size_t i=0;i<systems.size();++i) {
        JsonObject e=systems[i];const char* label=e["name"]|"";int id=idValue(e["id"]);if(id<0||!*label||strlen(label)>63) return false;
        for(size_t j=0;j<i;++j) if(idValue(systems[j]["id"])==id) return false;
        String lower=label;lower.toLowerCase();if(lower.indexOf("remote boot")>=0||lower.indexOf("ipxe")>=0) e["blocked"]=true;
        e["id"]=idText(id);
    }
    for(const char* key:{"default_target","fallback_boot_id"}) if(strlen(pc[key]|"")&&!hasSystem(pc,idValue(pc[key]))) return false;
    return true;
}
bool validate(JsonDocument& doc) {
    if(doc.overflowed()||doc["config_version"].as<int>()!=3||!rb::validCredential(doc["admin_token"].as<const char*>())) return false;
    if(!doc["dhcp"].as<bool>()) { IPAddress address;for(const char* k:{"ip","subnet","gateway","dns"}) if(!address.fromString(doc[k]|"")) return false; }
    const char* key=doc["tailscale_auth_key"]|"";
    if(strlen(key)>159||(*key&&(strncmp(key,"tskey-auth-",11)||strlen(key)<20))||strlen(doc["tailscale_device_name"]|"")>63) return false;
    JsonArray list=doc["pcs"].as<JsonArray>(),agents=doc["agents"].as<JsonArray>(),slots=doc["sinric_slots"].as<JsonArray>();
    if(list.isNull()||list.size()>rb::MaxPcs||agents.isNull()||agents.size()>rb::MaxAgents||slots.isNull()||slots.size()>8) return false;
    for(size_t i=0;i<list.size();++i) { if(!validatePc(list[i])) return false;for(size_t j=0;j<i;++j) if(String(list[i]["pc_id"]|"")==String(list[j]["pc_id"]|"")||String(list[i]["mac"]|"").equalsIgnoreCase(list[j]["mac"]|"")) return false; }
    for(size_t i=0;i<agents.size();++i) {
        JsonObject a=agents[i];if(!rb::validOpaqueId(a["agent_id"])||findPc(doc,a["pc_id"]|"").isNull()||strlen(a["token"]|"")!=64||strlen(a["installation_name"]|"")>63) return false;
        for(size_t j=0;j<i;++j) if(String(a["agent_id"]|"")==String(agents[j]["agent_id"]|"")||rb::tokenEqual(a["token"],agents[j]["token"])) return false;
    }
    JsonObject selected=findPc(doc,doc["sinric_pc_id"]|"");
    if(doc["sinric_enabled"].as<bool>()&&selected.isNull()) return false;
    if(rb::sinricReadiness(doc["sinric_enabled"]|false,doc["sinric_app_key"]|"",doc["sinric_app_secret"]|"")==rb::SinricReadiness::MissingCredentials) return false;
    for(size_t i=0;i<slots.size();++i) {
        char sid[25];if(selected.isNull()||!rb::normalizeSinricDeviceId(slots[i]["device_id"]|"",sid)) return false;slots[i]["device_id"]=sid;
        for(size_t j=0;j<i;++j) if(rb::sinricDeviceIdEqual(sid,slots[j]["device_id"]|"")) return false;
        String target=slots[i]["boot_id"]|"";if(target!="default"&&target!="shutdown"&&!hasSystem(selected,idValue(slots[i]["boot_id"]))) return false;
    }
    return measureJson(doc)<=20000;
}
void reloadStates() {
    for(auto& pc:pcs) if(pc.id.length()&&findPc(config,pc.id).isNull()) { detach(pc);pc=PcRuntime(); }
    for(JsonObject settings:config["pcs"].as<JsonArray>()) {
        String id=settings["pc_id"]|"";PcRuntime* pc=runtime(id);
        if(!pc) for(auto& free:pcs) if(!free.id.length()) { pc=&free;pc->id=id;break; }
        if(!pc) continue;
        pc->state.count=0;for(JsonObject e:settings["systems"].as<JsonArray>()) { auto& dest=pc->state.entries[pc->state.count++];dest.id=idValue(e["id"]);dest.hidden=e["hidden"]|false;dest.blocked=e["blocked"]|false; }
        pc->state.defaultTarget=idValue(settings["default_target"]);pc->state.fallback=idValue(settings["fallback_boot_id"]);pc->state.ttl=settings["pending_ttl_s"].as<uint32_t>()*1000;
        pc->state.lastSelected=idValue(settings["last_selected_target"]);
        String b=settings["physical_boot_behavior"]|"default_target";pc->state.behavior=b=="last_selected"?rb::PcState::Last:b=="exit_to_firmware"?rb::PcState::Exit:rb::PcState::Default;pc->state.reconcile();pc->state.heartbeatExpiry=90000;
    }
}
void restoreConfig(JsonDocument& live) {
    if(!store.load(live)||!validate(live)){locked=true;logEvent("CONFIG_ROLLBACK_FAILED");return;}
    reloadStates();
}
class ConfigTransaction:public rb::DocumentTransaction<JsonDocument> {
public:ConfigTransaction():DocumentTransaction(config,restoreConfig){}
};
bool commit(ConfigTransaction& next) {
    if(locked) { errorReply(409,"SCHEMA_LOCKED");return false; }
    if(!validate(next)) { errorReply(400,"INVALID_CONFIG");return false; }
    if(!store.save(next)) { errorReply(500,"NVS_WRITE_FAILED");return false; }
    next.accept();reloadStates();return true;
}
bool patchAllowed(JsonObject input,JsonObject destination,const char* allowed) {
    for(JsonPair p:input) { if(!strstr(allowed,(String("|")+p.key().c_str()+"|").c_str())) { errorReply(400,"UNKNOWN_FIELD");return false; }destination[p.key()]=p.value(); }return true;
}
void appendPcStatus(PcRuntime& pc,JsonObject d) {
    d["pc_id"]=pc.id;d["online"]=pc.state.online(millis());d["hostname"]=pc.hostname;d["os"]=pc.os;
    d["agent_id"]=pc.agentId;d["agent_transport"]="websocket-v2";d["reboot_enabled"]=pc.reboot&&pc.state.online(millis());d["shutdown_enabled"]=pc.shutdown&&pc.state.online(millis());
    d["pending_target"]=idText(pc.state.pendingValid(millis())?pc.state.pending:-1);d["default_target"]=idText(pc.state.defaultTarget);d["last_selected_target"]=idText(pc.state.lastSelected);d["pending_ttl_s"]=pc.state.ttl/1000;d["discovery_generation"]=pc.discovery;
}
void appendStatus(JsonObject d) {
    d["version"]=Version;d["setup_mode"]=setupMode;d["config_locked"]=locked;d["ip"]=WiFi.localIP().toString();d["rssi"]=WiFi.RSSI();d["wifi_connected"]=WiFi.status()==WL_CONNECTED;
    d["wifi_reconnect_attempts"]=wifiReconnect.attempts();d["wifi_retry_in_ms"]=wifiReconnect.retryInMs(millis());d["sinric_online"]=sinricOnline;d["uptime"]=millis()/1000;d["heap"]=ESP.getFreeHeap();
    d["pairing_window_open"]=pairingLimits.open(millis());d["max_pcs"]=rb::MaxPcs;d["max_agents"]=rb::MaxAgents;
    auto tail=microlink.snapshot();JsonObject t=d["tailscale"].to<JsonObject>();
    t["built"]=tail.built;t["configured"]=tail.configured;t["connected"]=tail.connected;t["state"]=tail.state;t["ip"]=String(tail.ip);t["peers"]=tail.peers;t["control_online"]=tail.controlOnline;t["derp_online"]=tail.derpOnline;t["derp_server_info"]=tail.serverInfo;
    t["map_updates"]=tail.mapUpdates;t["reconnects"]=tail.reconnects;t["tls_deferred"]=tail.tlsDeferred;t["wg_encrypted_rx"]=tail.encryptedRx;t["wg_authenticated_rx"]=tail.authenticatedRx;t["authenticated_age_ms"]=tail.authenticatedAgeMs;t["heap_free"]=tail.heapFree;t["heap_minimum"]=tail.heapMinimum;t["largest_block"]=tail.largestBlock;
}
void appendAgents(JsonArray arr) {for(JsonObject a:config["agents"].as<JsonArray>()) {JsonObject out=arr.add<JsonObject>();for(const char* k:{"agent_id","pc_id","installation_name","hostname","os"})out[k]=a[k];PcRuntime* pc=runtime(a["pc_id"]|"");bool online=pc&&pc->agentId==String(a["agent_id"]|"")&&pc->state.online(millis());AgentPresence* last=presence(a["agent_id"]|"");out["online"]=online;out["connected"]=online;out["id"]=a["agent_id"];out["last_seen"]=last&&last->seen?String(uint32_t(millis()-last->seen)/1000)+" s atrás (desde o reinício da ESP)":String("");out["permissions"]["reboot"]=last&&last->reboot;out["permissions"]["shutdown"]=last&&last->shutdown;}}
void copyPcMetadata(JsonObject out,JsonObjectConst pc) {for(JsonPairConst value:pc)if(strcmp(value.key().c_str(),"systems"))out[value.key().c_str()]=value.value();}
void bootstrap() {
    JsonDocument doc;JsonObject globals=doc["config"].to<JsonObject>();
    for(JsonPair p:config.as<JsonObject>()) { String key=p.key().c_str();if(key=="pcs"||key=="agents") continue;if(key=="admin_token"||key=="tailscale_auth_key"||key=="sinric_app_key"||key=="sinric_app_secret") globals[key+"_set"]=strlen(p.value().as<const char*>()?p.value().as<const char*>():"")>0;else globals[key]=p.value(); }
    globals["ssid"]=REMOTE_BOOT_LOCAL_WIFI_SSID;globals["wifi_password_set"]=strlen(REMOTE_BOOT_LOCAL_WIFI_PASSWORD)>0;
    JsonArray list=doc["pcs"].to<JsonArray>();for(JsonObject pc:config["pcs"].as<JsonArray>()) { JsonObject out=list.add<JsonObject>();copyPcMetadata(out,pc);out["systems_count"]=pc["systems"].size();PcRuntime* r=runtime(pc["pc_id"]|"");if(r) appendPcStatus(*r,out["status"].to<JsonObject>()); }
    appendAgents(doc["agents"].to<JsonArray>());appendStatus(doc["status"].to<JsonObject>());jsonReply(200,doc);
}
int requestBoot(PcRuntime& pc,int target,bool force) {
    if(locked||setupMode||WiFi.status()!=WL_CONNECTED) return 503;
    if(pc.wolRemaining||(pc.wolSent&&uint32_t(millis()-pc.lastWol)<3000)) return 429;
    if(!pc.state.valid(target)) return 400;if(pc.state.online(millis())&&!force) return 409;
    ConfigTransaction next;next.take();findPc(next,pc.id)["last_selected_target"]=idText(target);
    if(!validate(next)||!store.save(next)) return 500;next.accept();pc.state.request(target,millis(),force);
    pc.wolRemaining=findPc(config,pc.id)["wol_repeat"];pc.wolAt=millis();logEvent("BOOT_QUEUED");return 202;
}
int requestPower(PcRuntime& pc,const String& action,int target=-1) {
    if(locked||setupMode) return 503;if(!pc.state.online(millis())) return 409;
    if((action=="shutdown"&&!pc.shutdown)||(action=="reboot"&&!pc.reboot)) return 403;
    if(action=="reboot"&&!pc.state.valid(target)) return 400;
    if(!pc.power.enqueue(randomHex().c_str(),action.c_str(),idText(target).c_str(),pc.session.c_str(),millis())) return 409;
    pc.state.pending=rb::None;pc.wolRemaining=0;pc.sentCommand="";logEvent("POWER_QUEUED");return 202;
}
void wolTick() {
    for(auto& pc:pcs) {
        if(!pc.wolRemaining||static_cast<int32_t>(millis()-pc.wolAt)<0) continue;
        JsonObject settings=findPc(config,pc.id);uint8_t mac[6],packet[102];if(!parseMac(settings["mac"],mac)) { pc.wolRemaining=0;continue; }
        memset(packet,255,6);for(int i=0;i<16;++i) memcpy(packet+6+i*6,mac,6);
        IPAddress local=WiFi.localIP(),mask=WiFi.subnetMask(),broadcast;for(int i=0;i<4;++i) broadcast[i]=local[i]|~mask[i];
        if(udp.beginPacket(broadcast,settings["wol_port"].as<uint16_t>())) { udp.write(packet,sizeof packet);logEvent(udp.endPacket()?"WOL_SENT":"WOL_FAILED"); }
        --pc.wolRemaining;pc.lastWol=millis();pc.wolSent=true;pc.wolAt=millis()+settings["wol_interval_ms"].as<uint32_t>();
    }
}
