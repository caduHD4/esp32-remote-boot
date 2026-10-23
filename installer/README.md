# Local installation and mode conversion

Legacy IPv4 installation still accepts `bash installer/linux/install.sh IP` or
`./installer/windows/install.ps1 -EspAddress IP -IpxeFile PATH`. Pair a new
agent only once with the existing `install-agent` script.

For an **already paired PC**, choose its desired mode in the dashboard and
export that PC's public discovery profile. Run locally with administrator/root
privileges on native UEFI Linux or Windows. Converting either way preserves
PC ID, agent ID, token and power permissions:

```sh
sudo bash installer/linux/install.sh --discovery-profile /path/profile.json \
  --agent-file /path/current/remote-boot-agent
```

```powershell
# Build the PC-specific artifacts in Linux/WSL with the same public profile:
# bash ipxe/build.sh IP PC_ID /path/output 10000 /path/profile.json
./installer/windows/install.ps1 -DiscoveryProfile C:\transfer\profile.json `
  -IpxeFile C:\transfer\ipxe.efi -AgentFile C:\transfer\remote-boot-agent.exe
```

`--agent-file` / `-AgentFile` is optional when the installed native agent already
supports discovery CLI commands. An old agent can pass its old self-test but
reject `--apply-profile`; rerun with the current binary. Do **not** rerun the
pairing installer to upgrade. The supplied binary must pass `--self-test`.
Only the existing running service/task is stopped and restarted; no service or
scheduled task is created, reconfigured or enabled by conversion.

Compare the displayed fingerprint with an already trusted source, or with the
ESP's physical USB diagnostic when necessary. `TRUST` confirms the identity and
mode; it precedes all EFI confirmations. The normal-mode profile also confirms
the IPv4 on which normal mode will depend. The installer verifies the profile,
paired PC, manifest mode/revision/device/key and both actual artifact hashes
before changing configuration or EFI files. Windows also checks the selected
PE image and existing firmware entry's partition identity.

Linux builds with the profile automatically. Windows consumes the image,
embedded-loader source and manifest generated in Linux/WSL. The standalone
`RemoteBoot.efi` is hashed; the installed iPXE image contains the embedded loader.
The selected EFI directory receives `ipxe.efi` and `manifest.json`.

All conversion writes follow explicit `INSTALL` and, for an existing Linux
entry, `REUSE` confirmation. Windows additionally requests `VERIFIED` and
`REUSE`. Existing EFI entry and BootOrder remain unchanged. Only `TEST` opts
into BootNext; the installer never reboots or promotes the boot entry.

The printed private backup contains original agent configuration/binary,
original EFI image/manifest when present, public profile, new manifest and
restoration metadata. Windows saves configuration/binary ACLs; Linux records
file modes. On a partial failure, read the reported phase and backup path.
Rerunning with a fresh matching profile is supported. A rejected server report
leaves local preparation **pending**, even when EFI files have been written.
To explicitly restore this OS's previous files:

```sh
sudo bash installer/linux/install.sh --rollback /var/backups/remote-boot/BACKUP
```

```powershell
./installer/windows/install.ps1 -Rollback 'C:\ProgramData\RemoteBoot\backup-TIMESTAMP-ID'
```

Confirm with `RESTORE`. Keep/mount the original ESP; rollback checks its GPT
partition identity. It restores only the selected agent and EFI files. It
never imports global firmware snapshots or resets BootOrder/BootNext. A
conversion-created boot entry is removed only if unchanged and unreferenced;
otherwise it is left for explicit review. A previously selected BootNext may
therefore require manual review. Server desired mode/preparation is not undone
by local rollback: set the desired mode and rerun to report the restored state.

Each OS agent in dual boot must be converted separately. EFI can be shared.
An accepted preparation report describes installation; it is never proof of a
physical boot or of the other OS agent's readiness.
