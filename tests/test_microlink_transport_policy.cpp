#include <cassert>
#include <cstdio>
#include "../components/microlink/src/ml_transport_policy.h"
int main(){
    assert(!ml_tls_admit(1000,0,100,100));
    assert(ml_tls_admit(60000,0,100,100)); // Bounded low-memory deferral, not starvation.
    assert(ml_tls_admit(5000,0,100000,50000));
    assert(!ml_tls_admit(1000,0,100000,50000));
    assert(ml_tls_admit(2000,UINT32_MAX-4000,100000,50000));
    assert(ml_tls_service_window(1500,1000));
    assert(ml_tls_service_window(6000,1000)); // HTTP upgrade keeps receiving lines.
    assert(!ml_tls_service_window(7000,1000)); // Must abort/release, not starve DERP.
    assert(ml_tls_service_window(100,UINT32_MAX-100));
    assert(ml_retry_delay(0)==2000 && ml_retry_delay(20)==30000);
    assert(ml_disco_budget(0)>0 && ml_disco_budget(2)==0);
    assert(ml_reply_via_derp(true) && !ml_reply_via_derp(false));
    assert(!ml_remote_available(false,true,10,11));
    assert(!ml_remote_available(true,true,0,100));
    assert(ml_remote_available(true,true,10,11));
    assert(!ml_remote_available(true,true,10,180011));
    puts("PASS: bounded TLS admission/backoff, discovery budget, ingress replies, availability");
}
