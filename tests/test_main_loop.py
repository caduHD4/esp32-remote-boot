#!/usr/bin/env python3
"""Exercise the actual firmware loop with deterministic network/time adapters."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'firmware/src/main.cpp').read_text()
loop = source[source.index('void loop() {'):]
prefix = r'''
#include <cassert>
#include <cstdint>
#include <string>
#include <cstring>
#include <cstdio>
#include "wifi_reconnect_policy.hpp"
constexpr int WL_CONNECTED=3;
constexpr char REMOTE_BOOT_LOCAL_WIFI_SSID[]="test-network";
uint32_t clockNow=1000;
uint32_t millis(){return clockNow;}
void delay(int){}
struct Address {std::string toString(){return "10.0.1.10";}};
struct FakeWiFi {
 int state=0, reconnects=0;
 bool acceptReconnect=true;
 int status(){return state;}
 bool reconnect(){++reconnects;return acceptReconnect;}
 Address localIP(){return {};}
 int RSSI(){return -50;}
} WiFi;
struct FakeSerial {
 void println(const std::string&){}
 template<class... Args> void printf(const char*,Args...){}
} Serial;
struct FakeESP {
 void restart(){}
 uint32_t getFreeHeap(){return 60000;}
 uint32_t getMinFreeHeap(){return 50000;}
 uint32_t getMaxAllocHeap(){return 32000;}
} ESP;
struct Field {template<class T>T as(){return "test";}};
struct Config {Field operator[](const char*){return {};}} config;
struct Cloud {
 int handles=0;
 void handle(){++handles;}
 void stop(){}
 void begin(const char*,const char*){}
} SinricPro;
struct SinricProSwitch {bool sendPowerStateEvent(bool){return true;}};
SinricProSwitch fakeSwitch;
SinricProSwitch& operatorLookup(const std::string&){return fakeSwitch;}
struct Runtime {
 bool beginSinricHandle(bool){return true;}
 template<class F>void endSinricHandle(bool,F){}
 void tick(bool){}
} microlink;
struct Server {int ticks=0;void handleClient(){++ticks;}} server;
void agentSocketTick(){}
void wolTick(){}
void logEvent(const char*){}
rb::WiFiReconnectPolicy wifiReconnect;
bool sinricStarted=true,sinricOnline=false,wifiAddressReported=false;
uint32_t restartAt=0,slotReset[8]{};
std::string slotIds[8];
namespace rb {
bool sinricResetDue(uint32_t,uint32_t){return false;}
uint32_t nextSinricReset(uint32_t n,bool){return n;}
}
'''
# Replace only the SDK's operator[] spelling; the loop control flow is unchanged.
loop = loop.replace('SinricPro[slotIds[i]]', 'operatorLookup(slotIds[i])')
suffix = r'''
int main(){
 wifiReconnect.reset(clockNow);wifiReconnect.recordAttempt(clockNow);
 loop();
 assert(SinricPro.handles==0 && "Sinric must not attempt DNS/TLS without an IP");
 assert(server.ticks==1);
 assert(WiFi.reconnects==0 && "allow first association to finish");
 clockNow=21000;loop();
 assert(WiFi.reconnects==1 && "recover even when SDK emits no more reconnect events");
 clockNow=21001;loop();assert(WiFi.reconnects==1);
 clockNow=41000;loop();assert(WiFi.reconnects==2);
 WiFi.state=WL_CONNECTED;clockNow=41001;loop();
 assert(SinricPro.handles==1);assert(WiFi.reconnects==2);
 WiFi.state=0;clockNow=42000;loop();
 assert(WiFi.reconnects==2 && "do not interrupt SDK's immediate auto-reconnect");
 clockNow=61999;loop();assert(WiFi.reconnects==2);
 clockNow=62000;loop();assert(WiFi.reconnects==3);
 WiFi.state=WL_CONNECTED;clockNow=62001;loop();
 assert(SinricPro.handles==2);assert(WiFi.reconnects==3);
 WiFi.state=0;WiFi.acceptReconnect=false;clockNow=63000;loop();
 clockNow=83000;loop();assert(WiFi.reconnects==4);
 clockNow=83001;loop();assert(WiFi.reconnects==4 && "failed calls must also be rate-limited");
 puts("PASS: actual main loop gates offline cloud and recovers stalled Wi-Fi without repeated begin/erase");
}
'''
with tempfile.TemporaryDirectory() as temp:
    cpp = Path(temp) / 'loop.cpp'
    binary = Path(temp) / 'test_loop'
    cpp.write_text(prefix + loop + suffix)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation',
                    '-I' + str(root / 'firmware/include'), str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
