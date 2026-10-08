#include <cassert>
#include "../firmware/include/multi_pc_policy.hpp"
#include "../firmware/include/boot_state.hpp"
int main() {
    using namespace rb;
    assert(validOpaqueId("00112233445566778899aabbccddeeff"));
    assert(!validOpaqueId("00112233445566778899AABBCCDDEEFF"));
    assert(!validOpaqueId("0011"));
    PairingLimits limits;
    assert(!limits.open(0));
    limits.enable(0xfffffff0u);
    assert(limits.open(100));
    assert(!limits.open(300000));
    for(int i=0;i<6;++i) assert(limits.startAllowed(100));
    assert(!limits.startAllowed(100));
    assert(limits.startAllowed(60100));
    for(int i=0;i<5;++i) assert(limits.lookupAllowed(100));
    assert(!limits.lookupAllowed(100));
    assert(!limits.lookupAllowed(60099));
    assert(limits.lookupAllowed(60100));
    assert(crc32("123456789",9)==0xcbf43926u);
    assert(generationNewer(0,0xffffffffu));
    assert(!generationNewer(1,2));
    static_assert(sizeof(PcState)<200,"PC runtime must not duplicate catalog names");
    PcState first,second;first.count=second.count=1;first.entries[0]={1,false,false};second.entries[0]={2,false,false};
    assert(first.request(1,100)==202);assert(first.pending==1&&second.pending==None);
    assert(second.request(1,100)==400);assert(second.request(2,100)==202);
    assert(first.dispatch(110)==1&&second.dispatch(110)==2);
    first.heartbeat(120,1);assert(!first.pendingValid(120)&&second.pendingValid(120));
}
