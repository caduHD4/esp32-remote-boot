# Remote Boot LAN discovery v1 (normative contract)

IPv4 UDP port **49192**. All multibyte integers big endian, all lengths exact.
Maximum datagram512 bytes. No credential, MAC-based trust, HTTP redirect, subnet
scan or Internet relay. ECDSA P-256/SHA256, public point SEC1 uncompressed65 bytes,
signature IEEE-P1363 r||s64 bytes. Public JSON fields are lowercase hexadecimal.
Fingerprint is SHA256(public point65), lowercase hexadecimal64.

## Wire

Query40 bytes: offsets0..3 ASCII `RBD1`; byte4 type1; byte5 zero; u16@6=40;
nonce16 bytes@8 (cryptographic random); requested device ID16@24, all zero for
unbound listing only.

Reply177 bytes: magic@0; type2@4; flags0@5; u16@6=177; copied nonce16@8;
device ID16@24; current IPv4 network octets4@40; HTTP u16@44 (80); WS u16@46
(81); public point65@48; signature64@113. Sign SHA256(bytes0..112).
Bound clients must compare device ID and public point to the pinned profile,
verify signature and nonce, and require response source IPv4==signed IPv4.
The nonce expires with that discovery attempt. Reject invalid/trailing lengths,
zero/broadcast/multicast IPv4, ports other than configured valid range,
non-uncompressed public key, mismatched nonce/ID/key and invalid signature.
Unbound discovery can display public candidates but cannot send bearer tokens
or silently trust a key. Two devices with different keys cannot be substituted.

Cache-IP unicast first; if it fails, IPv4 limited and directed LAN broadcasts.
Agent budget10 seconds per attempt with bounded retries/backoff. iPXE total
budget is the existing supplied timeout (default10000ms) for the discovery
phase, not timeout per peer. Never put credentials in an EFI image.

## Public conversion profile JSON

Required keys:
`profile_version:1`, `protocol:2`, `discovery_version:1`, `pc_id` hex32,
`blind_mode` boolean, `mode_revision` positive uint32, `device_id` hex32,
`public_key` hex130, `fingerprint` hex64, `last_ipv4` dotted IPv4,
`http_port:80`, `ws_port:81`, `hostname` unique remote-boot-<ID suffix>.local.
No extra secret fields. Export from authenticated dashboard. First conversion
requires manual fingerprint confirmation; obtain/compare public fingerprint
through physical USB if there is no previously trusted source. Do not derive
trust from a previously sent HTTP bearer token alone.

## Server and configuration interfaces

Firmware keeps schema3 and protocol2. Optional pc `blind_mode` defaults false;
optional `mode_revision` defaults1. Increment revision only when desired mode
actually changes; creating a PC starts at1. Persist PC boot preparation as
`boot_mode_revision` (0 default), `boot_blind_mode`, `boot_ipxe_sha256` and
`boot_loader_sha256`; only the installer report can change these.

Identity is in separate NVS namespace `rb-discovery`. Persist one versioned
blob containing ID, private scalar32 and public point65 with integrity check;
never overwrite corrupt stored identity or regenerate it silently. An identity
failure leaves normal mode functional but makes blind activation/profile fail.
Initialize identity only once during resource preparation with Wi-Fi entropy
available; avoid endless writes/repeated keygen from loop(). Public identity
fingerprint may be printed to USB; no private scalar or tokens are printed.

* Admin `GET /api/v2/discovery`: public device identity/status, no private data.
* Admin `GET /api/v2/pcs/<id>/discovery-profile`: above profile.
* PC create/update accepts `blind_mode` boolean; absent on update preserves
  current value, absent on legacy load means false. Network DHCP settings unchanged.
* Approved pairing poll adds `discovery_profile` object, so new clients can pin
  metadata during approved first pairing. Old clients ignore optional additions.
* Agent hello adds `discovery_version` (1 supported), `blind_mode`, `mode_revision`.
  Expose observed capabilities per agent; absent means old/unprepared, not ready.
