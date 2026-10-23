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
}
