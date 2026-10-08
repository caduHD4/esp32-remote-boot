#include <SinricPro.h>
#include <SinricProSwitch.h>
#include "sinricpro_interface_compat.hpp"
#include "app_routes.hpp"
void startSinric() {
    if(setupMode||locked||!config["sinric_enabled"].as<bool>()||findPc(config,config["sinric_pc_id"]|"").isNull())return;
    size_t i=0;
    for(JsonObject slot:config["sinric_slots"].as<JsonArray>()) {
        slotIds[i]=slot["device_id"].as<String>();size_t index=i++;
        SinricProSwitch& device=SinricPro[slotIds[index]];
        device.onPowerState([](const String& deviceId,bool& on){
            if(!on)return true;if(!config["sinric_enabled"].as<bool>())return false;
            PcRuntime* pc=runtime(config["sinric_pc_id"]|"");if(!pc)return false;
            const char* ids[8]{};size_t count=0;for(JsonObject s:config["sinric_slots"].as<JsonArray>())ids[count++]=s["device_id"]|"";
            int slot=rb::sinricSlotForDevice(ids,count,deviceId.c_str());if(slot<0)return false;
            JsonObject settings=config["sinric_slots"][slot];String target=settings["boot_id"]|"default";
            int code=target=="shutdown"?requestPower(*pc,"shutdown"):requestBoot(*pc,target=="default"?pc->state.selected(millis()):idValue(settings["boot_id"]),false);
            const char* registered[8]{};for(int j=0;j<8;++j)registered[j]=slotIds[j].c_str();int idx=rb::sinricSlotForDevice(registered,8,deviceId.c_str());
            return idx>=0&&rb::acceptSinricDispatch(code,millis(),slotReset[idx]);
        });
    }
    SinricPro.onConnected([]{sinricOnline=true;});SinricPro.onDisconnected([]{sinricOnline=false;});
    SinricPro.begin(config["sinric_app_key"].as<const char*>(),config["sinric_app_secret"].as<const char*>());sinricStarted=true;
}
void setup() {
    Serial.begin(115200);delay(300);defaults(config);
    if(!nvs.begin("remote-boot-v3",false))locked=true;
    if(store.exists()) { JsonDocument loaded;if(!store.load(loaded)||!validate(loaded))locked=true;else {config.set(loaded);setupMode=false;} }
    reloadStates();WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setSleep(false);WiFi.setAutoReconnect(true);
    if(!strlen(REMOTE_BOOT_LOCAL_WIFI_SSID)) {locked=true;logEvent("LOCAL_WIFI_REQUIRED");}
    else {
        if(!config["dhcp"].as<bool>()) { IPAddress ip,mask,gateway,dns;ip.fromString(config["ip"]|"");mask.fromString(config["subnet"]|"");gateway.fromString(config["gateway"]|"");dns.fromString(config["dns"]|"");WiFi.config(ip,gateway,mask,dns); }
        wifiReconnect.reset(millis());WiFi.begin(REMOTE_BOOT_LOCAL_WIFI_SSID,REMOTE_BOOT_LOCAL_WIFI_PASSWORD);wifiReconnect.recordAttempt(millis());logEvent("WIFI_CONNECT_ATTEMPT");
    }
    routes();startAgentSocket();startSinric();microlink.begin(locked,setupMode,WiFi.status()==WL_CONNECTED,config["tailscale_auth_key"]|"",config["tailscale_device_name"]|"");logEvent(locked?"CONFIG_LOCKED_PRESERVED":"READY");
}
void loop() {
    if(sinricStarted&&microlink.beginSinricHandle(sinricOnline)) {
        SinricPro.handle();microlink.endSinricHandle(sinricOnline,[]{SinricPro.stop();SinricPro.begin(config["sinric_app_key"].as<const char*>(),config["sinric_app_secret"].as<const char*>());});
        for(int i=0;i<8;++i)if(rb::sinricResetDue(slotReset[i],millis())) {SinricProSwitch& device=SinricPro[slotIds[i]];slotReset[i]=rb::nextSinricReset(millis(),device.sendPowerStateEvent(false));}
    }
    bool connected=WiFi.status()==WL_CONNECTED;wifiReconnect.observe(connected,millis());
    if(connected&&!wifiAddressReported) {Serial.println("Wi-Fi connected: "+WiFi.localIP().toString());wifiAddressReported=true;}
    server.handleClient();agentSocketTick();wolTick();microlink.tick(connected);
    if(restartAt&&static_cast<int32_t>(millis()-restartAt)>=0)ESP.restart();delay(1);
}
