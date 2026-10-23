# rbdiscover (local iPXE extension)

`ipxe/build.sh` copies this extension into a private export of upstream commit
`7ada3e0f04d0526747df5e0e46c05c94cbf7a7d1`. It does not patch the source cache.
The command is `rbdiscover DEVICE_ID SEC1_PUBLIC_KEY CACHE_IPV4 [TIMEOUT_MS]`.
Only a fully authenticated reply stores the IPv4 setting `${rb_ip}`. The EFI
contains public identity pins, a public cached address, and the embedded loader.

The first send targets the cache. Every subsequent half-second round also sends
to limited and directed broadcasts on the configured IPv4 interface and an mDNS
A query for `remote-boot-<last12-device-ID>.local`. A legacy unicast mDNS query
uses the same ephemeral socket, bound to the configured IPv4 so it has a usable
return address. Only local routes for that address participate (the boot script
configures `net0`). At most eight route entries are examined and four distinct
mDNS candidates are tried. mDNS is an **untrusted locator**, never a trust source.
Every candidate still needs the pinned ID/key, fresh nonce, exact reply framing,
source/signed IPv4 equality and P-256/SHA256 proof over the canonical 113 bytes.
There is one total discovery deadline across all retries/fallbacks. DHCP and the
subsequent HTTP fetch retain their separate pre-existing script timeouts.

The mDNS parser accepts at most 512 bytes, eight questions and 32 records; name
expansion has bounded traversal and length, including compressed-name loops.
Only an exact owner-name/class-IN A record with nonzero TTL can supply a
candidate. No DNS record may directly update `${rb_ip}` or the boot URL.

Run the public-profile, parser and builder tests:

```
python3 tests/test_discovery_profile.py
python3 tests/test_ipxe_mdns.py
python3 tests/test_ipxe_build.py
```

`tests/test_ipxe_qemu.py` documents the opt-in OVMF/QEMU environment. It creates
its own disposable virtual disk and a socket-backed Ethernet with DHCP, mDNS,
signed UDP and an HTTP script responder. The test-only private source copy adds
a command that verifies the canonical shared signature fixture through the
actual iPXE ECDSA implementation. This command is absent from production builds.
No real EFI variables, disks, LAN interfaces or hardware are used. Production
images must be built with `ipxe/build.sh`; the virtual proof is not physical boot
validation.