* Agent-bearer `POST /api/v2/agent/preparation` accepts PC/agent IDs matching token,
  `mode_revision`, `blind_mode`, `discovery_version:1`, `boot_prepared` boolean.
  Only when boot_prepared=true also require exact `ipxe_sha256` and `loader_sha256`
  lowercase hex64. Reject stale mode/revision, mismatched IDs or unknown token.
  Persist per-agent preparedness and PC boot preparation; idempotent repeated
  reports must not continually write NVS. This is an installation report, NOT
  proof of physical boot. Export fields to bootstrap/PC/agent metadata.

UDP service runs only when connected and identity ready. At most1 incoming
packet processed per loop, at most4 signed replies/sec globally, skip signing
under unsafe memory pressure; never turn recovery into an allocation flood.
Register hostname and _remote-boot._udp mDNS service when IP available; recover
registration after IP changes without stopping Sinric or existing LAN services.

## Native agent CLI (cross-task contract)

Keep old --pair/--config/--check/--self-test behavior.

* `--discover [--profile FILE] [--timeout-ms N]`: public JSON candidates to stdout,
  no bearer sent. With profile, return only signature-verified pinned device.
* `--resolve --profile FILE`: print verified current IPv4 only; exit nonzero if
  not found. For installer use. Diagnostics go to stderr.
* `--apply-profile FILE --config FILE --confirm-fingerprint HEX64`: validate
  exact fingerprint, PC ID, profile and current pinned identity if already set;
  preserve credentials/permissions. For blind verify device before saving URL.
  For normal save explicitly confirmed profile.last_ipv4 and keep pin metadata
  for future conversion. No EFI writes, OS power action or service installation.
* `--report-preparation MANIFEST --config FILE`: authenticated preparation report
  to verified ESP after installer checked/wrote EFI. Validate manifest identities,
  mode/revision/hashes against local profile and config; report boot_prepared=true.

Config adds optional `blind_mode`, `mode_revision`, `device_id`, `public_key`,
`fingerprint`, `discovery_version`. Legacy config still valid. A blind config
requires complete pinned metadata. On reconnect verify cached endpoint via UDP
before transmitting credentials; on failure discover LAN interfaces; save URL
atomically using the actual supplied --config path. Pairing supplies profile when
available; do not turn an old client blindly into mode cego from server wishes.
Native unbound finder can open a selected dashboard only after explicit user
choice; printing candidates with URLs is the minimum required locator behavior.

## EFI builder / installer contract

`ipxe/build.sh IPV4 PC_ID [out] [timeout] [public-profile.json]`: old4 arguments
unchanged. With profile require matching PC ID and validate public fingerprint.
Mode false builds existing static template. Mode true invokes `rbdiscover` with
pinned ID/key and cacheIP, then uses discovered IPv4 for existing boot script.
Maintain embedded RemoteBoot.efi and upstream revision pin. Apply checked-in
local extension in a task-owned writable build checkout, never modify a user's
unrelated dirty iPXE checkout or global SDK.

Manifest keeps old esp_ipv4/pc_id/hashes and adds `profile_version:1`, `blind_mode`,
`mode_revision`, `device_id`, `public_key`, `fingerprint`, `discovery_version:1`.
Normal legacy manifests remain valid only for unchanged normal installs; mode
conversion requires full public profile + new manifest. Windows still builds in
Linux/WSL then installs on native Windows. No cloud build or new EFI privileges
in the agent. No BootOrder promotion; BootNext test remains explicit.

Installer accepts optional `--discovery-profile FILE` (Linux) / `-DiscoveryProfile`
(Windows); displays fingerprint and mode, requests TRUST confirmation first,
then existing EFI confirmations. Backs up agent config and installed artifacts.
Checks profile+manifest BEFORE writes; updates agent from confirmed profile,
writes EFI atomically with existing backup semantics, then reports preparation.
If partial failure, report exact phase, retain backups and permit rerun/rollback.
Never claim physical boot success from a preparation report. Update each OS's
agent independently in dual-boot; EFI shared entry need only be converted once.
