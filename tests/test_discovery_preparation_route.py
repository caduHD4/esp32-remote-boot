#!/usr/bin/env python3
"""Run the actual preparation HTTP handler with real JSON/transaction/store code.
Only HTTP transport and hardware Preferences are replaced by bounded native adapters.
"""
from pathlib import Path
import os
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
include=os.environ.get('ARDUINOJSON_INCLUDE',str(root/'.pio/libdeps/esp32c3_4mb/ArduinoJson/src'))
routes=(root/'firmware/include/app_routes.hpp').read_text()
handler=routes[routes.index('void preparationReport(){'):routes.index('void routes() {')]
prefix=r'''
#include <cassert>
#include <cstdio>
#include <string>
#include <ArduinoJson.h>
#include "discovery_config.hpp"
#include "config_store.hpp"
#include "document_transaction.hpp"
using String=std::string;
JsonDocument config,request;
Preferences nvs;rb::ConfigStore store(nvs);
bool locked=false,setupMode=false;int status=0;std::string supplied;
String bearer(){return supplied;}
void errorReply(int code,const char*){status=code;}
void savedReply(){status=200;}
bool body(JsonDocument& doc){doc.set(request);return true;}
JsonObject findPc(JsonDocument& doc,const String& id){for(JsonObject pc:doc["pcs"].as<JsonArray>())if(id==(pc["pc_id"]|""))return pc;return {};}
void restoreConfig(JsonDocument& live){assert(store.load(live));}
class ConfigTransaction:public rb::DocumentTransaction<JsonDocument>{public:ConfigTransaction():DocumentTransaction(config,restoreConfig){}};
bool commit(ConfigTransaction& next){if(!store.save(next)){status=500;return false;}next.accept();return true;}
'''
suffix=r'''
int main(){
 JsonObject pc=config["pcs"].to<JsonArray>().add<JsonObject>();pc["pc_id"]="11223344556677889900aabbccddeeff";pc["blind_mode"]=true;pc["mode_revision"]=2;
 JsonObject agent=config["agents"].to<JsonArray>().add<JsonObject>();agent["agent_id"]="aabbccddeeff00112233445566778899";agent["pc_id"]=pc["pc_id"];agent["token"]=std::string(64,'a');assert(store.save(config));
 request["pc_id"]=pc["pc_id"];request["agent_id"]=agent["agent_id"];request["blind_mode"]=true;request["mode_revision"]=2;request["discovery_version"]=1;request["boot_prepared"]=true;request["ipxe_sha256"]=std::string(64,'b');request["loader_sha256"]=std::string(64,'c');
 size_t writes=nvs.writes;supplied="wrong";preparationReport();assert(status==403&&nvs.writes==writes);
 supplied=std::string(64,'a');request["mode_revision"]=1;preparationReport();assert(status==409&&nvs.writes==writes);request["mode_revision"]=2;
 request["agent_id"]="00112233445566778899aabbccddeeff";preparationReport();assert(status==400&&nvs.writes==writes);request["agent_id"]="aabbccddeeff00112233445566778899";
 request["loader_sha256"]="bad";preparationReport();assert(status==400&&nvs.writes==writes);request["loader_sha256"]=std::string(64,'c');
 preparationReport();assert(status==200&&nvs.writes>writes);writes=nvs.writes;
 assert(config["pcs"][0]["boot_mode_revision"]==2);assert(config["agents"][0]["prepared_mode_revision"]==2);
 preparationReport();assert(status==200&&nvs.writes==writes);
 request["ipxe_sha256"]=std::string(64,'d');nvs.failSelector=true;preparationReport();assert(status==500);nvs.failSelector=false;
 assert(config["pcs"][0]["boot_ipxe_sha256"].as<std::string>()==std::string(64,'b'));
 preparationReport();assert(status==200&&config["pcs"][0]["boot_ipxe_sha256"].as<std::string>()==std::string(64,'d'));
 puts("PASS actual preparation route: bearer/identity/revision validation, durable report, idempotent NVS writes and rollback on persistence failure");
}
'''
with tempfile.TemporaryDirectory(prefix='rb-preparation-') as temp:
    cpp=Path(temp)/'route.cpp';binary=Path(temp)/'route'
    cpp.write_text(prefix+handler+suffix)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+include,'-I'+str(root/'tests/stubs'),'-I'+str(root/'firmware/include'),str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
