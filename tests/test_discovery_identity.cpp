#include <cassert>
#include <cstdio>
#include <cstring>
#include <openssl/sha.h>
#include "stubs/Preferences.h"
#include "../firmware/include/discovery_identity_store.hpp"
static unsigned generations;
static bool digest(const uint8_t* b,size_t n,uint8_t* out){return SHA256(b,n,out)!=nullptr;}
static bool generate(uint8_t* data){++generations;for(size_t i=4;i<117;++i)data[i]=uint8_t(i);data[52]=4;return true;}
static bool validate(const uint8_t* data){return data[52]==4;}
int main(){
 rb::DiscoveryServiceRetry retry;assert(retry.due(100));assert(!retry.due(101));assert(!retry.due(5099));assert(retry.due(5100));retry.reset();assert(retry.due(5101));
 rb::DiscoveryBudget budget;assert(!budget.take(0,23999,12000));assert(!budget.take(0,24000,11999));
 for(int i=0;i<4;++i){assert(budget.take(999,24000,12000));}assert(!budget.take(1000,24000,12000));assert(!budget.take(1998,24000,12000));assert(budget.take(1999,24000,12000));
 Preferences p;uint8_t b[149]{};assert(rb::prepareDiscoveryIdentity(p,b,generate,validate,digest));assert(generations==1&&p.writes==1);
 assert(rb::prepareDiscoveryIdentity(p,b,generate,validate,digest));assert(generations==1&&p.writes==1);
 p.data["identity"][20]^=1;assert(!rb::prepareDiscoveryIdentity(p,b,generate,validate,digest));assert(generations==1&&p.writes==1);
 p.data["identity"].resize(3);assert(!rb::prepareDiscoveryIdentity(p,b,generate,validate,digest));assert(generations==1&&p.writes==1);
 Preferences failed;failed.corruptReadback=true;assert(!rb::prepareDiscoveryIdentity(failed,b,generate,validate,digest));assert(generations==2&&failed.writes==1);
 assert(!rb::prepareDiscoveryIdentity(failed,b,generate,validate,digest));assert(generations==2&&failed.writes==1);
 puts("PASS identity first-create, no repeated writes, corruption/short-read/write verification fail closed without key rotation");
}
