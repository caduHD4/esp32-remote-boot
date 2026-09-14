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
fetch_peers = coord_source.split("static int do_fetch_peers", 1)[1].split(
    "static int do_start_long_poll", 1
)[0]
assert fetch_peers.count("ml_psram_malloc(ML_H2_BUFFER_SIZE)") == 1
assert "ml_psram_malloc(ML_JSON_BUFFER_SIZE)" not in fetch_peers
assert "ml_psram_malloc(ML_NOISE_FRAME_BUFFER_SIZE)" not in fetch_peers
assert "h2_recv + h2_total" in fetch_peers
assert "MapResponse buffer allocation failed" in fetch_peers
assert "if (ML_H2_BUFFER_SIZE > 65535)" in coord_source

with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "limits.cpp"
    binary = Path(directory) / "limits"
    source.write_text(
        '#define SOC_CPU_CORES_NUM 1\n'
        '#define tskNO_AFFINITY -1\n'
        '#include "esp32c3_limits.h"\n'
        '#include "wireguard_limits.h"\n'
        'static_assert(ML_TASK_NET_IO_CORE == -1);\n'
        'static_assert(ML_TASK_DERP_TX_CORE == -1);\n'
        'static_assert(ML_TASK_COORD_CORE == -1);\n'
        'static_assert(ML_TASK_WG_MGR_CORE == -1);\n'
        'static_assert(ML_NOISE_FRAME_BUFFER_SIZE == 24 * 1024);\n'
        'static_assert(WIREGUARDIF_MTU == 1280);\n'
        'int main() {}\n',
        encoding="utf-8",
    )
    subprocess.run(
        ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(microlink / "port"), "-I", str(wireguard / "include"), str(source), "-o", str(binary)],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
