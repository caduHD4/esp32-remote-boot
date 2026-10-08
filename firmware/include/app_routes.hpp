#pragma once
#include "app_pairing.hpp"
#include "app_socket.hpp"
void clearInvalidTargets(JsonDocument& next,JsonObject pc) {
    for(const char* key:{"default_target","fallback_boot_id","last_selected_target"}) if(!hasSystem(pc,idValue(pc[key]))) pc[key]="";
    if(String(next["sinric_pc_id"]|"")!=String(pc["pc_id"]|""))return;
    JsonArray slots=next["sinric_slots"].as<JsonArray>();for(int i=int(slots.size())-1;i>=0;--i) { String target=slots[i]["boot_id"]|"";if(target!="default"&&target!="shutdown"&&!hasSystem(pc,idValue(slots[i]["boot_id"])))slots.remove(i); }
}
void catalogSync(PcRuntime& runtime) {
    JsonDocument input,next,merged;if(!body(input))return;
    JsonArray fresh=input["systems"].as<JsonArray>();if(fresh.isNull()||fresh.size()>rb::MaxEntries) { errorReply(400,"CATALOG_LIMIT");return; }
    next.set(config);JsonObject pc=findPc(next,runtime.id);JsonArray list=merged.to<JsonArray>();
    for(JsonObject old:pc["systems"].as<JsonArray>()) for(JsonObject entry:fresh) if(idValue(old["id"])==idValue(entry["id"])) { entry["hidden"]=old["hidden"]|false;list.add(entry); }
    for(JsonObject entry:fresh) { bool found=false;for(JsonObject e:list) if(idValue(e["id"])==idValue(entry["id"])) found=true;if(!found)list.add(entry); }
    if(list.size()!=fresh.size()) { errorReply(400,"DUPLICATE_ID");return; }
    String before,after;serializeJson(pc["systems"],before);serializeJson(list,after);pc["systems"]=list;clearInvalidTargets(next,pc);
    if(before!=after) { pc["catalog_generation"]=pc["catalog_generation"].as<uint32_t>()+1;if(!commit(next))return; }
    JsonDocument reply;reply["synced"]=fresh.size();jsonReply(200,reply);
}
void pcRoute(const String& id,const String& action) {
    PcRuntime* pc=runtime(id);if(!pc) { errorReply(404,"PC_NOT_FOUND");return; }JsonObject settings=findPc(config,id);
    if(action==""&&server.method()==HTTP_GET) { JsonDocument reply;reply.set(settings);reply.remove("systems");reply["systems_count"]=settings["systems"].size();appendPcStatus(*pc,reply["status"].to<JsonObject>());jsonReply(200,reply);return; }
    if(action==""&&server.method()==HTTP_PUT) {
        JsonDocument input,next;if(!body(input))return;next.set(config);
        if(!patchAllowed(input.as<JsonObject>(),findPc(next,id),"|name|mac|wol_port|wol_repeat|wol_interval_ms|pending_ttl_s|physical_boot_behavior|default_target|fallback_boot_id|"))return;
        if(commit(next))savedReply();return;
    }
    if(action==""&&server.method()==HTTP_DELETE) {
        JsonDocument input,next;if(!body(input))return;if(String(input["confirm"]|"")!="DELETE_PC") { errorReply(400,"CONFIRM_REQUIRED");return; }
        next.set(config);JsonArray list=next["pcs"].as<JsonArray>();for(int i=int(list.size())-1;i>=0;--i)if(String(list[i]["pc_id"]|"")==id)list.remove(i);
        JsonArray agents=next["agents"].as<JsonArray>();for(int i=int(agents.size())-1;i>=0;--i)if(String(agents[i]["pc_id"]|"")==id)agents.remove(i);
        if(String(next["sinric_pc_id"]|"")==id) {next["sinric_pc_id"]="";next["sinric_enabled"]=false;next["sinric_slots"].to<JsonArray>();}
        if(!commit(next))return;for(auto& p:pairings)if(p.pcId==id)cancelPair(p);savedReply();return;
    }
    if(action=="status"&&server.method()==HTTP_GET) { JsonDocument reply;appendPcStatus(*pc,reply.to<JsonObject>());jsonReply(200,reply);return; }
    if(action=="systems"&&server.method()==HTTP_GET) {
        int offset=server.hasArg("offset")?server.arg("offset").toInt():0,limit=server.hasArg("limit")?server.arg("limit").toInt():8;
        if(offset<0||limit<1||limit>8) {errorReply(400,"INVALID_PAGE");return;}
        JsonDocument reply;JsonArray entries=reply["systems"].to<JsonArray>(),list=settings["systems"].as<JsonArray>();for(int i=offset;i<int(list.size())&&i<offset+limit;++i)entries.add(list[i]);
        reply["offset"]=offset;reply["total"]=list.size();reply["generation"]=settings["catalog_generation"]|1;jsonReply(200,reply);return;
    }
    if(action=="systems"&&server.method()==HTTP_PUT) {
        JsonDocument input,next,ordered;if(!body(input))return;JsonArray items=input["systems"].as<JsonArray>();
        if(items.isNull()||items.size()!=settings["systems"].size()) {errorReply(400,"CATALOG_MISMATCH");return;}
        next.set(config);JsonArray list=ordered.to<JsonArray>();for(JsonObject entry:items) {
            JsonObject old;for(JsonObject current:settings["systems"].as<JsonArray>())if(idValue(current["id"])==idValue(entry["id"]))old=current;
            if(old.isNull()) {errorReply(400,"CATALOG_MISMATCH");return;}JsonObject dest=list.add<JsonObject>();dest.set(old);dest["hidden"]=entry["hidden"]|false;
        }
        JsonObject updated=findPc(next,id);updated["systems"]=list;updated["catalog_generation"]=updated["catalog_generation"].as<uint32_t>()+1;if(commit(next))savedReply();return;
    }
    if(server.method()!=HTTP_POST) {errorReply(405,"METHOD_NOT_ALLOWED");return;}
    JsonDocument input;if(!body(input))return;
    if(action=="discovery"||action=="discovery/request") {++pc->discovery;JsonDocument reply;reply["generation"]=pc->discovery;jsonReply(202,reply);return;}
    int code=400;
    if(action=="boot") {bool force=input["force"]|false;if(force&&String(input["confirm"]|"")!="FORCE_BOOT"){errorReply(400,"CONFIRM_REQUIRED");return;}code=requestBoot(*pc,idValue(input["boot_id"]),force);}
    else if(action=="shutdown"||action=="reboot") {if(String(input["confirm"]|"")!=(action=="shutdown"?"SHUTDOWN":"REBOOT")){errorReply(400,"CONFIRM_REQUIRED");return;}code=requestPower(*pc,action,idValue(input["boot_id"]));}
    else {errorReply(404,"NOT_FOUND");return;}
    if(code!=202) {errorReply(code,code==403?"AGENT_PERMISSION_DISABLED":code==409?"PC_STATE_CONFLICT":code==429?"RATE_LIMIT":"COMMAND_REJECTED");return;}
    JsonDocument reply;reply["queued"]=true;reply["pc_id"]=id;jsonReply(202,reply);
}
void dynamicRoute() {
    String uri=server.uri();
    if(uri.startsWith("/api/v1/")) {errorReply(410,"PROTOCOL_VERSION_UNSUPPORTED");return;}
    if(uri.startsWith("/boot/")&&uri.endsWith(".ipxe")&&server.method()==HTTP_GET) {
        String id=uri.substring(6,uri.length()-5);PcRuntime* pc=runtime(id);if(!pc||locked||setupMode){server.sendHeader("Cache-Control","no-store");server.send(200,"text/plain","#!ipxe\nexit\n");return;}
        int target=pc->state.dispatch(millis());String script="#!ipxe\n";
        if(target>=0){script+="imgexec RemoteBoot.efi boot="+idText(target);if(pc->state.valid(pc->state.fallback)&&pc->state.fallback!=target)script+=" fallback="+idText(pc->state.fallback);script+=" || goto failed\nexit\n:failed\necho RemoteBoot failed\nexit 1\n";}else script+="exit\n";
        server.sendHeader("Cache-Control","no-store");server.send(200,"text/plain",script);return;
    }
    const String pairPrefix="/api/v2/pairing/";
    if(uri.startsWith(pairPrefix)){String rest=uri.substring(pairPrefix.length());int slash=rest.indexOf('/');pairingRoute(slash<0?rest:rest.substring(0,slash),slash<0?"":rest.substring(slash+1));return;}
    if(!auth())return;const String pcPrefix="/api/v2/pcs/";
    if(uri.startsWith(pcPrefix)){String rest=uri.substring(pcPrefix.length());int slash=rest.indexOf('/');pcRoute(slash<0?rest:rest.substring(0,slash),slash<0?"":rest.substring(slash+1));return;}
    const String agentPrefix="/api/v2/agents/";
    if(uri.startsWith(agentPrefix)&&server.method()==HTTP_DELETE) {
        String id=uri.substring(agentPrefix.length());if(findAgent(id).isNull()){errorReply(404,"AGENT_NOT_FOUND");return;}
        JsonDocument next;next.set(config);JsonArray agents=next["agents"].as<JsonArray>();for(int i=int(agents.size())-1;i>=0;--i)if(String(agents[i]["agent_id"]|"")==id)agents.remove(i);
        if(!commit(next))return;for(auto& pc:pcs)if(pc.agentId==id)detach(pc);for(auto& p:pairings)if(p.agentId==id)cancelPair(p);savedReply();return;
    }
    errorReply(404,"NOT_FOUND");
}
void routes() {
    const char* headers[]={"Authorization","If-None-Match","X-Agent-Session"};server.collectHeaders(headers,3);
    server.on("/",HTTP_GET,[]{String etag=String('"')+webAssetEtag+'"';server.sendHeader("ETag",etag);server.sendHeader("Cache-Control","no-cache");if(server.header("If-None-Match")==etag){server.send(304);return;}server.sendHeader("Content-Encoding","gzip");server.send_P(200,"text/html",reinterpret_cast<const char*>(webAsset),sizeof webAsset);});
    server.on("/api/v2/setup",HTTP_GET,[]{JsonDocument reply;reply["required"]=setupMode&&!locked;reply["config_locked"]=locked;jsonReply(200,reply);});
    server.on("/api/v2/setup",HTTP_POST,[]{
        if(!setupMode||locked){errorReply(409,"SETUP_CLOSED");return;}JsonDocument input,next;if(!body(input))return;
        const char* password=input["password"],*repeat=input["repeat_password"];
        if(!rb::validCredential(password)){errorReply(400,"INVALID_PASSWORD");return;}if(!repeat||strcmp(password,repeat)){errorReply(400,"PASSWORD_MISMATCH");return;}
        defaults(next);next["admin_token"]=password;if(!commit(next))return;setupMode=false;savedReply(200,true);restartAt=millis()+1500;
    });
    server.on("/api/v2/bootstrap",HTTP_GET,[]{if(auth())bootstrap();});
    server.on("/api/v2/status",HTTP_GET,[]{if(!auth())return;JsonDocument reply;appendStatus(reply.to<JsonObject>());jsonReply(200,reply);});
    server.on("/api/v2/pcs",HTTP_GET,[]{if(!auth())return;JsonDocument reply;JsonArray arr=reply["pcs"].to<JsonArray>();for(JsonObject pc:config["pcs"].as<JsonArray>()){JsonObject out=arr.add<JsonObject>();out.set(pc);out.remove("systems");}jsonReply(200,reply);});
    server.on("/api/v2/pcs",HTTP_POST,[]{
        if(!auth())return;if(config["pcs"].size()>=rb::MaxPcs){errorReply(409,"PC_CAPACITY");return;}JsonDocument input,next;if(!body(input))return;
        next.set(config);JsonObject pc=next["pcs"].as<JsonArray>().add<JsonObject>();pcDefaults(pc);
        if(!patchAllowed(input.as<JsonObject>(),pc,"|name|mac|wol_port|wol_repeat|wol_interval_ms|pending_ttl_s|physical_boot_behavior|default_target|fallback_boot_id|"))return;
        String id=pc["pc_id"]|"";if(!commit(next))return;JsonDocument reply;reply["pc"]=findPc(config,id);jsonReply(201,reply);
    });
    server.on("/api/v2/agents",HTTP_GET,[]{if(!auth())return;JsonDocument reply;appendAgents(reply["agents"].to<JsonArray>());jsonReply(200,reply);});
    server.on("/api/v2/config",HTTP_PUT,[]{
        if(!auth())return;JsonDocument input,next;if(!body(input))return;next.set(config);
        if(!patchAllowed(input.as<JsonObject>(),next.as<JsonObject>(),"|admin_token|tailscale_auth_key|tailscale_device_name|dhcp|ip|subnet|gateway|dns|"))return;
        if(commit(next)){savedReply(200,true);restartAt=millis()+1500;}
    });
    server.on("/api/v2/integrations/sinric",HTTP_PUT,[]{
        if(!auth())return;JsonDocument input,next;if(!body(input))return;next.set(config);String previous=config["sinric_pc_id"]|"",chosen=input["sinric_pc_id"]|previous.c_str();
        if(chosen!=previous)next["sinric_slots"].to<JsonArray>();
        if(!patchAllowed(input.as<JsonObject>(),next.as<JsonObject>(),"|sinric_pc_id|sinric_enabled|sinric_app_key|sinric_app_secret|sinric_slots|"))return;
        if(chosen!=previous)next["sinric_slots"].to<JsonArray>();
        if(commit(next)){savedReply(200,true);restartAt=millis()+1500;}
    });
    server.on("/api/v2/pairing/window",HTTP_POST,[]{if(!auth())return;if(locked||setupMode){errorReply(409,"SETUP_REQUIRED");return;}pairingLimits.enable(millis());JsonDocument reply;reply["expires_in"]=300;reply["open"]=true;jsonReply(200,reply);});
    server.on("/api/v2/pairing/window",HTTP_DELETE,[]{if(!auth())return;pairingLimits.enabled=false;for(auto& p:pairings)if(!p.agentId.length())cancelPair(p);savedReply();});
    server.on("/api/v2/pairing/start",HTTP_POST,pairingStart);
    server.on("/api/v2/pairing/lookup",HTTP_POST,[]{if(auth())pairingLookup();});
    server.on("/api/v2/agent/systems/sync",HTTP_POST,[]{PcRuntime* pc=agentAuth();if(pc)catalogSync(*pc);});
    server.on("/api/v2/logs",HTTP_GET,[]{if(!auth())return;JsonDocument reply;JsonArray arr=reply["logs"].to<JsonArray>();uint32_t count=logIndex<32?logIndex:32;for(uint32_t i=0;i<count;++i)arr.add(logs[(logIndex-count+i)%32]);jsonReply(200,reply);});
    server.on("/api/v2/system/reboot",HTTP_POST,[]{if(!auth())return;savedReply(202,true);restartAt=millis()+1000;});
    server.on("/api/v2/system/reset",HTTP_POST,[]{if(!auth())return;JsonDocument input;if(!body(input))return;if(String(input["confirm"]|"")!="FACTORY_RESET"){errorReply(400,"CONFIRM_REQUIRED");return;}if(!nvs.clear()){errorReply(500,"NVS_WRITE_FAILED");return;}savedReply(200,true);restartAt=millis()+1000;});
    server.onNotFound(dynamicRoute);server.begin();
}
