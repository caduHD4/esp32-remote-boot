#include <cassert>
#include <cstdio>
#include <cstring>

#include "../firmware/include/setup_policy.hpp"

int main() {
    assert(rb::validNewCredential("1"));
    assert(rb::validNewCredential("12345678"));
    assert(rb::validNewCredential("a b!@#$%"));
    assert(!rb::validNewCredential(""));
    assert(!rb::validNewCredential("123456789"));
    assert(rb::validNewCredential("", true));
    assert(rb::validNewCredential(nullptr, true));
    assert(!rb::validNewCredential(nullptr));

    using rb::SetupStep;
    for (SetupStep step : {
        SetupStep::Password, SetupStep::Computer, SetupStep::Boot,
        SetupStep::Integrations, SetupStep::Finish, SetupStep::Complete
    }) {
        assert(rb::parseSetupStep(rb::setupStepName(step)) == step);
    }
    assert(rb::parseSetupStep("unknown") == SetupStep::Invalid);
    assert(rb::stepAfterComputer(false) == SetupStep::Integrations);
    assert(rb::stepAfterComputer(true) == SetupStep::Boot);
    assert(rb::validSetupTransition(SetupStep::Password, SetupStep::Computer, false));
    assert(rb::validSetupTransition(SetupStep::Computer, SetupStep::Integrations, false));
    assert(rb::validSetupTransition(SetupStep::Computer, SetupStep::Boot, true));
    assert(!rb::validSetupTransition(SetupStep::Computer, SetupStep::Boot, false));
    assert(rb::validSetupTransition(SetupStep::Boot, SetupStep::Integrations, true));
    assert(rb::validSetupTransition(SetupStep::Integrations, SetupStep::Finish, false));
    assert(rb::validSetupTransition(SetupStep::Finish, SetupStep::Complete, false));
    assert(!rb::validSetupTransition(SetupStep::Complete, SetupStep::Password, false));

    rb::AuthThrottle throttle;
    for (uint8_t i = 0; i < rb::AuthFailureLimit - 1; ++i) {
        assert(throttle.allowed(100));
        throttle.failure(100);
    }
    assert(throttle.allowed(100));
    throttle.failure(100);
    assert(!throttle.allowed(101));
    assert(!throttle.allowed(30099));
    assert(throttle.allowed(30100));
    throttle.refresh(30100);
    assert(throttle.failures == 0);
    throttle.failure(UINT32_MAX - 10);
    throttle.success();
    assert(throttle.allowed(5));

    puts("PASS: guided setup transitions, short credentials and authentication throttle");
}
