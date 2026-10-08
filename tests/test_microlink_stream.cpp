#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include "../components/microlink/src/ml_coord_workspace.h"
#include "../components/microlink/src/ml_h2_stream_data.h"
#include "../components/microlink/src/ml_stream.h"

// Removing retained header/payload state loses these frames; treating EOF as
// idle or ignoring the length bound must fail these assertions.
static int consume(void *context,uint32_t stream,const uint8_t *data,size_t size) {
    assert(stream==5);static_cast<std::string*>(context)->append(reinterpret_cast<const char*>(data),size);return 0;
}
static void* fail_allocate(size_t) {return nullptr;}
int main() {
    ml_frame_reader r{};
    const unsigned char derp[] = {3,0,0,0,3,'a','b','c'};
    for (size_t i=0;i<sizeof derp;++i) {
        size_t used=0;
        int rc=ml_frame_feed(&r,ML_FRAME_DERP,derp+i,1,&used,64);
        assert(used==1);
        assert(rc==(i==7?1:0));
    }
    assert(r.length==3 && std::memcmp(r.payload,"abc",3)==0);
    ml_frame_reset(&r);
    const unsigned char huge[]={3,0,1,0,0}; size_t used=0;
    assert(ml_frame_feed(&r,ML_FRAME_DERP,huge,5,&used,64)==-1);
    ml_frame_reset(&r);
    const unsigned char empty[]={6,0,0,0,0};
    assert(ml_frame_feed(&r,ML_FRAME_DERP,empty,5,&used,64)==1);
    ml_frame_reset(&r);
    const unsigned char map[]={2,0,0,0,'{','}',2,0,0,0,'{','}'};
    assert(ml_frame_feed(&r,ML_FRAME_MAP,map,sizeof map,&used,64)==1);
    assert(used==6 && r.length==2 && std::memcmp(r.payload,"{}",2)==0);
    ml_frame_reset(&r);
    assert(ml_frame_feed(&r,ML_FRAME_MAP,map+6,6,&used,64)==1);
    ml_frame_reset(&r);
    const unsigned char h2[]={0,0,2,0,0,0,0,0,5,'{','}'};
    for(size_t split=1;split<sizeof h2;++split){
        assert(ml_frame_feed(&r,ML_FRAME_H2,h2,split,&used,64)==0);
        assert(ml_frame_feed(&r,ML_FRAME_H2,h2+split,sizeof h2-split,&used,64)==1);
        assert(r.header[8]==5 && std::memcmp(r.payload,"{}",2)==0);
        ml_frame_reset(&r);
    }
    // A full 22KB map must assemble with caller-owned storage, without a
    // second heap copy. Reset must not free that storage, including failures.
    unsigned char storage[32769]{};
    unsigned char body[22942]; std::memset(body, 'x', sizeof body);
    const unsigned char map_header[] = {0x9e,0x59,0,0};
    assert(ml_frame_feed_buffer(&r,ML_FRAME_MAP,map_header,4,&used,32768,storage,sizeof storage)==0);
    assert(r.payload==storage && r.payload_borrowed);
    for(size_t pos=0; pos<sizeof body;) {
        size_t size=sizeof body-pos; if(size>1024) size=1024;
        int rc=ml_frame_feed_buffer(&r,ML_FRAME_MAP,body+pos,size,&used,32768,storage,sizeof storage);
        assert(used==size); pos+=used;
        assert(rc==(pos==sizeof body?1:0));
    }
    assert(r.length==sizeof body && storage[sizeof body]==0);
    ml_frame_reset(&r);
    storage[0]=42; assert(storage[0]==42 && !r.payload);
    assert(ml_frame_feed_buffer(&r,ML_FRAME_MAP,map_header,4,&used,32768,storage,22942)==-1);
    ml_frame_reset(&r);
    const unsigned char max_header[]={0,128,0,0};
    assert(ml_frame_feed_buffer(&r,ML_FRAME_MAP,max_header,4,&used,32768,storage,sizeof storage)==0);
    assert(r.length==32768 && storage[32768]==0);
    ml_frame_reset(&r);
    ml_coord_workspace lease{};
    assert(!ml_coord_workspace_acquire(&lease,32769,fail_allocate));
    assert(!lease.storage&&!lease.capacity);
    ml_coord_workspace_release(&lease);
    assert(ml_coord_workspace_acquire(&lease,32769,std::malloc));
    auto *first=lease.storage;
    assert(ml_coord_workspace_acquire(&lease,32769,fail_allocate)&&lease.storage==first);
    assert(!ml_coord_workspace_acquire(&lease,32770,std::malloc));
    ml_coord_workspace_release(&lease);ml_coord_workspace_release(&lease);
    assert(!lease.storage&&!lease.capacity);
    // DATA bodies never allocate a retained H2 payload, even at 32KB.
    std::vector<uint8_t> data(9+32768,'d');
    const uint8_t data_header[]={0,128,0,0,0,0,0,0,5};
    std::memcpy(data.data(),data_header,9);std::string delivered;
    for(size_t pos=0;pos<data.size();) {
        size_t size=data.size()-pos;if(size>317)size=317;
        int rc=ml_h2_feed(&r,data.data()+pos,size,&used,32768,consume,&delivered);
        pos+=used;assert(!r.payload);
        assert(rc==(pos==data.size()?1:0));
    }
    assert(delivered==std::string(32768,'d'));ml_frame_reset(&r);
    const uint8_t padded[]={0,0,6,0,8,0,0,0,5,2,'a','b','c',0,0};
    for(size_t split=1;split<sizeof padded;++split) {
        delivered.clear();
        assert(ml_h2_feed(&r,padded,split,&used,32768,consume,&delivered)==0);
        assert(ml_h2_feed(&r,padded+split,sizeof padded-split,&used,32768,consume,&delivered)==1);
        assert(delivered=="abc"&&!r.payload);ml_frame_reset(&r);
    }
    auto invalid=std::vector<uint8_t>(padded,padded+sizeof padded);invalid[9]=6;
    assert(ml_h2_feed(&r,invalid.data(),invalid.size(),&used,32768,consume,&delivered)==-1);ml_frame_reset(&r);
    data[4]=1;
    assert(ml_h2_feed(&r,data.data(),data.size(),&used,32768,consume,&delivered)==-1);ml_frame_reset(&r);
    data[4]=0;
    assert(ml_h2_feed(&r,data.data(),data.size(),&used,32767,consume,&delivered)==-1);ml_frame_reset(&r);
    const uint8_t ping[]={0,0,8,6,0,0,0,0,0,0,1,2,3,4,5,6,7};
    assert(ml_h2_feed(&r,ping,sizeof ping,&used,32768,consume,&delivered)==1);
    assert(r.payload&&r.length==8);ml_frame_reset(&r);
    assert(ml_receive_result(0,EAGAIN)==-1);
    assert(ml_receive_result(-1,EAGAIN)==0);
    assert(ml_receive_result(-1,ECONNRESET)==-1);
    assert(ml_receive_result(3,EAGAIN)==3);
    assert(ml_h2_stream_closed(0,1,5));
    assert(ml_h2_stream_closed(3,0,5));
    assert(ml_h2_stream_closed(7,0,0));
    assert(!ml_h2_stream_closed(0,1,7));
    assert(!ml_h2_stream_closed(0,0,5));
    puts("PASS: incremental DERP/H2/Map framing, bounds, EOF and stream closure");
}
