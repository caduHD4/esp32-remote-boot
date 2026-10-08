#!/usr/bin/env python3
"""Exercise the actual selective delta parser with a tiny cJSON heap budget.
Usage: python tests/test_microlink_delta.py PATH_TO_CJSON_DIRECTORY
"""
import subprocess
import sys
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / "components/microlink/src/ml_coord.c").read_text(encoding="utf-8")
parser = source.split("static int parse_map_delta", 1)[1].split("static int consume_map_data", 1)[0]
strings = source.split("static bool slice_string", 1)[1].split("static ", 1)[0]
cjson = Path(sys.argv[1])
harness = r'''
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <cstddef>
#include <cstdint>
extern "C" {
#include "cJSON.h"
#include "ml_json_scan.h"
}
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
struct microlink_t { uint32_t vpn_ip; };
static size_t live, peak;
union Allocation { size_t size; std::max_align_t align; };
static void *limited_alloc(size_t size) {
    if(live + size > 2048) return nullptr;
    auto *p = (Allocation*)std::malloc(sizeof(Allocation)+size);
    if(!p) return nullptr;
    p->size=size; live+=size; if(live>peak) peak=live;
    return p+1;
}
static void limited_free(void *p) {
    if(p) { auto *h=(Allocation*)p-1; live-=h->size; std::free(h); }
}
static unsigned peers, removed, patches;
static void parse_peers_from_map_response(microlink_t*, cJSON *object) {
    for(const char *field : {"Peers", "PeersChanged", "peers"}) {
        cJSON *p=cJSON_GetObjectItem(object,field);
        if(p) { assert(cJSON_GetArraySize(p)==1); ++peers; break; }
    }
    cJSON *r=cJSON_GetObjectItem(object,"PeersRemoved");
    if(r) { assert(cJSON_GetArraySize(r)==1); ++removed; }
    cJSON *patch=cJSON_GetObjectItem(object,"PeersChangedPatch");
    if(patch) { assert(cJSON_GetObjectItem(patch,"nodekey:abcd")); ++patches; }
}
''' + "static bool slice_string" + strings + "static int parse_map_delta" + parser + r'''
int main() {
    cJSON_Hooks hooks{limited_alloc,limited_free}; cJSON_InitHooks(&hooks);
    microlink_t ml{};
    std::string large = "{\"DERPMap\":{\"Ignored\":\"" + std::string(22942,'x') +
        "\"},\"Node\":{\"Addresses\":[\"100.1.2.3/32\"]},"
        "\"PeersChanged\":[{\"Name\":\"one\"},{\"Name\":\"two\"}],"
        "\"PeersRemoved\":[\"nodekey:abcd\",\"nodekey:ef01\"],"
        "\"PeersChangedPatch\":{\"nodekey:abcd\":{\"DERPRegion\":9}}}";
    assert(parse_map_delta(&ml,{large.data(),large.size()})==0);
    assert(ml.vpn_ip==0x64010203 && peers==2 && removed==2 && patches==1);
    assert(live==0 && peak<=2048);
    assert(parse_map_delta(&ml,{"{}",2})==0);
    assert(parse_map_delta(&ml,{"{broken",7})==-1);
    assert(parse_map_delta(&ml,{"{}trailing",10})==-1);
    const char precedence[]="{\"Peers\":[{}],\"PeersChanged\":[{},{}]}";
    peers=0;
    assert(parse_map_delta(&ml,{precedence,sizeof precedence-1})==0 && peers==1);
    assert(live==0);
    printf("PASS: real delta parser: 22KB ignored map, peers/removals/patches, 2KB DOM budget, malformed JSON, precedence (peak=%zu)\n",peak);
}
'''
with tempfile.TemporaryDirectory() as directory:
    temp = Path(directory)
    test = temp / "delta.cpp"
    test.write_text(harness, encoding="utf-8")
    includes = ["-I", str(cjson), "-I", str(root / "components/microlink/src")]
    for name, path in (("cjson", cjson / "cJSON.c"), ("scan", root / "components/microlink/src/ml_json_scan.c")):
        subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", *includes, "-c", str(path), "-o", str(temp / (name + ".o"))],check=True)
    binary = temp / "delta.exe"
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", *includes, str(test), str(temp / "cjson.o"), str(temp / "scan.o"), "-o", str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
