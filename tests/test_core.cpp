#include <cassert>
#include <cstdio>
#include <vector>
#include <random>
#include "../firmware/include/boot_state.hpp"
#include "../uefi/remote-boot/load_option.h"
static std::vector<uint8_t> option(const char* name,const char* file) {
    std::vector<uint8_t> b={1,0,0,0,0,0};
    do { b.push_back(*name);b.push_back(0); } while(*name++);
    size_t start=b.size();b.insert(b.end(),{4,4,0,0});
    do { b.push_back(*file);b.push_back(0); } while(*file++);
    size_t len=b.size()-start;b[start+2]=len&255;b[start+3]=len>>8;
    b.insert(b.end(),{127,255,4,0});len=b.size()-start;b[4]=len&255;b[5]=len>>8;
    b.insert(b.end(),{0x80,0,0xff,1,2});return b;
}
int main() {
    rb::State s; s.count=4;s.entries[0]={1,"Windows Boot Manager",false,false};s.entries[1]={2,"GRUB",false,false};s.entries[2]={3,"UKI",false,false};s.entries[3]={4,"Remote Boot iPXE",true,true};s.defaultTarget=1;s.fallback=2;
    assert(s.selected(0)==1);assert(s.request(4,0)==400);assert(s.request(2,0)==202);
    assert(s.selected(1)==2&&s.selected(2)==2);s.heartbeat(3,1);assert(s.pending==2);assert(s.request(1,4)==409);
    s.heartbeat(5,2);assert(s.pending==-1);assert(s.request(3,6,true)==202);assert(!s.online(45005));
    assert(s.selected(180006)==1);s.behavior=rb::State::Last;assert(s.selected(180006)==3);
    s.behavior=rb::State::Exit;assert(s.selected(180006)==-1);
    s.heartbeatSeen=false;s.request(1,UINT32_MAX-20);assert(s.pendingValid(10));assert(!s.pendingValid(180010));
    s.entries[0].blocked=true;s.reconcile();assert(s.pending==-1&&s.defaultTarget==-1);
    int id;assert(rb::parseId("A0f1",id)&&id==0xa0f1);assert(!rb::parseId("00001",id));assert(!rb::parseId("0;00",id));
    assert(rb::tokenEqual("abc","abc"));assert(!rb::tokenEqual("abcx","abc"));assert(!rb::tokenEqual("ab","abc"));assert(!rb::tokenEqual("",""));
    for(const char* name:{"Windows Boot Manager","shim/GRUB","Limine","systemd-boot","UKI"}) {
        auto b=option(name,"\\EFI\\Vendor\\loader.efi");rb_option o{};
        assert(rb_parse_option(b.data(),b.size(),&o));assert(o.optional_size==5&&o.optional[0]==0x80&&o.optional[2]==0xff);assert(!rb_blocked(&o));
        for(size_t i=0;i<b.size()-5;i++)assert(!rb_parse_option(b.data(),i,&o));
        b[4]=255;b[5]=255;assert(!rb_parse_option(b.data(),b.size(),&o));
    }
    for(const char* path:{"\\EFI\\iPXE\\ipxe.efi","\\EFI\\Other\\RemoteBoot.efi"}) {auto b=option("Renamed",path);rb_option o{};assert(rb_parse_option(b.data(),b.size(),&o));assert(rb_blocked(&o));}
    auto b=option("Remote Boot iPXE","\\EFI\\other.efi");rb_option o{};assert(rb_parse_option(b.data(),b.size(),&o)&&rb_blocked(&o));
    std::mt19937 rng(712);for(int i=0;i<20000;i++){std::vector<uint8_t> bytes(rng()%256);for(auto& v:bytes)v=rng();if(rb_parse_option(bytes.data(),bytes.size(),&o))rb_blocked(&o);}
    puts("PASS: state, wraparound, TTL, heartbeat, auth, catalog, EFI parser, OptionalData, recursion guards, 20000 malformed buffers");
}
