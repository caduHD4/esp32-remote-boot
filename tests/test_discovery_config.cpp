#include <cassert>
#include <cstdio>
#include <string>
#include <ArduinoJson.h>
#include "../firmware/include/discovery_config.hpp"
int main(){
 JsonDocument doc;JsonObject pc=doc["pc"].to<JsonObject>(),agent=doc["agent"].to<JsonObject>();
 assert(rb::discoveryPcValid(pc));assert(pc.size()==0);
 assert(rb::discoveryPcDefaults(pc));assert(pc["blind_mode"]==false&&pc["mode_revision"]==1&&pc["boot_mode_revision"]==0);
 JsonDocument patch;patch["blind_mode"]="true";assert(!rb::discoveryPatch(pc,patch.as<JsonObject>(),true));assert(pc["mode_revision"]==1);
 patch["blind_mode"]=true;assert(!rb::discoveryPatch(pc,patch.as<JsonObject>(),false));assert(pc["blind_mode"]==false);
 assert(rb::discoveryPatch(pc,patch.as<JsonObject>(),true));assert(pc["blind_mode"]==true&&pc["mode_revision"]==2);
 assert(rb::discoveryPatch(pc,patch.as<JsonObject>(),true));assert(pc["mode_revision"]==2);
 patch.clear();assert(rb::discoveryPatch(pc,patch.as<JsonObject>(),false));assert(pc["blind_mode"]==true&&pc["mode_revision"]==2);
 patch["blind_mode"]=false;assert(rb::discoveryPatch(pc,patch.as<JsonObject>(),false));assert(pc["mode_revision"]==3);
 pc["mode_revision"]=UINT32_MAX;patch["blind_mode"]=true;assert(!rb::discoveryPatch(pc,patch.as<JsonObject>(),true));pc["mode_revision"]=3;
 for(const char* bad:{"null","1","\"false\""}){JsonDocument invalid;deserializeJson(invalid,std::string("{\"blind_mode\":")+bad+"}");assert(!rb::discoveryPcDefaults(invalid.as<JsonObject>()));}
 pc["pc_id"]="11223344556677889900aabbccddeeff";agent["pc_id"]=pc["pc_id"];agent["agent_id"]="aabbccddeeff00112233445566778899";agent["token"]=std::string(64,'a');
 JsonDocument report;report["pc_id"]=pc["pc_id"];report["agent_id"]=agent["agent_id"];report["mode_revision"]=3;report["blind_mode"]=false;report["discovery_version"]=1;report["boot_prepared"]=true;
 const std::string token(64,'a');
 assert(rb::discoveryPreparation(pc,agent,report.as<JsonObject>(),token.c_str(),false)==rb::Preparation::Invalid);
 report["ipxe_sha256"]=std::string(64,'b');report["loader_sha256"]=std::string(64,'c');
 assert(rb::discoveryPreparation(pc,agent,report.as<JsonObject>(),"wrong",false)==rb::Preparation::Invalid);
 report["mode_revision"]=2;assert(rb::discoveryPreparation(pc,agent,report.as<JsonObject>(),token.c_str(),false)==rb::Preparation::Stale);report["mode_revision"]=3;
 report["pc_id"]="ffeeddccbbaa00998877665544332211";assert(rb::discoveryPreparation(pc,agent,report.as<JsonObject>(),token.c_str(),false)==rb::Preparation::Invalid);report["pc_id"]=pc["pc_id"];
 report["loader_sha256"]=std::string(64,'C');assert(rb::discoveryPreparation(pc,agent,report.as<JsonObject>(),token.c_str(),false)==rb::Preparation::Invalid);report["loader_sha256"]=std::string(64,'c');
 assert(rb::discoveryPreparation(pc,agent,report.as<JsonObject>(),token.c_str(),false)==rb::Preparation::Changed);assert(pc["boot_mode_revision"]==0);
 assert(rb::discoveryPreparation(pc,agent,report.as<JsonObject>(),token.c_str(),true)==rb::Preparation::Changed);assert(pc["boot_mode_revision"]==3&&pc["boot_blind_mode"]==false&&agent["prepared_mode_revision"]==3);
 assert(rb::discoveryPreparation(pc,agent,report.as<JsonObject>(),token.c_str(),true)==rb::Preparation::Unchanged);
 report["boot_prepared"]=false;report.remove("ipxe_sha256");report.remove("loader_sha256");assert(rb::discoveryPreparation(pc,agent,report.as<JsonObject>(),token.c_str(),true)==rb::Preparation::Changed);assert(pc["boot_mode_revision"]==3&&agent["boot_prepared"]==false);
 puts("PASS legacy defaults, strict modes/revisions, preserving updates, identity gating, authenticated matching preparation reports and idempotency");
}
