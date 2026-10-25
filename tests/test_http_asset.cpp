#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "../firmware/include/http_asset.hpp"

int main() {
    const uint8_t bytes[]{1,2,3,4,5,6,7};
    uint32_t now=0;std::vector<uint8_t> sent;unsigned calls=0;
    auto clock=[&]{return now;};auto alive=[] {return true;};auto yield=[&]{++now;};
    auto partial=[&](const uint8_t* p,size_t n)->int {++calls;if(calls%3==1)return 0;size_t k=n>2?2:n;sent.insert(sent.end(),p,p+k);return int(k);};
    assert(rb::sendAssetBytes(bytes,sizeof bytes,partial,clock,alive,yield,100,20));
    assert(sent==std::vector<uint8_t>(bytes,bytes+sizeof bytes));
    now=0;calls=0;
    auto stalled=[&](const uint8_t*,size_t)->int {++calls;return 0;};
    assert(!rb::sendAssetBytes(bytes,sizeof bytes,stalled,clock,alive,yield,100,20));
    assert(now==20&&calls<=21);
    now=0;sent.clear();
    auto trickle=[&](const uint8_t* p,size_t)->int {sent.push_back(*p);now+=9;return 1;};
    assert(!rb::sendAssetBytes(bytes,sizeof bytes,trickle,clock,alive,yield,25,20));
    assert(sent.size()<sizeof bytes);
    now=0;
    auto late=[&](const uint8_t*,size_t n)->int {now=101;return int(n);};
    assert(!rb::sendAssetBytes(bytes,sizeof bytes,late,clock,alive,yield,100,20));
    now=0;calls=0;
    auto fatal=[&](const uint8_t*,size_t)->int {++calls;return -1;};
    assert(!rb::sendAssetBytes(bytes,sizeof bytes,fatal,clock,alive,yield,100,20));assert(calls==1);
    calls=0;
    assert(!rb::sendAssetBytes(bytes,sizeof bytes,partial,clock,[]{return false;},yield,100,20));assert(calls==0);
    now=0xfffffff0u;calls=0;
    assert(!rb::sendAssetBytes(bytes,sizeof bytes,stalled,clock,alive,yield,100,20));assert(now==4);
    assert(rb::sendAssetBytes(bytes,0,stalled,clock,alive,yield,100,20));
    puts("PASS: bounded HTTP bytes preserve partial writes, retry pressure, absolute/idle deadlines, disconnect, errors and rollover");
    struct Peer {int id=0;bool data=false,open=true,stopped=false;};
    std::vector<Peer> peers(8);
    struct Client {
        Peer* peer=nullptr;
        explicit operator bool() const {return peer!=nullptr;}
        int available() const {return peer->data;}
        bool connected() const {return peer->open;}
        void stop() {peer->stopped=true;peer->open=false;}
    };
    for(size_t i=0;i<peers.size();++i)peers[i].id=int(i);
    std::vector<Client> queue;
    auto accept=[&]{if(queue.empty())return Client{};Client next=queue.front();queue.erase(queue.begin());return next;};
    rb::PendingConnections<Client,3> pending;Client ready;
    queue={Client{&peers[0]},Client{&peers[1]}};pending.admit(accept,0);
    assert(pending.size()==2&&!pending.take(ready,10,5000));
    peers[1].data=true; // A real request behind an idle preconnect is served at once.
    assert(pending.take(ready,10,5000)&&ready.peer->id==1&&pending.size()==1&&!peers[0].stopped);
    queue={Client{&peers[2]},Client{&peers[3]},Client{&peers[4]}};pending.admit(accept,20);
    assert(pending.size()==3&&peers[0].stopped&&!peers[2].stopped); // Full: the oldest idle yields.
    peers[3].open=false;assert(!pending.take(ready,30,5000)&&peers[3].stopped&&pending.size()==2);
    peers[4].data=true;peers[4].open=false; // Request bytes then FIN are still served.
    assert(pending.take(ready,5020,5000)&&ready.peer->id==4&&peers[2].stopped&&pending.size()==0); // Idle deadline.
    for(int i=5;i<8;++i){peers[i].data=true;queue.push_back(Client{&peers[i]});}
    queue.push_back(Client{&peers[0]});peers[0]={0,false,true,false};pending.admit(accept,0xfffffff0u);
    assert(pending.size()==3&&peers[0].stopped); // Never evict a queued request.
    assert(pending.take(ready,4,5000)&&ready.peer->id==5); // Rollover keeps age small.
    puts("PASS: pending HTTP connections serve the first request, evict idle preconnects, expire and survive rollover");
}
