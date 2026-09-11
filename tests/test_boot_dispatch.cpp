#include <cassert>
#include <cstdio>
#include "../firmware/include/boot_state.hpp"

int main() {
    rb::State state;
    state.count=2;
    state.entries[0]={8,"CachyOS UKI",false,false};
    state.entries[1]={0,"Windows Boot Manager",false,false};
    state.defaultTarget=0;

    assert(state.request(8,1000)==202);
    assert(state.dispatch(1100)==8);
    assert(state.dispatch(1200)==rb::None);

    assert(state.dispatch(61100)==8);

    assert(state.request(8,1300)==202);
    assert(state.dispatch(1400)==8);

    puts("PASS: each accepted request is dispatched only once");
}
