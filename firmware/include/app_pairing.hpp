#pragma once
#include "app_model.hpp"
String normalizeCode(String value) { value.toUpperCase();value.replace("-","");value.replace(" ","");return value; }
struct ConfirmReceipt {String id,secret;uint32_t created=0;};
ConfirmReceipt confirmReceipts[rb::MaxAgents];unsigned receiptIndex=0;
String pairingCode() { constexpr char alphabet[]="0123456789ABCDEFGHJKMNPQRSTVWXYZ";String code;for(int i=0;i<8;++i) { if(i==4)code+='-';code+=alphabet[esp_random()&31]; }return code; }
PendingPair* findPair(const String& id) { for(auto& p:pairings) if(p.id==id) return &p;return nullptr; }
void cancelPair(PendingPair& p) { p.canceled=true;p.token=""; }
void pairingStart() {
    if(locked||setupMode||!pairingLimits.open(millis())) { errorReply(403,"PAIRING_WINDOW_CLOSED");return; }
    if(!pairingLimits.startAllowed(millis())) { errorReply(429,"RATE_LIMIT");return; }
    JsonDocument input;if(!body(input))return;
    String hostname=input["hostname"]|"",os=input["os"]|"";
    if(!hostname.length()||hostname.length()>63||!os.length()||os.length()>63) { errorReply(400,"INVALID_METADATA");return; }
    PendingPair* pair=nullptr;for(auto& p:pairings) if(!p.active(millis())) { pair=&p;break; }
    if(!pair) { errorReply(429,"PAIRING_CAPACITY");return; }
    *pair=PendingPair();pair->id=randomHex();pair->secret=randomHex(32);pair->hostname=hostname;pair->os=os;pair->created=millis();
    bool duplicate;do { pair->code=pairingCode();duplicate=false;for(auto& p:pairings) if(&p!=pair&&p.active(millis())&&p.code==pair->code) duplicate=true; }while(duplicate);
    JsonDocument reply;reply["pairing_id"]=pair->id;reply["device_secret"]=pair->secret;reply["user_code"]=pair->code;reply["expires_in"]=300;reply["poll_interval"]=2;jsonReply(201,reply);
}
void pairingLookup() {
    if(!pairingLimits.lookupAllowed(millis())) { errorReply(429,"LOOKUP_LOCKED");return; }
    JsonDocument input;if(!body(input))return;String code=normalizeCode(input["code"]|"");
    for(auto& pair:pairings) if(pair.active(millis())&&normalizeCode(pair.code)==code) {
        JsonDocument reply;reply["pairing_id"]=pair.id;reply["hostname"]=pair.hostname;reply["os"]=pair.os;reply["expires_in"]=300-uint32_t(millis()-pair.created)/1000;jsonReply(200,reply);return;
    }
    errorReply(404,"PAIRING_NOT_FOUND");
}
void pairingRoute(const String& id,const String& action) {
    PendingPair* pair=findPair(id);if(!pair) {
        if(action=="confirm"&&server.method()==HTTP_POST) for(auto& receipt:confirmReceipts) if(receipt.id==id&&uint32_t(millis()-receipt.created)<300000&&rb::tokenEqual(bearer().c_str(),receipt.secret.c_str())) {savedReply();return;}
        errorReply(404,"PAIRING_NOT_FOUND");return;
    }
    if(action=="poll"||action=="confirm") {
        if(!rb::tokenEqual(bearer().c_str(),pair->secret.c_str())) { errorReply(403,"FORBIDDEN");return; }
        if(server.method()!=HTTP_POST) { errorReply(405,"METHOD_NOT_ALLOWED");return; }
        JsonDocument reply;
        if(uint32_t(millis()-pair->created)>=300000) reply["status"]="expired";
        else if(pair->canceled) reply["status"]="canceled";
        else if(action=="confirm") {
            if(!pair->agentId.length()||findAgent(pair->agentId).isNull()) { errorReply(409,"PAIRING_NOT_APPROVED");return; }
            if(!pair->confirmed) {auto& receipt=confirmReceipts[receiptIndex++%rb::MaxAgents];receipt.id=pair->id;receipt.secret=pair->secret;receipt.created=pair->created;}
            pair->confirmed=true;pair->token="";reply["saved"]=true;
        } else if(pair->confirmed) reply["status"]="confirmed";
        else if(pair->agentId.length()) {
            if(findAgent(pair->agentId).isNull()) { cancelPair(*pair);reply["status"]="canceled"; }
            else { reply["status"]="approved";reply["agent_id"]=pair->agentId;reply["pc_id"]=pair->pcId;reply["token"]=pair->token;reply["protocol"]=2; }
        } else reply["status"]="pending";
        jsonReply(200,reply);return;
    }
    if(!auth())return;
    if(server.method()==HTTP_DELETE&&action=="") { cancelPair(*pair);savedReply();return; }
    if(server.method()!=HTTP_POST||action!="approve") { errorReply(405,"METHOD_NOT_ALLOWED");return; }
    if(!pair->active(millis())) { errorReply(409,"PAIRING_EXPIRED");return; }
    if(pair->agentId.length()) { savedReply();return; }
    JsonDocument input,next;if(!body(input))return;String pcId=input["pc_id"]|"",name=input["installation_name"]|"";
    if(findPc(config,pcId).isNull()||!name.length()||name.length()>63) { errorReply(400,"INVALID_PAIRING_TARGET");return; }
    if(config["agents"].size()>=rb::MaxAgents) { errorReply(409,"AGENT_CAPACITY");return; }
    String agentId=randomHex(),token=randomHex(32);next.set(config);JsonObject agent=next["agents"].as<JsonArray>().add<JsonObject>();
    agent["agent_id"]=agentId;agent["pc_id"]=pcId;agent["token"]=token;agent["hostname"]=pair->hostname;agent["os"]=pair->os;agent["installation_name"]=name;
    if(!commit(next))return;
    pair->agentId=agentId;pair->pcId=pcId;pair->token=token;logEvent("AGENT_PAIRED");savedReply();
}
