#pragma once
#include "ml_stream.h"
typedef int (*ml_h2_data_consumer)(void*,uint32_t,const uint8_t*,size_t);
// DATA is delivered in bounded fragments, without retaining a second body.
// Only complete Noise records may be passed here, after authentication.
static inline int ml_h2_feed(ml_frame_reader *r,const uint8_t *data,size_t size,
        size_t *consumed,size_t maximum,ml_h2_data_consumer consume,void *context) {
    *consumed=0;
    while(r->header_used<9&&*consumed<size)r->header[r->header_used++]=data[(*consumed)++];
    if(r->header_used<9)return 0;
    if(r->header[3]!=0) {
        size_t used=0;
        int result=ml_frame_feed(r,ML_FRAME_H2,data+*consumed,size-*consumed,&used,maximum);
        *consumed+=used;return result;
    }
    r->length=((uint32_t)r->header[0]<<16)|((uint32_t)r->header[1]<<8)|r->header[2];
    uint32_t stream=((uint32_t)(r->header[5]&127)<<24)|((uint32_t)r->header[6]<<16)|
        ((uint32_t)r->header[7]<<8)|r->header[8];
    if(r->length>maximum||ml_h2_stream_closed(0,r->header[4],stream))return -1;
    if((r->header[4]&8)&&r->used==0) {
        if(!r->length)return -1;
        if(*consumed==size)return 0;
        r->padding=data[(*consumed)++];++r->used;
        if(r->padding>=r->length)return -1;
    }
    size_t count=r->length-r->used;
    if(count>size-*consumed)count=size-*consumed;
    size_t end=r->length-r->padding;
    size_t content=r->used<end?end-r->used:0;
    if(content>count)content=count;
    if(content&&consume(context,stream,data+*consumed,content)<0)return -1;
    r->used+=count;*consumed+=count;
    return r->used==r->length?1:0;
}
