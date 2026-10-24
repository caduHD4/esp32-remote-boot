#ifndef RB_DISCOVERY_PROTOCOL_H
#define RB_DISCOVERY_PROTOCOL_H
/* Canonical discovery v1 framing only. Callers MUST verify ECDSA P-256/SHA256
 * over RB_DISCOVERY_SIGNED_SIZE bytes before trusting any reply endpoint. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define RB_DISCOVERY_PORT 49192
#define RB_DISCOVERY_QUERY_SIZE 40
#define RB_DISCOVERY_REPLY_SIZE 177
#define RB_DISCOVERY_SIGNED_SIZE 113
#define RB_DISCOVERY_MAX_DATAGRAM 512
static inline uint16_t rb_discovery_u16(const uint8_t *p) {return (uint16_t)((uint16_t)p[0]<<8|p[1]);}
static inline int rb_discovery_zero(const uint8_t *p,size_t n) {uint8_t v=0;while(n--)v|=*p++;return v==0;}
static inline int rb_discovery_valid_ipv4(const uint8_t *p) {return p&&p[0]!=0&&p[0]!=127&&p[0]<224;}
static inline int rb_discovery_header(const uint8_t *p,size_t n,uint8_t type,size_t exact) {
 return p&&n==exact&&!memcmp(p,"RBD1",4)&&p[4]==type&&p[5]==0&&rb_discovery_u16(p+6)==exact;
}
static inline void rb_discovery_query(uint8_t *out,const uint8_t *nonce,const uint8_t *id) {
 memcpy(out,"RBD1",4);out[4]=1;out[5]=0;out[6]=0;out[7]=40;memcpy(out+8,nonce,16);memcpy(out+24,id,16);
}
static inline int rb_discovery_valid_query(const uint8_t *p,size_t n,const uint8_t *id) {
 return rb_discovery_header(p,n,1,40)&&(rb_discovery_zero(p+24,16)||!memcmp(p+24,id,16));
}
static inline void rb_discovery_reply(uint8_t *out,const uint8_t *nonce,const uint8_t *id,const uint8_t *ip,const uint8_t *key) {
 memcpy(out,"RBD1",4);out[4]=2;out[5]=0;out[6]=0;out[7]=177;memcpy(out+8,nonce,16);memcpy(out+24,id,16);
 memcpy(out+40,ip,4);out[44]=0;out[45]=80;out[46]=0;out[47]=81;memcpy(out+48,key,65);memset(out+113,0,64);
}
/* id/key may be NULL only for public unbound listing; source/nonce required. */
static inline int rb_discovery_valid_reply(const uint8_t *p,size_t n,const uint8_t *nonce,const uint8_t *id,const uint8_t *key,const uint8_t *source,uint16_t http,uint16_t ws) {
 return rb_discovery_header(p,n,2,177)&&nonce&&source&&http&&ws&&
 !memcmp(p+8,nonce,16)&&(!id||!memcmp(p+24,id,16))&&!rb_discovery_zero(p+24,16)&&
 rb_discovery_valid_ipv4(p+40)&&!memcmp(p+40,source,4)&&rb_discovery_u16(p+44)==http&&rb_discovery_u16(p+46)==ws&&p[48]==4&&(!key||!memcmp(p+48,key,65));
}
#endif
