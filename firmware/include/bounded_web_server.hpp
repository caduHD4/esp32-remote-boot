#pragma once
#include <WebServer.h>
#include <lwip/sockets.h>
#include <errno.h>
#include "http_asset.hpp"
namespace rb {
class BoundedWebServer:public WebServer {
public:
    using WebServer::WebServer;
    bool sendBoundedAsset(int code,const char* type,const uint8_t* bytes,size_t length) {
        String header;_prepareHeader(header,code,type,length);
        auto& peer=_currentClient;const int fd=peer.fd();const uint32_t started=millis();
        if(fd<0||!header.length()){peer.stop();return false;}
        // The SDK normally sets this on write(). Direct nonblocking sends must
        // set it too: lwIP otherwise allows close() to wait its default20s.
        timeval sendTimeout{1,0};
        if(lwip_setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&sendTimeout,sizeof sendTimeout)!=0){peer.stop();return false;}
        auto write=[fd](const uint8_t* data,size_t n)->int {
            const int result=lwip_send(fd,data,n,MSG_DONTWAIT);
            if(result<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))return 0;
            return result>0?result:-1;
        };
        auto now=[]()->uint32_t{return millis();};auto alive=[&peer]{return bool(peer.connected());};auto yield=[]{delay(1);};
        constexpr uint32_t total=8000,idle=3000;
        bool okay=sendAssetBytes(reinterpret_cast<const uint8_t*>(header.c_str()),header.length(),write,now,alive,yield,total,idle);
        const uint32_t elapsed=uint32_t(millis()-started);
        if(okay)okay=sendAssetBytes(bytes,length,write,now,alive,yield,elapsed<total?total-elapsed:0,idle);
        // USB diagnostics can block for seconds when no serial monitor reads.
        // Keep the response path free of serial writes; failures use the event log.
        if(!okay) {
            #if defined(LWIP_SO_LINGER) && LWIP_SO_LINGER
            linger immediate{1,0};lwip_setsockopt(fd,SOL_SOCKET,SO_LINGER,&immediate,sizeof immediate);
#endif
            peer.stop(); // Abort a short response instead of blocking on queued data.
        }
        return okay;
    }
};
}
