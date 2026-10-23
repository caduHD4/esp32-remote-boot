#!/usr/bin/env python3
"""Exercise the actual discovery tick with bounded syscall and SDK adapters.
No real sockets, hardware, identity keys or credential files are used.
"""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'firmware/src/discovery_runtime.cpp').read_text()
tick=source[source.index('void DiscoveryRuntime::tick('):source.index('void DiscoveryRuntime::status(')]
arduino=r'''
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
using String=std::string;
struct IPAddress {
 uint8_t octets[4]={192,168,50,80};
 uint8_t operator[](int n)const{return octets[n];}
 bool operator!=(const IPAddress& other)const{return memcmp(octets,other.octets,4)!=0;}
};
'''
prefix=r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "discovery_runtime.hpp"
#include "discovery_protocol.h"
static unsigned receiveCalls=0,allocationCalls=0,signCalls=0,sendCalls=0,createCalls=0,closeCalls=0;
static uint32_t now=100;static unsigned heap=64000,largest=32000;
static int queued=0,packetLength=40;static bool addSucceeds=false;
static uint8_t packet[2048]{};
static IPAddress currentIp;
uint32_t millis(){return now;}
struct ESPType{unsigned getFreeHeap(){return heap;}unsigned getMaxAllocHeap(){return largest;}} ESP;
struct WiFiType{IPAddress localIP(){return currentIp;}} WiFi;
struct MDNSType{
 unsigned begins=0,adds=0,ends=0;
 bool begin(const char*){++begins;return true;}
 bool addService(const char*,const char*,int){++adds;return addSucceeds;}
 void end(){++ends;}
} MDNS;
int lwip_socket(int,int,int){++createCalls;return 7;}
int lwip_bind(int,const sockaddr*,socklen_t){return 0;}
int lwip_close(int){++closeCalls;return 0;}
int lwip_fcntl(int,int,int){return 0;}
int lwip_recvfrom(int,void* buffer,size_t capacity,int flags,sockaddr* source,socklen_t* length){
 assert(flags&MSG_DONTWAIT);++receiveCalls;if(!queued)return -1;--queued;
 assert(*length>=sizeof(sockaddr_in));auto* remote=reinterpret_cast<sockaddr_in*>(source);
 *remote={};remote->sin_family=AF_INET;remote->sin_port=htons(49193);remote->sin_addr.s_addr=htonl(0xc0a83202);*length=sizeof(*remote);
 size_t copied=std::min(capacity,size_t(packetLength));memcpy(buffer,packet,copied);return int(copied);
}
int lwip_sendto(int,const void*,size_t n,int flags,const sockaddr*,socklen_t){assert(flags&MSG_DONTWAIT);++sendCalls;return int(n);}
// The old SDK adapter intentionally exposes the documented per-parse allocation.
int WiFiUDP::parsePacket(){++allocationCalls;void* p=std::malloc(1460);std::free(p);++receiveCalls;return queued?packetLength:0;}
int WiFiUDP::read(uint8_t* output,size_t n){--queued;memcpy(output,packet,n);return int(n);}
void WiFiUDP::flush(){if(queued)--queued;}
namespace rb {
void DiscoveryRuntime::prepare(){attempted=true;identityReady=true;memset(identity,0,sizeof identity);memset(identity+4,0x11,16);identity[52]=4;host="synthetic-test";}
bool DiscoveryRuntime::sign(uint8_t*){++signCalls;return true;}
'''
suffix=r'''
}
int main(int argc,char** argv){
 assert(argc==2);int scenario=atoi(argv[1]);rb::DiscoveryRuntime runtime;
 uint8_t nonce[16]={1},id[16];memset(id,0x11,16);rb_discovery_query(packet,nonce,id);
 if(scenario==1){
  runtime.tick(true);receiveCalls=allocationCalls=0;heap=1000;largest=500;queued=3;
  for(int i=0;i<1000;++i){++now;runtime.tick(true);}
  assert(receiveCalls==0&&allocationCalls==0&&signCalls==0&&queued==3);
 }else if(scenario==2){
  addSucceeds=true;runtime.tick(true);receiveCalls=allocationCalls=0;
  for(int i=0;i<1000;++i){++now;runtime.tick(true);} // idle polling owns no heap buffer
  assert(allocationCalls==0&&signCalls==0);
  for(int length:{0,1,39,41,512,2048}){
   packetLength=length;queued=2;unsigned before=receiveCalls;++now;runtime.tick(true);
   assert(receiveCalls==before+1&&queued==1&&allocationCalls==0&&signCalls==0);
   ++now;runtime.tick(true);assert(queued==0&&signCalls==0);
  }
  packetLength=40;packet[5]=1;queued=2;runtime.tick(true);assert(queued==1&&signCalls==0);runtime.tick(true);assert(queued==0&&signCalls==0);packet[5]=0;
  queued=2;++now;runtime.tick(true);assert(queued==1&&signCalls==1&&sendCalls==1);
 }else if(scenario==3){
  runtime.tick(true);assert(MDNS.begins==1&&MDNS.adds==1);
  now=5099;runtime.tick(true);assert(MDNS.begins==1&&MDNS.adds==1);
  now=5100;runtime.tick(true);assert(MDNS.begins==1&&MDNS.adds==2&&MDNS.ends==0);
  addSucceeds=true;now=10100;runtime.tick(true);assert(MDNS.begins==1&&MDNS.adds==3&&MDNS.ends==0);
  now=15100;runtime.tick(true);assert(MDNS.adds==3&&MDNS.begins==1);
  runtime.tick(false);currentIp.octets[3]=81;++now;runtime.tick(true);
  assert(MDNS.begins==1&&MDNS.adds==3&&MDNS.ends==0);
 }
 puts("PASS actual discovery tick scenario");
}
'''
with tempfile.TemporaryDirectory(prefix='rb-discovery-runtime-') as temp:
    base=Path(temp)
    (base/'Arduino.h').write_text(arduino)
    (base/'IPAddress.h').write_text('#include "Arduino.h"\n')
    (base/'ArduinoJson.h').write_text('struct JsonObject{};struct JsonObjectConst{};\n')
    (base/'WiFiUdp.h').write_text('''#pragma once
#include "Arduino.h"
class WiFiUDP{public:void stop(){}int begin(int){return 1;}int parsePacket();int read(uint8_t*,size_t);void flush();IPAddress remoteIP(){return {};}uint16_t remotePort(){return 49193;}bool beginPacket(IPAddress,uint16_t){return true;}void write(uint8_t*,size_t){}void endPacket(){}};
''')
    # Keep the legacy adapter available even after production removes its include.
    cpp=base/'runtime.cpp';cpp.write_text('#include "WiFiUdp.h"\n'+prefix+tick+suffix)
    binary=base/'runtime'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+temp,'-I'+str(root/'firmware/include'),'-I'+str(root/'shared'),str(cpp),'-o',str(binary)],check=True)
    failed=[]
    for scenario in (1,2,3):
        result=subprocess.run([str(binary),str(scenario)])
        if result.returncode:failed.append(scenario)
    assert not failed,f'failed scenarios: {failed}'
print('PASS fixed-buffer receive, early memory admission, one-datagram limit, malformed traffic and mDNS service retry/reconnect')
