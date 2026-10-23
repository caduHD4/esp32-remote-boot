#include "discovery_runtime.hpp"
#include "../../shared/discovery_protocol.h"
#include <Preferences.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <lwip/sockets.h>
#include <errno.h>
#include <esp_system.h>
#include <mbedtls/ecp.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/sha256.h>
#include <mbedtls/version.h>
namespace {
int randomBytes(void*,unsigned char* bytes,size_t n){esp_fill_random(bytes,n);return 0;}
bool digest(const uint8_t* bytes,size_t n,uint8_t* output){
#if MBEDTLS_VERSION_MAJOR >= 3
    return mbedtls_sha256(bytes,n,output,0)==0;
#else
    return mbedtls_sha256_ret(bytes,n,output,0)==0;
#endif
}
String hex(const uint8_t* data,size_t length){const char chars[]="0123456789abcdef";String out;out.reserve(length*2);for(size_t i=0;i<length;++i){out+=chars[data[i]>>4];out+=chars[data[i]&15];}return out;}
struct Key {
    mbedtls_ecp_group group;mbedtls_mpi scalar;mbedtls_ecp_point point;
    Key(){mbedtls_ecp_group_init(&group);mbedtls_mpi_init(&scalar);mbedtls_ecp_point_init(&point);}
    ~Key(){mbedtls_ecp_point_free(&point);mbedtls_mpi_free(&scalar);mbedtls_ecp_group_free(&group);}
    bool load(){return mbedtls_ecp_group_load(&group,MBEDTLS_ECP_DP_SECP256R1)==0;}
};
bool generate(uint8_t* data){
    Key key;size_t length=0;
    esp_fill_random(data+4,16);
    return !rb_discovery_zero(data+4,16)&&key.load()&&mbedtls_ecp_gen_keypair(&key.group,&key.scalar,&key.point,randomBytes,nullptr)==0&&
        mbedtls_mpi_write_binary(&key.scalar,data+20,32)==0&&mbedtls_ecp_point_write_binary(&key.group,&key.point,MBEDTLS_ECP_PF_UNCOMPRESSED,&length,data+52,65)==0&&length==65;
}
bool validate(const uint8_t* data){
    if(rb_discovery_zero(data+4,16)||data[52]!=4)return false;
    Key key;mbedtls_ecp_point derived;mbedtls_ecp_point_init(&derived);
    bool okay=key.load()&&mbedtls_mpi_read_binary(&key.scalar,data+20,32)==0&&mbedtls_ecp_check_privkey(&key.group,&key.scalar)==0&&
        mbedtls_ecp_point_read_binary(&key.group,&key.point,data+52,65)==0&&mbedtls_ecp_check_pubkey(&key.group,&key.point)==0&&
        mbedtls_ecp_mul(&key.group,&derived,&key.scalar,&key.group.G,randomBytes,nullptr)==0&&mbedtls_ecp_point_cmp(&derived,&key.point)==0;
    mbedtls_ecp_point_free(&derived);return okay;
}
}
namespace rb {
void DiscoveryRuntime::prepare(){
    if(attempted)return;attempted=true;
    Preferences storage;
    if(!storage.begin("rb-discovery",false)){state="storage_unavailable";return;}
    identityReady=prepareDiscoveryIdentity(storage,identity,generate,validate,digest);storage.end();
    if(!identityReady){state="identity_error";volatile uint8_t* p=identity;for(size_t i=0;i<sizeof identity;++i)p[i]=0;Serial.println("Discovery identity unavailable; normal mode preserved");return;}
    uint8_t hash[32];if(!digest(identity+52,65,hash)){identityReady=false;state="identity_error";return;}
    deviceId=hex(identity+4,16);publicKey=hex(identity+52,65);fingerprint=hex(hash,32);host="remote-boot-"+deviceId.substring(20);
    state="ready";Serial.println("Discovery device ID: "+deviceId);Serial.println("Discovery public fingerprint: "+fingerprint);
}
bool DiscoveryRuntime::sign(uint8_t* reply){
    Key key;mbedtls_mpi r,s;mbedtls_mpi_init(&r);mbedtls_mpi_init(&s);uint8_t hash[32];
    bool okay=key.load()&&mbedtls_mpi_read_binary(&key.scalar,identity+20,32)==0&&digest(reply,RB_DISCOVERY_SIGNED_SIZE,hash)&&
        mbedtls_ecdsa_sign(&key.group,&r,&s,&key.scalar,hash,sizeof hash,randomBytes,nullptr)==0&&
        mbedtls_mpi_write_binary(&r,reply+113,32)==0&&mbedtls_mpi_write_binary(&s,reply+145,32)==0;
    mbedtls_mpi_free(&r);mbedtls_mpi_free(&s);return okay;
}
void DiscoveryRuntime::tick(bool connected){
    if(!connected){
        if(socketFd>=0){lwip_close(socketFd);socketFd=-1;}
        listening=false;services.reset();return;
    }
    if(!attempted){
        if(ESP.getFreeHeap()<24000||ESP.getMaxAllocHeap()<12000)return;
        prepare();
    }
    if(!identityReady)return;
    IPAddress ip=WiFi.localIP();uint8_t address[4]={ip[0],ip[1],ip[2],ip[3]};if(!rb_discovery_valid_ipv4(address))return;
    if(ip!=activeIp){
        if(socketFd>=0){lwip_close(socketFd);socketFd=-1;}
        listening=false;activeIp=ip;services.reset();
    }
    // Admission happens before receive/service work, so recovery cannot allocate under pressure.
    if(ESP.getFreeHeap()<24000||ESP.getMaxAllocHeap()<12000)return;
    if((!listening||!mdnsReady)&&services.due(millis())){
        if(!listening){
            int fd=lwip_socket(AF_INET,SOCK_DGRAM,0);
            if(fd>=0){
                int broadcast=1;lwip_setsockopt(fd,SOL_SOCKET,SO_BROADCAST,&broadcast,sizeof broadcast);
                sockaddr_in local{};local.sin_family=AF_INET;local.sin_port=htons(RB_DISCOVERY_PORT);local.sin_addr.s_addr=htonl(INADDR_ANY);
                if(lwip_bind(fd,reinterpret_cast<const sockaddr*>(&local),sizeof local)==0){socketFd=fd;listening=true;Serial.printf("Discovery UDP ready heap=%u largest=%u\n",ESP.getFreeHeap(),ESP.getMaxAllocHeap());}
                else {Serial.printf("Discovery bind failed errno=%d\n",errno);lwip_close(fd);}
            } else Serial.printf("Discovery socket failed errno=%d heap=%u largest=%u\n",errno,ESP.getFreeHeap(),ESP.getMaxAllocHeap());
        }
        // Keep the responder alive; only a missing service registration is retried.
        if(!mdnsInitialized){mdnsInitialized=MDNS.begin(host.c_str());if(!mdnsInitialized)Serial.println("Discovery mDNS begin failed");}
        if(mdnsInitialized&&!mdnsReady){mdnsReady=MDNS.addService("remote-boot","udp",RB_DISCOVERY_PORT);if(!mdnsReady)Serial.println("Discovery mDNS service failed");}
    }
    if(!listening)return;
    // One nonblocking datagram per pass into a fixed stack buffer; no per-poll heap use.
    uint8_t datagram[RB_DISCOVERY_MAX_DATAGRAM];sockaddr_in remote{};socklen_t remoteLength=sizeof remote;
    int length=lwip_recvfrom(socketFd,datagram,sizeof datagram,MSG_DONTWAIT,reinterpret_cast<sockaddr*>(&remote),&remoteLength);
    if(length!=int(RB_DISCOVERY_QUERY_SIZE)){if(length>0)Serial.printf("Discovery ignored len=%d\n",length);return;}
    uint8_t source[4];memcpy(source,&remote.sin_addr.s_addr,sizeof source);uint16_t port=ntohs(remote.sin_port);
    if(!rb_discovery_valid_ipv4(source)||!port||!rb_discovery_valid_query(datagram,RB_DISCOVERY_QUERY_SIZE,identity+4))return;
    if(!budget.take(millis(),ESP.getFreeHeap(),ESP.getMaxAllocHeap()))return;
    uint8_t reply[RB_DISCOVERY_REPLY_SIZE];rb_discovery_reply(reply,datagram+8,identity+4,address,identity+52);
    if(!sign(reply))return;
    int sent=lwip_sendto(socketFd,reply,sizeof reply,MSG_DONTWAIT,reinterpret_cast<const sockaddr*>(&remote),remoteLength);
    if(sent<0)Serial.printf("Discovery send failed errno=%d\n",errno);
}
void DiscoveryRuntime::status(JsonObject out) const {
    out["ready"]=identityReady;out["status"]=state;out["discovery_version"]=1;
    if(identityReady){out["device_id"]=deviceId;out["public_key"]=publicKey;out["fingerprint"]=fingerprint;out["hostname"]=host+".local";out["last_ipv4"]=WiFi.localIP().toString();out["http_port"]=80;out["ws_port"]=81;}
}
bool DiscoveryRuntime::profile(JsonObject out,JsonObjectConst pc) const {
    if(!identityReady||WiFi.status()!=WL_CONNECTED||pc.isNull())return false;
    out["profile_version"]=1;out["protocol"]=2;out["discovery_version"]=1;out["pc_id"]=pc["pc_id"];
    out["blind_mode"]=pc["blind_mode"]|false;out["mode_revision"]=pc["mode_revision"]|uint32_t(1);out["device_id"]=deviceId;out["public_key"]=publicKey;out["fingerprint"]=fingerprint;
    out["last_ipv4"]=WiFi.localIP().toString();out["http_port"]=80;out["ws_port"]=81;out["hostname"]=host+".local";return true;
}
}
