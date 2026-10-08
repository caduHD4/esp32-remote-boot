#include <cassert>
#include <string>
#include <cstdlib>
#include <cstdio>
#include "../firmware/include/config_store.hpp"
#include "../firmware/include/document_transaction.hpp"
struct TrackingAllocator : ArduinoJson::Allocator {
 size_t calls=0;
 void* allocate(size_t size) override {++calls;return std::malloc(size);}
 void deallocate(void* value) override {std::free(value);}
 void* reallocate(void* value,size_t size) override {++calls;return std::realloc(value,size);}
};
static rb::ConfigStore* persistent;
static unsigned rollbacks;
static void rollback(JsonDocument& live) {++rollbacks;assert(persistent->load(live));}
int main() {
 TrackingAllocator allocator;JsonDocument live(&allocator);Preferences nvs;rb::ConfigStore store(nvs);persistent=&store;
 JsonObject object=live["pc"].to<JsonObject>();object["payload"]=std::string(18000,'x');object["count"]=96;
 assert(store.save(live));
 const char* original=object["payload"].as<const char*>();size_t before=allocator.calls;
 {
  rb::DocumentTransaction<JsonDocument> next(live,rollback);
  assert(next.take());assert(live.isNull());assert(allocator.calls==before);
  assert(next["pc"]["payload"].as<const char*>()==original);
  // JsonObject aliases bind to the owning document; reacquire after take().
  JsonObject owned=next["pc"].as<JsonObject>();assert(owned["payload"].as<const char*>()==original);
  owned["count"]=97;assert(next["pc"]["count"]==97);
  assert(!next.take());assert(store.save(next));
  next.accept();assert(allocator.calls==before);
  assert(live["pc"]["payload"].as<const char*>()==original);
  assert(live["pc"]["count"]==97);
 }
 assert(rollbacks==0&&live["pc"]["count"]==97);
 {
  rb::DocumentTransaction<JsonDocument> next(live,rollback);assert(next.take());
  next["pc"]["count"]=98;nvs.failSelector=true;assert(!store.save(next));nvs.failSelector=false;
 }
 assert(rollbacks==1&&live["pc"]["count"]==97);
 assert(live["pc"]["payload"].as<std::string>()==std::string(18000,'x'));
 assert(nvs.largestRead<=512&&nvs.largestWrite<=512);
 // Aliases into the failed document are intentionally invalid after rollback;
 // acquire them again from live after the durable snapshot has been loaded.
 JsonObject reloaded=live["pc"].as<JsonObject>();assert(reloaded["count"]==97);
 {
  JsonDocument input;JsonArray items=input["systems"].to<JsonArray>();
  JsonObject item=items.add<JsonObject>();item["id"]=std::string("0001");item["name"]=std::string(63,'n');item["hidden"]=true;item["blocked"]=false;
  rb::DocumentTransaction<JsonDocument> next(live,rollback);assert(next.take());
  JsonArray list=next["pc"]["systems"].to<JsonArray>();
  for(JsonObject source:items) {
   JsonObject dest=list.add<JsonObject>();
   for(const char* key:{"id","name","hidden","blocked"})dest[key]=source[key];
  }
  input.clear();
  assert(list[0]["name"].as<std::string>()==std::string(63,'n'));
  assert(list[0]["id"].as<std::string>()=="0001");assert(list[0]["hidden"]==true);
  assert(store.save(next));next.accept();
 }
 assert(live["pc"]["systems"][0]["name"].as<std::string>()==std::string(63,'n'));
 puts("PASS: real ArduinoJson18KB transaction: zero-allocation take/accept, alias identity, derived-document serializer, chunk-store rollback, input field copies survive input.clear");
}
