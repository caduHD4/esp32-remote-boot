#pragma once
#include <ArduinoJson.h>
#include <stdint.h>
#include <string.h>
#include "boot_state.hpp"
namespace rb {
inline bool discoveryHex(const char* value,size_t bytes){
    if(!value||strlen(value)!=bytes*2)return false;
    for(size_t i=0;i<bytes*2;++i)if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))return false;
    return true;
}
inline bool discoveryPcValid(JsonObjectConst pc){
    return (pc["blind_mode"].isUnbound()||pc["blind_mode"].is<bool>())&&
        (pc["mode_revision"].isUnbound()||(pc["mode_revision"].is<uint32_t>()&&pc["mode_revision"].as<uint32_t>()>0))&&
        (pc["boot_mode_revision"].isUnbound()||pc["boot_mode_revision"].is<uint32_t>())&&
        (pc["boot_blind_mode"].isUnbound()||pc["boot_blind_mode"].is<bool>());
}
inline bool discoveryPcDefaults(JsonObject pc){
    if(pc["blind_mode"].isUnbound())pc["blind_mode"]=false;
    if(pc["mode_revision"].isUnbound())pc["mode_revision"]=1;
    if(pc["boot_mode_revision"].isUnbound())pc["boot_mode_revision"]=0;
    if(pc["boot_blind_mode"].isUnbound())pc["boot_blind_mode"]=false;
    return discoveryPcValid(pc);
}
// Call before copying permitted PC fields. Missing field preserves desired mode.
inline bool discoveryPatch(JsonObject pc,JsonObjectConst input,bool identityReady){
    if(input["blind_mode"].isUnbound())return true;
    if(!input["blind_mode"].is<bool>())return false;
    bool desired=input["blind_mode"].as<bool>(),previous=pc["blind_mode"]|false;
    if(desired&&!identityReady)return false;
    uint32_t revision=pc["mode_revision"]|uint32_t(1);
    if(desired!=previous){if(revision==UINT32_MAX)return false;pc["mode_revision"]=revision+1;}
    pc["blind_mode"]=desired;return true;
}
enum class Preparation{Invalid,Stale,Unchanged,Changed};
inline Preparation discoveryPreparation(JsonObject pc,JsonObject agent,JsonObjectConst input,const char* token,bool apply){
    if(pc.isNull()||agent.isNull()||!tokenEqual(token,agent["token"]|"")||
       strcmp(input["pc_id"]|"",pc["pc_id"]|"")||strcmp(input["pc_id"]|"",agent["pc_id"]|"")||strcmp(input["agent_id"]|"",agent["agent_id"]|"")||
       !input["mode_revision"].is<uint32_t>()||!input["mode_revision"].as<uint32_t>()||!input["blind_mode"].is<bool>()||
       !input["discovery_version"].is<unsigned>()||input["discovery_version"].as<unsigned>()!=1||!input["boot_prepared"].is<bool>())return Preparation::Invalid;
    uint32_t revision=input["mode_revision"];bool blind=input["blind_mode"],boot=input["boot_prepared"];
    if(revision!=(pc["mode_revision"]|uint32_t(1))||blind!=(pc["blind_mode"]|false))return Preparation::Stale;
    const char* ipxe=input["ipxe_sha256"]|"",*loader=input["loader_sha256"]|"";
    if(boot&&(!discoveryHex(ipxe,32)||!discoveryHex(loader,32)))return Preparation::Invalid;
    bool changed=(agent["prepared_mode_revision"]|uint32_t(0))!=revision||!agent["prepared_blind_mode"].is<bool>()||(agent["prepared_blind_mode"]|false)!=blind||
        (agent["prepared_discovery_version"]|0)!=1||!agent["boot_prepared"].is<bool>()||(agent["boot_prepared"]|false)!=boot;
    if(boot)changed=changed||(pc["boot_mode_revision"]|uint32_t(0))!=revision||(pc["boot_blind_mode"]|false)!=blind||strcmp(pc["boot_ipxe_sha256"]|"",ipxe)||strcmp(pc["boot_loader_sha256"]|"",loader)||strcmp(agent["ipxe_sha256"]|"",ipxe)||strcmp(agent["loader_sha256"]|"",loader);
    if(!changed)return Preparation::Unchanged;
    if(apply){
        agent["prepared_mode_revision"]=revision;agent["prepared_blind_mode"]=blind;agent["prepared_discovery_version"]=1;agent["boot_prepared"]=boot;
        if(boot){pc["boot_mode_revision"]=revision;pc["boot_blind_mode"]=blind;pc["boot_ipxe_sha256"]=ipxe;pc["boot_loader_sha256"]=loader;agent["ipxe_sha256"]=ipxe;agent["loader_sha256"]=loader;}
    }
    return Preparation::Changed;
}
}
