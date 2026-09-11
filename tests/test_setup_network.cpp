#include <cassert>
#include <cstdint>
#include <cstdio>
#include "../firmware/include/setup_network_policy.hpp"

int main() {
    rb::SetupNetworkPolicy policy;

    assert(!policy.shouldProcessDns());
    assert(!policy.shouldStartRecoveryAp(false,1000));
    assert(!policy.shouldStartRecoveryAp(false,61000));
    assert(policy.shouldStartRecoveryAp(false,61001));

    policy.observeWiFi(true,62000);
    assert(!policy.shouldStartRecoveryAp(false,63000));

    policy.accessPointStarted();
    assert(policy.shouldProcessDns());
    assert(!policy.shouldStartRecoveryAp(false,200000));

    puts("PASS: setup DNS and recovery AP state");
}
