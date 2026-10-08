#include <cassert>
#include "../firmware/include/config_store.hpp"
int main() {
    Preferences prefs;rb::ConfigStore store(prefs);JsonDocument config;
    assert(!store.exists());assert(!store.load(config));
    config.text="{first}";assert(store.save(config));
    config.text="{second}";assert(store.save(config));
    JsonDocument restored;rb::ConfigStore reboot(prefs);assert(reboot.load(restored));assert(restored.text=="{second}");
    prefs.failBlob=true;config.text="{failed blob}";assert(!store.save(config));prefs.failBlob=false;
    rb::ConfigStore afterBlob(prefs);assert(afterBlob.load(restored));assert(restored.text=="{second}");
    prefs.corruptReadback=true;assert(!store.save(config));prefs.corruptReadback=false;
    rb::ConfigStore afterCorrupt(prefs);assert(afterCorrupt.load(restored));assert(restored.text=="{second}");
    prefs.failSelector=true;config.text="{uncommitted}";assert(!store.save(config));prefs.failSelector=false;
    rb::ConfigStore afterSelector(prefs);assert(afterSelector.load(restored));assert(restored.text=="{second}");
    prefs.data["bank0"].back()^=1;
    rb::ConfigStore corruptedActive(prefs);assert(!corruptedActive.load(restored)); // uncommitted bank1 cannot become live
    Preferences initial;rb::ConfigStore initialStore(initial);initial.failSelector=true;assert(!initialStore.save(config));
    rb::ConfigStore initialReboot(initial);assert(!initialReboot.load(restored));
    Preferences fallback;rb::ConfigStore history(fallback);config.text="{old}";assert(history.save(config));config.text="{current}";assert(history.save(config));
    fallback.data["bank0"].back()^=1;rb::ConfigStore fallbackReboot(fallback);assert(fallbackReboot.load(restored));assert(restored.text=="{old}");
    config.text=std::string(20001,'x');assert(!history.save(config));
}
