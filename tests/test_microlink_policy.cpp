#include <cassert>
#include "../firmware/include/microlink_policy.hpp"

int main() {
    using rb::MicrolinkDecision;
    assert(rb::microlinkDecision(false, true, false, false, true) == MicrolinkDecision::Disabled);
    assert(rb::microlinkDecision(true, false, false, false, true) == MicrolinkDecision::NotConfigured);
    assert(rb::microlinkDecision(true, true, true, false, true) == MicrolinkDecision::ConfigLocked);
    assert(rb::microlinkDecision(true, true, false, true, true) == MicrolinkDecision::SetupMode);
    assert(rb::microlinkDecision(true, true, false, false, false) == MicrolinkDecision::WifiOffline);
    assert(rb::microlinkDecision(true, true, false, false, true) == MicrolinkDecision::Ready);
}
