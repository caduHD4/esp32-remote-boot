#include <cassert>
#include <cstring>
#include <cstdio>
#include "../components/microlink/src/ml_stream.h"

// Removing retained header/payload state loses these frames; treating EOF as
// idle or ignoring the length bound must fail these assertions.
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
