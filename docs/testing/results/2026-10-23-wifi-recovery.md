# ESP32-C3 Wi-Fi availability investigation — 2026-10-23

## Observations

The board was powered through the PC USB port and remained lit when Google
Gemini reported that its boot command failed. At investigation start, LAN HTTP
and ARP resolution failed for the ESP. The initial serial attachment unexpectedly
caused `USB_UART_CHIP_RESET` despite DTR/RTS being disabled. LAN HTTP returned.
The residual serial output contained a DNS failure at about 34 hours uptime;
it does not establish the original Wi-Fi disconnect reason or prove a crash.

The installed image descriptor identified commit `e1e5180` (2026-10-08), API v2,
configuration schema 3, and the MicroLink variant. Its firmware sources match
those on the inspected main branch. Configuration was valid, Sinric pointed at
the existing PC, and its CachyOS switch targeted boot entry `0000`. The WoL MAC
matched the host Ethernet interface. Existing UEFI entries were only inspected.

## Demonstrated defects and correction

The actual `loop()` attempted Sinric DNS/TLS without a Wi-Fi IP. It also observed
the Wi-Fi retry policy without servicing it. The installed Arduino STA SDK only
automatically retries selected disconnect reasons and does not recover lost IP
on its own in every state. A stopped automatic retry could therefore leave
both the LAN dashboard and cloud control unavailable indefinitely.

Sinric handling now waits for `WL_CONNECTED`. If the SDK has not recovered an
IP within 20 seconds, the application requests `WiFi.reconnect()` using the
existing STA configuration. Attempts are spaced by 20 seconds, including rejected
attempts, with a grace period after loss. There is still one `WiFi.begin()` at
startup; the correction does not clear network credentials or configuration.
Recovery attempts are logged; the IP is reported again after reconnection.

This corrects demonstrated missing recovery and premature cloud work. The exact
initiating cause of the earlier outage remains unknown because its pre-reset
Wi-Fi event history was not available.

## Preservation and hardware validation

- A private 4 MiB flash backup, SHA256 checksums and application rollback image
  were saved outside Git, under the user's local state directory.
- Only the application at `0x20000` was written. Read-back matched the built
  image byte for byte. Bootloader, partition table and PHY region were unchanged.
- Decoded full application configuration before and after was identical,
  including credentials, one PC, two agents and two Sinric mappings. Runtime
  NVS bytes can change due to peer caching; raw NVS equality is not asserted.
- A temporary diagnostic image invoked `esp_wifi_disconnect()` once, at 60s
  uptime, without erasing network settings. Recovery was requested at about
  80s, reassociation succeeded and LAN HTTP and Sinric returned without a reboot.
  Timeouts during this intentional outage are expected. The diagnostic
  injection was removed before building and writing the final image.
- A real normal boot request while the PC was online returned `409 PC_STATE_CONFLICT`; no boot was queued and no force/reboot/shutdown was used.
- Real authenticated dashboard login succeeded with the existing password;
  no JavaScript page errors were observed.
- `bash tests/run.sh` passed. The compiled actual-loop regression first failed
  on offline Sinric work, then on absent Wi-Fi recovery, and passed after the
  fixes. Policy tests cover the recovery window and timer wrap.
- The Chromium UI suite passed 96 tests at four widths, using an already
  installed Chromium executable. Firefox and WebKit were not run.

Independent code review found no Critical/Important issues. One nonblocking
coverage limitation remains: the loop harness stubs MicroLink and does not
exercise a held Sinric TLS lease across disconnection. Source inspection found
that the lease is serviced after reconnection.

## Final monitoring and remaining physical test

The user requested stopping the soak before the planned 30 minutes. It ended
after 1261 seconds (about 21 minutes, including 30 seconds of initial
warm-up): 122 successful LAN page loads and 243 successful authenticated
status reads, with zero LAN failures. Sinric was online in every status sample.
There was no reset after the initial USB monitor attachment, no DNS failure,
and no panic/watchdog/brownout in the captured serial output. Lowest sampled
free heap was 52420 bytes; the slowest status read took 0.305 seconds.

The real authenticated browser recorded 191 API responses, zero API/request
failures and zero JavaScript page errors. Checks over the local host's Tailscale
client recorded 40 successes and one timeout on the first attempt at about
30 seconds; this does not substitute for an Android/mobile-data remote test.
Serial and browser monitoring processes were stopped at the user's request.

The physical PC-off → Gemini → CachyOS boot test remains unperformed. The user
will perform it manually with the same USB power arrangement; wait at least
45 seconds after shutdown so stale agent presence can expire before the voice
command. The session did not shut down the working PC automatically. WoL
activation could not be read using the unprivileged `ethtool` request; no host
or UEFI settings were changed. The exact cause that initiated the historic
outage remains unconfirmed; this correction addresses demonstrated recovery
and startup defects rather than claiming that the original incident was
fully reconstructed.
