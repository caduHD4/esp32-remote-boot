#include <initializer_list>
#include <cassert>
#include <cstdio>
#include <cstring>
#include "../shared/discovery_protocol.h"
int main() {
 uint8_t q[40]={0},nonce[16],id[16],key[65]={4},ip[4]={192,168,1,80};
 for(int i=0;i<16;++i){nonce[i]=i;id[i]=i+16;}
 rb_discovery_query(q,nonce,id);
 const uint8_t prefix[]={82,66,68,49,1,0,0,40};assert(!memcmp(q,prefix,8));
 assert(rb_discovery_valid_query(q,40,id));assert(!rb_discovery_valid_query(q,39,id));assert(!rb_discovery_valid_query(q,41,id));
 q[5]=1;assert(!rb_discovery_valid_query(q,40,id));q[5]=0;q[24]^=1;assert(!rb_discovery_valid_query(q,40,id));
 memset(q+24,0,16);assert(rb_discovery_valid_query(q,40,id));
 uint8_t r[178]={0};rb_discovery_reply(r,nonce,id,ip,key);
 assert(rb_discovery_valid_reply(r,177,nonce,id,key,ip,80,81));
 assert(!rb_discovery_valid_reply(r,178,nonce,id,key,ip,80,81));
 for(int offset:{0,4,5,6,7,8,24,40,44,46,48,64}){r[offset]^=1;assert(!rb_discovery_valid_reply(r,177,nonce,id,key,ip,80,81));r[offset]^=1;}
 uint8_t bad[4]={224,0,0,1};assert(!rb_discovery_valid_ipv4(bad));bad[0]=0;assert(!rb_discovery_valid_ipv4(bad));memset(bad,255,4);assert(!rb_discovery_valid_ipv4(bad));
 puts("PASS discovery canonical framing, binding, lengths, address and port rejection");
}
