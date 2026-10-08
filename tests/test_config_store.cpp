#include <cassert>
#include <cstdio>
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
    config.text="{partial}";config.allocationFailed=true;assert(!history.save(config));
    rb::ConfigStore noPartial(fallback);assert(noPartial.load(restored));assert(restored.text=="{old}");

    // Maximum snapshots use only bounded NVS operations, including verification.
    Preferences maximum;rb::ConfigStore maximumStore(maximum);
    config.allocationFailed=false;config.text="{"+std::string(19998,'x')+"}";
    assert(config.text.size()==20000);assert(maximumStore.save(config));
    assert(maximum.largestWrite<=512&&maximum.largestRead<=512);
    assert(maximum.data["bank1"].size()==16);
    assert(maximum.data["b1_39"].size()==32);
    rb::ConfigStore maximumReboot(maximum);assert(maximumReboot.load(restored));assert(restored.text==config.text);
    assert(maximum.largestRead<=512);
    assert(maximumReboot.save(config));
    maximum.data["b0_20"][100]^=1;
    rb::ConfigStore brokenChunk(maximum);assert(brokenChunk.load(restored));assert(restored.text==config.text);
    maximum.shortReadKey="b1_15";
    rb::ConfigStore shortChunk(maximum);assert(!shortChunk.load(restored));

    // Interrupt every body chunk and the header write; the committed bank wins.
    for(size_t failedWrite=1;failedWrite<=4;++failedWrite) {
        Preferences faults;rb::ConfigStore transaction(faults);
        config.text="{old}";assert(transaction.save(config));
        faults.writes=0;faults.failWriteAt=failedWrite;
        config.text="{"+std::string(1498,'y')+"}";
        assert(!transaction.save(config));faults.failWriteAt=0;
        rb::ConfigStore reset(faults);assert(reset.load(restored));assert(restored.text=="{old}");
    }
    Preferences readFailure;rb::ConfigStore readFailureStore(readFailure);
    config.text="{saved}";assert(readFailureStore.save(config));
    readFailure.shortReadKey="b0_00";config.text="{candidate}";assert(!readFailureStore.save(config));
    readFailure.shortReadKey="";
    rb::ConfigStore failedVerification(readFailure);assert(failedVerification.load(restored));assert(restored.text=="{saved}");
    // Surplus chunks left by a larger inactive snapshot are never consumed.
    config.text="{"+std::string(1022,'z')+"}";assert(readFailureStore.save(config));
    config.text="{tiny}";assert(readFailureStore.save(config));assert(readFailureStore.save(config));
    rb::ConfigStore smaller(readFailure);assert(smaller.load(restored));assert(restored.text=="{tiny}");

    // Legacy full-blob snapshots remain readable and migrate on the next save.
    auto legacyBank=[](Preferences& target,unsigned bank,uint32_t gen,const std::string& body) {
        struct Header { uint32_t magic,generation,length,crc; };
        Header h{0x52423343,gen,static_cast<uint32_t>(body.size()),rb::crc32(body.data(),body.size())};
        std::vector<uint8_t> blob(sizeof h+body.size());
        std::memcpy(blob.data(),&h,sizeof h);std::memcpy(blob.data()+sizeof h,body.data(),body.size());
        target.putBytes(bank?"bank1":"bank0",blob.data(),blob.size());
    };
    Preferences legacy;legacyBank(legacy,0,8,"{legacy}");legacy.putULong64("commit",16);
    rb::ConfigStore legacyStore(legacy);assert(legacyStore.load(restored));assert(restored.text=="{legacy}");
    config.text="{migrated}";assert(legacyStore.save(config));
    rb::ConfigStore migrated(legacy);assert(migrated.load(restored));assert(restored.text=="{migrated}");
    legacy.data["b1_00"].back()^=1;
    rb::ConfigStore legacyFallback(legacy);assert(legacyFallback.load(restored));assert(restored.text=="{legacy}");
    Preferences wrap;legacyBank(wrap,0,0xffffffffu,"{last}");wrap.putULong64("commit",uint64_t(0xffffffffu)<<1);
    rb::ConfigStore wrapStore(wrap);assert(wrapStore.load(restored));assert(wrapStore.save(config));
    rb::ConfigStore wrapped(wrap);assert(wrapped.load(restored));assert(restored.text=="{migrated}");
    wrap.data["b1_00"].back()^=1;
    rb::ConfigStore wrappedFallback(wrap);assert(wrappedFallback.load(restored));assert(restored.text=="{last}");
    puts("PASS: actual ConfigStore chunked20KB, bounded512B I/O, interrupted chunks/header/selector, CRC/fallback, legacy migration, overflow and generation wrap");
}
