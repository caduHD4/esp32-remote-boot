#ifndef RB_LOAD_OPTION_H
#define RB_LOAD_OPTION_H
#include <stddef.h>
#include <stdint.h>
typedef struct { const uint8_t *description,*path,*optional; size_t description_size,path_size,optional_size; uint32_t attributes; } rb_option;
static inline uint16_t rb_u16(const uint8_t *p) { return (uint16_t)(p[0]|((uint16_t)p[1]<<8)); }
static inline uint32_t rb_u32(const uint8_t *p) { return rb_u16(p)|((uint32_t)rb_u16(p+2)<<16); }
static inline int rb_path_valid(const uint8_t *p,size_t n) {
    size_t off=0;
    while(n-off>=4) {
        size_t len=rb_u16(p+off+2);
        if(len<4||len>n-off) return 0;
        if(p[off]==0x7f) return p[off+1]==0xff && len==4 && off+4==n;
        if(p[off]==4 && p[off+1]==4 && (len<6 || len%2 || rb_u16(p+off+len-2))) return 0;
        off+=len;
    }
    return 0;
}
static inline int rb_parse_option(const uint8_t *p,size_t n,rb_option *out) {
    if(!p||n<12||n>65536) return 0;
    size_t end=6;
    while(end+2<=n && rb_u16(p+end)) end+=2;
    if(end+2>n) return 0;
    end+=2; size_t path_len=rb_u16(p+4);
    if(path_len<4||path_len>n-end||!rb_path_valid(p+end,path_len)) return 0;
    out->attributes=rb_u32(p); out->description=p+6; out->description_size=end-6;
    out->path=p+end; out->path_size=path_len; out->optional=p+end+path_len; out->optional_size=n-end-path_len;
    return 1;
}
static inline int rb_utf16_contains(const uint8_t *p,size_t n,const char *needle) {
    for(size_t i=0;i+2<=n;i+=2) {
        size_t j=0;
        while(needle[j] && i+2*j+2<=n) {
            unsigned c=rb_u16(p+i+2*j); if(c>='A'&&c<='Z') c+=32;
            if(c!=(unsigned char)needle[j]) break;
            ++j;
        }
        if(!needle[j]) return 1;
    }
    return 0;
}
static inline int rb_blocked(const rb_option *o) {
    if(rb_utf16_contains(o->description,o->description_size,"remote boot")||rb_utf16_contains(o->description,o->description_size,"ipxe")) return 1;
    size_t off=0;
    while(off+4<=o->path_size) {
        size_t len=rb_u16(o->path+off+2);
        if(o->path[off]==4&&o->path[off+1]==4 &&
            (rb_utf16_contains(o->path+off+4,len-4,"ipxe.efi")||rb_utf16_contains(o->path+off+4,len-4,"remoteboot.efi"))) return 1;
        off+=len;
    }
    return 0;
}
#endif
