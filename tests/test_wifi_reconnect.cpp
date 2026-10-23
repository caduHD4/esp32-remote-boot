#include <cassert>
#include <cstdio>
#include "../firmware/include/wifi_reconnect_policy.hpp"

int main() {
    rb::WiFiReconnectPolicy policy;
    policy.reset(1000);
    assert(policy.due(1000));
    policy.recordAttempt(1000);
    assert(policy.attempts() == 1);
    assert(!policy.due(20999));
    assert(policy.due(21000));

    policy.recordAttempt(21000);
    assert(policy.attempts() == 2);
    assert(!policy.due(40999));
    assert(policy.due(41000));

    policy.observe(true, 41001);
    assert(policy.connected());
    assert(policy.attempts() == 0);
    assert(!policy.due(10000));

    policy.observe(false, 50001);
    assert(!policy.connected());
    assert(!policy.due(50001));
    assert(!policy.due(70000));
    assert(policy.due(70001));

    policy.reset(0xfffffff0u);
    policy.recordAttempt(0xfffffff0u);
    assert(!policy.due(0x20u));
    assert(policy.due(0x4e10u));
    puts("PASS: Wi-Fi reconnect policy");
}
