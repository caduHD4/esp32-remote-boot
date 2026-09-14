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
    puts("PASS: Sinric readiness policy");
}
