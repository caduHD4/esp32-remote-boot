#include <cassert>
#include <cstdio>
#include "../firmware/include/sinric_policy.hpp"

int main() {
    using rb::SinricReadiness;
    assert(rb::sinricReadiness(false,nullptr,nullptr)==SinricReadiness::Disabled);
    assert(rb::sinricReadiness(false,"short","short")==SinricReadiness::Disabled);
    assert(rb::sinricReadiness(true,nullptr,"abcdefghij")==SinricReadiness::MissingCredentials);
    assert(rb::sinricReadiness(true,"abcdefghij",nullptr)==SinricReadiness::MissingCredentials);
    assert(rb::sinricReadiness(true,"123456789","abcdefghij")==SinricReadiness::MissingCredentials);
    assert(rb::sinricReadiness(true,"abcdefghij","123456789")==SinricReadiness::MissingCredentials);
    assert(rb::sinricReadiness(true,"abcdefghij","0123456789")==SinricReadiness::Ready);

    const char* ids[]={"0123456789abcdef01234567","aaaaaaaaaaaaaaaaaaaaaaaa"};
    assert(rb::sinricSlotForDevice(ids,2,"0123456789ABCDEF01234567")==0);
    assert(rb::sinricSlotForDevice(ids,2,"AAAAAAAAAAAAAAAAAAAAAAAA")==1);
    assert(rb::sinricSlotForDevice(ids,2,"bbbbbbbbbbbbbbbbbbbbbbbb")==-1);
    assert(rb::sinricSlotForDevice(ids,1,"0123456789abcdef0123456")==-1);
    const char* reordered[]={"aaaaaaaaaaaaaaaaaaaaaaaa","0123456789abcdef01234567"};
    assert(rb::sinricSlotForDevice(reordered,2,"0123456789ABCDEF01234567")==1);

    char normalized[25];
    assert(rb::normalizeSinricDeviceId("ABCDEF0123456789ABCDEF01",normalized));
    assert(std::strcmp(normalized,"abcdef0123456789abcdef01")==0);
    assert(!rb::normalizeSinricDeviceId("xyz",normalized));

    assert(!rb::sinricResetDue(0,1000));
    assert(!rb::sinricResetDue(2000,1999));
    assert(rb::sinricResetDue(2000,2000));
    assert(rb::nextSinricReset(2000,true)==0);
    assert(rb::nextSinricReset(2000,false)==3000);
    uint32_t resetAt=0;
    assert(!rb::acceptSinricDispatch(409,2000,resetAt));
    assert(resetAt==0);
    assert(rb::acceptSinricDispatch(202,2000,resetAt));
    assert(resetAt==3000);
    puts("PASS: Sinric readiness policy");
}
