#!/usr/bin/env python3
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
microlink = root / "components/microlink"
wireguard = root / "components/wireguard_lwip"

assert (microlink / "LICENSE").is_file(), "MicroLink license is missing"
assert (wireguard / "LICENSE").is_file(), "WireGuard license is missing"
assert not (microlink / "components/wireguard_lwip").exists(), "nested ESP-IDF component will not be discovered"

upstream = (microlink / "UPSTREAM.md").read_text(encoding="utf-8")
assert "216da3300f0493b0860247d43f7af5ce29df63a5" in upstream

kconfig = (microlink / "Kconfig").read_text(encoding="utf-8")
h2_config = kconfig.split("config ML_H2_BUFFER_SIZE_KB", 1)[1].split(
    "config ML_JSON_BUFFER_SIZE_KB", 1
)[0]
assert "default 32" in h2_config
assert "range 32 2048" in h2_config

coord_source = (microlink / "src/ml_coord.c").read_text(encoding="utf-8")
register = coord_source.split("static int do_register", 1)[1].split("static int do_fetch_peers", 1)[0]
assert '#include "ml_register_scratch.h"' in coord_source
assert '_Static_assert(ML_H2_BUFFER_SIZE >= ML_REGISTER_SHARED_MIN_CAPACITY' in coord_source
assert "ml_register_workspace_init(&workspace, s_map_response_buffer" in register
for name in ("h2_resp", "resp_buf", "frame_buf"):
    assert f"free({name})" not in register
for size in (16384, 8192, 4096):
    assert f"ml_psram_malloc({size})" not in register
fetch_peers = coord_source.split("static int do_fetch_peers", 1)[1].split(
    "static int do_start_long_poll", 1
)[0]
assert "static uint8_t s_map_response_buffer[ML_H2_BUFFER_SIZE + 1];" in coord_source
assert "uint8_t *h2_recv = s_map_response_buffer;" in fetch_peers
assert "ml_psram_malloc(ML_H2_BUFFER_SIZE)" not in fetch_peers
assert "ml_psram_malloc(ML_JSON_BUFFER_SIZE)" not in fetch_peers
assert "ml_psram_malloc(ML_NOISE_FRAME_BUFFER_SIZE)" not in fetch_peers
assert "h2_recv + h2_total" in fetch_peers
assert "MapResponse buffer allocation failed" not in fetch_peers
assert "if (ML_H2_BUFFER_SIZE > 65535)" in coord_source

with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "limits.cpp"
    binary = Path(directory) / "limits"
    source.write_text(
        '#define SOC_CPU_CORES_NUM 1\n'
        '#define tskNO_AFFINITY -1\n'
        '#include "esp32c3_limits.h"\n'
        '#include "ml_register_scratch.h"\n'
        '#include <cassert>\n'
        '#include <cstring>\n'
        '#include "wireguard_limits.h"\n'
        'static_assert(ML_TASK_NET_IO_CORE == -1);\n'
        'static_assert(ML_TASK_DERP_TX_CORE == -1);\n'
        'static_assert(ML_TASK_COORD_CORE == -1);\n'
        'static_assert(ML_TASK_WG_MGR_CORE == -1);\n'
        'static_assert(ML_NOISE_FRAME_BUFFER_SIZE == 24 * 1024);\n'
        'static_assert(WIREGUARDIF_MTU == 1280);\n'
        'static_assert(ML_REGISTER_H2_SIZE == 16384);\n'
        'static_assert(ML_REGISTER_RESPONSE_SIZE == 8192);\n'
        'static_assert(ML_REGISTER_FRAME_SIZE == 4096);\n'
        'static_assert(ML_REGISTER_RESPONSE_OFFSET == ML_REGISTER_H2_SIZE);\n'
        'static_assert(ML_REGISTER_FRAME_OFFSET == ML_REGISTER_RESPONSE_OFFSET + ML_REGISTER_RESPONSE_SIZE);\n'
        'static_assert(ML_REGISTER_WORKSPACE_SIZE == 28672);\n'
        'static_assert(ML_REGISTER_SHARED_MIN_CAPACITY == 32768);\n'
        'int main() {\n'
        '  unsigned char memory[32768]; std::memset(memory, 0, sizeof memory);\n'
        '  ml_register_workspace w{};\n'
        '  assert(!ml_register_workspace_init(&w, memory, 32767));\n'
        '  assert(!ml_register_workspace_init(&w, memory, 28672));\n'
        '  assert(!ml_register_workspace_init(&w, nullptr, sizeof memory));\n'
        '  assert(!ml_register_workspace_init(nullptr, memory, sizeof memory));\n'
        '  assert(ml_register_workspace_init(&w, memory, sizeof memory));\n'
        '  std::memset(w.h2, 1, ML_REGISTER_H2_SIZE);\n'
        '  std::memset(w.response, 2, ML_REGISTER_RESPONSE_SIZE);\n'
        '  std::memset(w.frame, 3, ML_REGISTER_FRAME_SIZE);\n'
        '  for (unsigned i=0;i<sizeof memory;++i)\n'
        '    assert(memory[i] == (i<16384?1:i<24576?2:i<28672?3:0));\n'
        '}\n',
        encoding="utf-8",
    )
    subprocess.run(
        ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(microlink / "port"), "-I", str(microlink / "src"), "-I", str(wireguard / "include"), str(source), "-o", str(binary)],
        check=True,
    )
    subprocess.run([str(binary)], check=True)

long_poll = coord_source.split("static int consume_map_data", 1)[1].split("static int consume_h2_frame", 1)[0]
assert "ml_frame_feed_buffer(&ml->map_rx" in long_poll
assert "s_map_response_buffer, sizeof(s_map_response_buffer)" in long_poll
assert "cJSON_ParseWithLengthOpts" not in long_poll
assert "parse_map_delta(ml, update)" in long_poll
