#pragma once
#include "app_model.hpp"
void socketReply(uint8_t num,JsonDocument& doc) { String value;serializeJson(doc,value);agentSocket.sendTXT(num,value); }
void socketIdentity(PcRuntime& pc,JsonDocument& doc) { doc["pc_id"]=pc.id;doc["agent_id"]=pc.agentId;doc["session_id"]=pc.session; }
void startAgentSocket() {
    agentSocket.begin();agentSocket.enableHeartbeat(60000,15000,2);
    agentSocket.onEvent([](uint8_t num,WStype_t type,uint8_t* payload,size_t length) {
        if(num>=WEBSOCKETS_SERVER_CLIENT_MAX)return;
        PcRuntime* pc=socketPc(num);
        if(type==WStype_CONNECTED) {
            unsigned waiting=0;for(bool value:socketWaiting) if(value) ++waiting;
            if(waiting>=1||length!=9||memcmp(payload,"/agent/v2",9)) { agentSocket.disconnect(num);return; }
            socketWaiting[num]=true;socketOpened[num]=millis();return;
        }
        if(type==WStype_DISCONNECTED) { socketWaiting[num]=false;if(pc) { pc->socket=-1;detach(*pc);logEvent("AGENT_DISCONNECTED"); }return; }
        if(type==WStype_PONG) {if(pc){pc->state.heartbeatAt=millis();AgentPresence* last=presence(pc->agentId);if(last)last->seen=millis();}return;}
        if(type==WStype_PING)return;
        if(type!=WStype_TEXT||length>12000) { agentSocket.disconnect(num);return; }
        JsonDocument doc;if(deserializeJson(doc,payload,length)||!doc.is<JsonObject>()) { agentSocket.disconnect(num);return; }
        String kind=doc["type"]|"";
        if(!pc) {
            JsonObject binding=findAgent(doc["agent_id"]|"");String session=doc["session_id"]|"";
            if(doc["protocol"].as<int>()!=2) { JsonDocument error;error["error"]="PROTOCOL_VERSION_UNSUPPORTED";socketReply(num,error);agentSocket.disconnect(num);return; }
            if(!socketWaiting[num]||kind!="hello"||locked||setupMode||!rb::validOpaqueId(session.c_str())||binding.isNull()||!rb::tokenEqual(doc["token"]|"",binding["token"]|"")) { agentSocket.disconnect(num);return; }
            pc=runtime(binding["pc_id"]|"");if(!pc||pc->socket>=0) { agentSocket.disconnect(num);return; }
            pc->socket=num;socketWaiting[num]=false;pc->session=session;pc->agentId=binding["agent_id"].as<String>();pc->sentCommand="";pc->power.clear();
            pc->reboot=doc["permissions"]["reboot"]|false;pc->shutdown=doc["permissions"]["shutdown"]|false;
            AgentPresence* last=presence(pc->agentId);if(last){last->seen=millis();last->reboot=pc->reboot;last->shutdown=pc->shutdown;}
            pc->hostname=String(doc["hostname"]|"").substring(0,63);pc->os=String(doc["os"]|"").substring(0,63);pc->state.heartbeat(millis(),idValue(doc["boot_id"]));pc->sentDiscovery=pc->discovery;
            doc.clear();doc["type"]="ready";socketIdentity(*pc,doc);socketReply(num,doc);logEvent("AGENT_CONNECTED");return;
        }
        if(String(doc["pc_id"]|"")!=pc->id||String(doc["agent_id"]|"")!=pc->agentId||String(doc["session_id"]|"")!=pc->session) { agentSocket.disconnect(num);return; }
        if(kind=="ack") {
            String id=doc["id"]|"";bool accepted=pc->power.active(millis())&&uint32_t(millis()-pc->sentAt)<15000&&pc->power.id==id.c_str()&&pc->power.session==pc->session.c_str();
            if(accepted) { accepted=(pc->power.action=="shutdown"?pc->shutdown:pc->reboot);if(accepted){pc->acceptedCommand=id;pc->acceptedAt=millis();}pc->power.clear(); }
            doc.clear();doc["type"]="ack";doc["id"]=id;doc["accepted"]=accepted;socketIdentity(*pc,doc);socketReply(num,doc);logEvent(accepted?"POWER_ACK_ACCEPTED":"POWER_ACK_REJECTED");
        } else if(kind=="result") {if(!pc->acceptedCommand.length()||String(doc["id"]|"")!=pc->acceptedCommand||uint32_t(millis()-pc->acceptedAt)>=30000){agentSocket.disconnect(num);return;}logEvent(doc["requested"].as<bool>()?"OS_POWER_REQUESTED":"OS_POWER_REFUSED");pc->acceptedCommand="";}
        else agentSocket.disconnect(num);
    });
}
void agentSocketTick() {
    agentSocket.loop();
    for(uint8_t num=0;num<WEBSOCKETS_SERVER_CLIENT_MAX;++num) if(socketWaiting[num]&&uint32_t(millis()-socketOpened[num])>=5000) agentSocket.disconnect(num);
    for(auto& pc:pcs) {
        if(pc.socket<0)continue;if(!pc.state.online(millis())) { detach(pc);continue; }
        if(pc.power.active(millis())&&pc.sentCommand!=pc.power.id.c_str()) {
            JsonDocument doc;doc["type"]="command";doc["id"]=pc.power.id.c_str();doc["action"]=pc.power.action.c_str();doc["boot_id"]=pc.power.target.c_str();socketIdentity(pc,doc);
            socketReply(pc.socket,doc);pc.sentCommand=pc.power.id.c_str();pc.sentAt=millis();
        }
        if(pc.sentDiscovery!=pc.discovery) { JsonDocument doc;doc["type"]="discover";doc["generation"]=pc.discovery;socketIdentity(pc,doc);socketReply(pc.socket,doc);pc.sentDiscovery=pc.discovery; }
    }
}
PcRuntime* agentAuth() {
    String token=bearer();for(JsonObject binding:config["agents"].as<JsonArray>()) if(rb::tokenEqual(token.c_str(),binding["token"]|"")) {
        PcRuntime* pc=runtime(binding["pc_id"]|"");
        if(pc&&pc->socket>=0&&pc->agentId==String(binding["agent_id"]|"")&&pc->session==server.header("X-Agent-Session")&&pc->state.online(millis())) return pc;
        errorReply(409,"AGENT_SESSION_REQUIRED");return nullptr;
    }
    errorReply(403,"FORBIDDEN");return nullptr;
}
