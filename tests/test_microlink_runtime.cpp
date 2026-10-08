#include <cassert>
#include <cstring>
#include "../firmware/include/microlink_lifecycle.hpp"

int main() {
    rb::MicrolinkLifecycle lifecycle;
    const rb::MicrolinkStartContext offline{true, true, false, false, false};
    assert(!lifecycle.shouldStart(offline));
    assert(std::strcmp(lifecycle.state(), "wifi_offline") == 0);

    const rb::MicrolinkStartContext ready{true, true, false, false, true};
    assert(lifecycle.shouldStart(ready));
    assert(!lifecycle.shouldStart(ready));
    assert(std::strcmp(lifecycle.state(), "starting") == 0);

    lifecycle.recordStartResult(false);
    assert(std::strcmp(lifecycle.state(), "error") == 0);
    assert(!lifecycle.shouldStart(ready));
    lifecycle.resetAfterWiFiLoss();
    assert(lifecycle.shouldStart(ready));

    rb::MicrolinkLifecycle disabled;
    const rb::MicrolinkStartContext notBuilt{false, true, false, false, true};
    assert(!disabled.shouldStart(notBuilt));
    assert(std::strcmp(disabled.state(), "disabled") == 0);
}
