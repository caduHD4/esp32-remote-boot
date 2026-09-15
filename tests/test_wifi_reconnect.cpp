#include <cassert>
#include <cstdio>
#include "../firmware/include/wifi_reconnect_policy.hpp"

int main() {
    rb::WiFiReconnectPolicy policy;
    policy.reset(1000);
    assert(policy.due(1000));
    policy.recordAttempt(1000);
    assert(policy.attempts() == 1);
    assert(!policy.due(1999));
    assert(policy.due(2000));

    policy.recordAttempt(2000);
    assert(policy.attempts() == 2);
    assert(!policy.due(3999));
    assert(policy.due(4000));

    policy.observe(true, 4001);
    assert(policy.connected());
    assert(policy.attempts() == 0);
    assert(!policy.due(10000));

    policy.observe(false, 10001);
    assert(!policy.connected());
    assert(policy.due(10001));
    puts("PASS: Wi-Fi reconnect policy");
}
