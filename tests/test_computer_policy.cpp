#include <cassert>
#include <cstdio>
#include <cstring>

#include "../firmware/include/computer_policy.hpp"

int main() {
    char mac[18];
    assert(rb::normalizeComputerMac("aa:bb:cc:dd:ee:fe", mac));
    assert(std::strcmp(mac, "AA:BB:CC:DD:EE:FE") == 0);
    assert(rb::normalizeComputerMac("AABBCCDDEEFE", mac));
    assert(rb::normalizeComputerMac("aa-bb-cc-dd-ee-fe", mac));
    assert(!rb::normalizeComputerMac("AB:BB:CC:DD:EE:FF", mac)); // multicast
    assert(!rb::normalizeComputerMac("00:00:00:00:00:00", mac));
    assert(!rb::normalizeComputerMac("AA::BB:CC:DD:EE:FE", mac));

    assert(rb::validComputerId("pc-a1b2c3"));
    assert(rb::validComputerId("desktop_2"));
    assert(!rb::validComputerId(""));
    assert(!rb::validComputerId("pc com espaco"));

    rb::ComputerIdentity one[] = {
        {"pc-one", "02:00:00:00:00:01", ""}
    };
    assert(rb::computerIdentitiesValid(one, 1));
    assert(rb::resolveAdminComputer(one, 1, nullptr) == 0);
    assert(rb::resolveAdminComputer(one, 1, "") == 0);
    assert(rb::resolveAdminComputer(one, 1, "pc-one") == 0);
    assert(rb::resolveAdminComputer(one, 1, "missing") == rb::ComputerNotFound);
    assert(rb::computerByMac(one, 1, "02-00-00-00-00-01") == 0);
    assert(rb::computerByAgentToken(one, 1, "") == rb::ComputerNotFound);

    rb::ComputerIdentity four[] = {
        {"pc-one",   "02:00:00:00:00:01", "token-one"},
        {"pc-two",   "02:00:00:00:00:02", ""},
        {"pc-three", "02:00:00:00:00:03", "token-three"},
        {"pc-four",  "02:00:00:00:00:04", ""}
    };
    assert(rb::computerIdentitiesValid(four, 4));
    assert(rb::resolveAdminComputer(four, 4, nullptr) == rb::ComputerIdRequired);
    assert(rb::computerById(four, 4, "pc-three") == 2);
    assert(rb::computerByMac(four, 4, "02:00:00:00:00:04") == 3);
    assert(rb::computerByAgentToken(four, 4, "token-one") == 0);
    assert(rb::computerByAgentToken(four, 4, "token-three") == 2);

    rb::ComputerIdentity five[] = {
        four[0], four[1], four[2], four[3],
        {"pc-five", "02:00:00:00:00:05", ""}
    };
    assert(!rb::computerIdentitiesValid(five, 5));

    rb::ComputerIdentity duplicateId[] = {
        four[0], {"pc-one", "02:00:00:00:00:06", "other"}
    };
    rb::ComputerIdentity duplicateMac[] = {
        four[0], {"other", "02-00-00-00-00-01", "other"}
    };
    rb::ComputerIdentity duplicateToken[] = {
        four[0], {"other", "02:00:00:00:00:06", "token-one"}
    };
    assert(!rb::computerIdentitiesValid(duplicateId, 2));
    assert(!rb::computerIdentitiesValid(duplicateMac, 2));
    assert(!rb::computerIdentitiesValid(duplicateToken, 2));

    rb::State state;
    rb::Entry first{1, "Windows", false, false};
    assert(state.append(first));
    assert(!state.append(first));
    for (size_t i = 1; i < rb::MaxEntries; ++i) {
        rb::Entry entry{static_cast<uint16_t>(i + 1), "", false, false};
        assert(state.append(entry));
    }
    rb::Entry overflow{99, "overflow", false, false};
    assert(!state.append(overflow));

    puts("PASS: multi-computer identity, selection, lookup and catalog policies");
}
