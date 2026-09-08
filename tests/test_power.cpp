#include "../firmware/include/power_command.hpp"
#include <cassert>
#include <limits>
int main() {
    rb::PowerCommand q;
    assert(q.enqueue("a","shutdown","","session-one",100));
    assert(!q.enqueue("b","reboot","0001","session-one",101));
    assert(!q.heartbeat("session-one","wrong",true,102));
    assert(q.active(102));
    assert(q.heartbeat("session-one","a",true,103));
    assert(!q.heartbeat("session-one","a",true,104));
    assert(!q.active(104));
    q.enqueue("b","shutdown","","session-one",200);
    assert(!q.heartbeat("session-two","b",true,201));
    assert(!q.active(201));
    q.enqueue("c","shutdown","","session-two",300);
    assert(!q.heartbeat("session-two","c",false,301));
    assert(!q.active(301));
    q.enqueue("d","shutdown","","session-two",400);
    assert(!q.heartbeat("session-two","d",true,30400));
    assert(!q.active(30400));
    const auto nearWrap=std::numeric_limits<uint32_t>::max()-10;
    q.enqueue("e","shutdown","","session-two",nearWrap);
    assert(q.active(10));
    assert(!q.active(30000));
    // Legacy agents can continue to request reboot with an empty session.
    assert(q.enqueue("f","reboot","0001","",30001));
    assert(q.heartbeat("","f",false,30002));
}
