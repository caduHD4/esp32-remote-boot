# ESP32 Remote Boot 3

[English](README.md) | [Português (Brasil)](README.pt-BR.md)

Choose which installed operating system a PC starts, wake it over Ethernet, and request a restart or shutdown from the ESP32 dashboard. Version **3.0.0-alpha.1**, configuration **v3**, API **v2**.

Supports up to **4 PCs**, **8 agent installations**, and **24 UEFI boot entries per PC**. Windows and Linux on the same physical computer share one PC record and have separate agents. Sinric Pro controls one selected PC.

## Read this first

Follow steps 1–11 in order. Each step has a checkpoint: solve any failure before continuing.

The roles are:

- **ESP32:** hosts the dashboard and stores the boot choice.
- **Agent:** runs inside Windows/Linux, reports boot entries, and handles permitted restart/shutdown requests.
- **iPXE + RemoteBoot.efi:** run before the operating system and apply the ESP32's boot choice. They are installed once per physical PC.
- **BIOS/UEFI:** starts iPXE and provides network access when the NIC needs a firmware driver.

The operating systems must already be installed and bootable. This project does not install Windows/Linux or host their installation images.

In commands below, **192.168.1.50 is an example**. Replace it with your ESP32's reserved LAN IPv4 address. Run project commands from the folder containing README.md, platformio.ini, agent/, installer/, and ipxe/. Do not copy the terminal prompt.

## 1. Check the hardware and prepare the BIOS

You need an ESP32-C3 with **4 MB flash**, a USB data cable, a **2.4 GHz Wi-Fi** network, and an x86-64 PC with Ethernet, UEFI, and Wake-on-LAN. The Linux installer requires systemd. Automated EFI installation requires GPT disks and a FAT EFI System Partition.

Connect the PC's Ethernet cable to the router. Power the ESP32 from a supply that stays on when the PC is off.

Enter the PC's BIOS/UEFI, usually with **Del** or **F2** while turning it on. Menu names vary:

1. Use **UEFI** mode; disable **CSM/Legacy**. Confirm that the existing Windows/Linux installations already boot in UEFI mode before changing this.
2. Disable **Secure Boot** for the project's unsigned EFI binaries. If Windows uses BitLocker/device encryption, have the recovery key available before changing boot settings. Do not clear the TPM.
3. Enable **Wake-on-LAN / Power On By PCI-E**. Disable **ErP/deep sleep** if it removes power from the Ethernet adapter when the PC is off.
4. Enable **Network Stack** and **IPv4 PXE Support**, often under **Advanced → Network Stack Configuration**; ASUS boards may require **F7** for Advanced Mode. This is needed for NICs using the firmware's UEFI/SNP driver, including RTL8125 with the iPXE revision used here.
5. Save and boot your existing operating system. Leave its boot priority in place until the iPXE test succeeds.

Enabling the firmware's network driver does not require a TFTP server. Later, select **Remote Boot iPXE**, rather than the separate firmware PXE/IPv4 boot option.

**Checkpoint:** Windows/Linux still starts normally, and the Ethernet cable is connected. See [BIOS details](docs/bios.md).

## 2. Get the source and installation tools

Get the updated **main** source from [GitHub](https://github.com/caduHD4/esp32-remote-boot): **Code → Download ZIP**, then extract it. Alternatively, with Git installed:

~~~bash
git -c core.autocrlf=false clone https://github.com/caduHD4/esp32-remote-boot.git
cd esp32-remote-boot
~~~

For a private repository, sign in with an account that has access. Do not use an old source folder containing unknown build artifacts.

### Linux: install prerequisites

Ubuntu/Debian:

~~~bash
sudo apt update
sudo apt install python3 python3-venv git build-essential binutils gnu-efi perl liblzma-dev jq curl efibootmgr ethtool
~~~

Arch/CachyOS:

~~~bash
sudo pacman -S --needed python git base-devel binutils gnu-efi perl xz jq curl efibootmgr ethtool
~~~

Create a Python environment **inside the project folder**:

~~~bash
python3 -m venv .venv
.venv/bin/python -m pip install platformio==6.1.19
.venv/bin/pio --version
~~~

### Windows: install prerequisites

Install [Python 3](https://www.python.org/downloads/windows/) with the Python launcher. Open PowerShell in the project folder and run:

~~~powershell
py -m venv .venv
.\.venv\Scripts\python.exe -m pip install platformio==6.1.19
.\.venv\Scripts\pio.exe --version
~~~

Use the executable paths shown here; activating the Python environment is unnecessary. If you already use pipx, the equivalent installation is: **pipx install --force platformio==6.1.19**.

**Checkpoint:** the version command prints **PlatformIO Core 6.1.19**. Use this exact version: 6.2.0 has a SCons compatibility issue with the MicroLink toolchain.

## 3. Configure and flash the ESP32

Copy config.local.example.json to **config.local.json** in the project folder. Edit only the values, retaining valid JSON:

~~~json
{
  "ssid": "YOUR_2_4_GHZ_WIFI",
  "wifi_password": "YOUR_WIFI_PASSWORD"
}
~~~

Keep this file private. Wi-Fi credentials are embedded during compilation; changing them requires another upload.

Choose **one** firmware variant:

| What you need | Variant |
|---|---|
| Dashboard, agents, boot control and Sinric on the LAN | esp32c3_4mb |
| The ESP32 itself must join Tailscale | esp32c3_4mb_microlink |

Connect the ESP32 by USB. For the standard variant:

~~~bash
# Linux
.venv/bin/pio run -e esp32c3_4mb -t upload
.venv/bin/pio device monitor -b 115200
~~~

~~~powershell
# Windows PowerShell
.\.venv\Scripts\pio.exe run -e esp32c3_4mb -t upload
.\.venv\Scripts\pio.exe device monitor -b 115200
~~~

For Tailscale, replace esp32c3_4mb in the upload command with **esp32c3_4mb_microlink**. If automatic USB selection fails, use the correct port with **--upload-port PORT** for upload and **-p PORT** for the monitor. Find it with **pio device list**, using the executable path for your OS above. Exit the monitor with **Ctrl+C**.

Write down the IPv4 address printed by the ESP32. In your router's DHCP settings, reserve that address for the ESP32 so it does not change.

**Checkpoint:** the upload succeeds and the ESP32 obtains a LAN IPv4 address. An ordinary upload preserves saved settings. Migration from version 2 requires backup and a separate erase because the partition layout changed; follow [migration/recovery](docs/setup.md) before flashing an existing version 2 device.

## 4. Open the dashboard and create the PC record

1. Open **http://YOUR_ESP32_IP** in a browser on the same LAN.
2. On first access, fill **Senha** and **Repetir senha**, then click **Salvar e reiniciar ESP**. The password must contain 8–128 characters. After the ESP32 restarts, log in with it.
3. Find the PC's **Ethernet MAC address**. On Windows, run **getmac /v** or **ipconfig /all**. On Linux, run **ip -br link**. Use the wired adapter, not Wi-Fi, Bluetooth, or Tailscale.
4. Click **Adicionar PC** and enter a name and that MAC address.

The dashboard currently uses Portuguese labels; this guide names the buttons as they appear.

For dual boot, create **one record for the physical PC**, then pair both OS agents to it. Create another PC record only for another computer.

**Checkpoint:** the PC card appears. Offline is expected until its agent connects. The internal PC ID is generated automatically; you do not need to find it in the dashboard.

## 5. Download the agents and check the ZIPs

From the same [3.x release](https://github.com/caduHD4/esp32-remote-boot/releases), download **agent-SHA256SUMS** and the package for each OS you use:

- Windows: **remote-boot-agent-win-x64.zip**
- Linux: **remote-boot-agent-linux-x64.zip**

Check each ZIP before extracting:

~~~powershell
Get-FileHash .\remote-boot-agent-win-x64.zip -Algorithm SHA256
~~~

~~~bash
sha256sum remote-boot-agent-linux-x64.zip
~~~

Compare the result with the line for that exact ZIP in agent-SHA256SUMS. They must match.

Extract the package to a separate folder. Copy its executable into your updated source folder at the matching path below; create the folders if needed. Use the installers from the updated source.

~~~text
project/
  README.md
  platformio.ini
  installer/
  agent/
  ipxe/
  build/
    agent/
      win-x64/remote-boot-agent.exe
      linux-x64/remote-boot-agent
~~~

You only need the executable for the current OS. No .NET runtime is needed; the release agents are NativeAOT.

**Checkpoint:** the executable exists at the expected path. Do not continue if the ZIP hash differs or the installer says the binary is missing.

## 6. Install and pair an agent in each operating system

On the dashboard, click **Parear agent** first and leave the dialog open. It opens a five-minute pairing window.

### Windows

Boot Windows on the target PC. Open **PowerShell as administrator**, change to the project folder, and run:

~~~powershell
.\installer\windows\install-agent.ps1 -EspAddress '192.168.1.50'
~~~

If PowerShell blocks the downloaded script, you can allow script execution for this terminal only with **Set-ExecutionPolicy -Scope Process Bypass**, then repeat the command. Organization policies may still apply.

### Linux

Boot the actual Linux installation on the target PC, **not WSL**. Confirm UEFI mode:

~~~bash
test -d /sys/firmware/efi && echo "UEFI OK"
~~~

Then, from the project folder:

~~~bash
chmod +x build/agent/linux-x64/remote-boot-agent
sudo bash installer/linux/install-agent.sh 192.168.1.50
~~~

Enter your Linux administrator password when sudo requests it.

### Complete the pairing

For either OS:

1. When asked about remote restart, type **REBOOT** to enable it, or press Enter to leave it disabled.
2. When asked about remote shutdown, type **SHUTDOWN** to enable it, or press Enter to leave it disabled.
3. The installer shows a short code such as **ABCD-EFGH**. Keep the terminal open.
4. Enter it in the dashboard and click **Consultar código**.
5. Check hostname/OS, select the correct **PC de destino**, and give the installation a name such as Windows or Linux.
6. Check **Conferi o computador e autorizo este vínculo**, then click **Confirmar vínculo**.
7. Wait for the installer to finish. Approval alone is not completion: it must save credentials, connect, and synchronize the boot catalog.

If the code expires, reopen the pairing window and rerun the installer. For dual boot, repeat this whole step after booting the other OS; select the **same PC record**. Install the agent in every OS from which you want status, restart, or shutdown.

**Checkpoint:** **Sistema → Agents vinculados** shows the current OS agent as connected and its requested permissions enabled. Linux also reports **active** with:

~~~bash
systemctl is-active remote-boot
~~~

On Windows, check the **RemoteBootAgent** task in Task Scheduler. Agents are installed separately from iPXE; this step does not modify boot entries.

## 7. Set the default system and enable Wake-on-LAN

Select the PC in the dashboard. Wait for its boot catalog to load; use **Sincronizar agent** under **Agents vinculados** if needed.

In **Configuração → Ajustes do PC selecionado**:

1. Set **Sistema padrão** to the OS loader you want for normal startup.
2. Set **Fallback** to another valid local loader if available.
3. Set **Botão físico do PC** to **Usar sistema padrão** for the initial test.
4. Click **Salvar ajustes do PC**.

Do not select Remote Boot/iPXE as a destination. These entries are blocked to prevent recursion. A GRUB entry starts GRUB, including its own menu; it does not select a particular item inside GRUB.

Enable WoL inside **every installed OS**:

- **Windows:** in the Ethernet adapter's Device Manager settings, enable Wake on Magic Packet and Shutdown Wake-On-Lan where available, and permit the adapter to wake the PC. Disable Windows Fast Startup. See [Windows WoL](docs/windows-wol.md).
- **Linux with NetworkManager:** find the wired connection using **nmcli connection show**, then run the command below with its exact connection name. Find the Ethernet interface with **ip -br link**.

~~~bash
sudo nmcli connection modify 'YOUR_ETHERNET_CONNECTION' 802-3-ethernet.wake-on-lan magic
sudo ethtool YOUR_ETHERNET_INTERFACE
~~~

Reactivate the connection when it is safe to interrupt networking, then confirm **Wake-on: g** with ethtool. Other network managers require their equivalent configuration; see [Linux WoL](docs/linux-wol.md).

**Checkpoint:** the dashboard has valid default/fallback choices and the agent is connected. Verify wake from a real shutdown later; WoL support depends on the NIC, firmware, and last OS shut down.

## 8. Build and install iPXE once per PC

Use **A** if the physical PC has Linux. Use **B** if it only has Windows. For dual boot, A is sufficient: do not install another iPXE image from Windows just because you installed a second agent.

### A. Install from the PC's real Linux system

Boot Linux and confirm that its agent is paired. The dependencies from step 2 must be installed. Run from the updated project folder:

~~~bash
sudo bash installer/linux/install.sh
~~~

The installer **reads the ESP32 address and internal PC ID from /etc/remote-boot/agent.json**, builds the PC-specific image, and checks its manifest. You do not supply a PC ID manually.

Answer its prompts in order:

1. **EFI System Partition mount:** review the detected partition, normally /boot/efi, /efi, or /boot. Press Enter only if it is correct. This must be the mounted FAT GPT EFI partition, not the Linux root filesystem or a Windows data partition.
2. **INSTALL:** type INSTALL to write the image and create the UEFI entry. Keep the displayed backup path.
3. **REUSE:** if an existing Remote Boot entry is found, type REUSE only after confirming it points to the selected EFI partition. Duplicate entries need investigation.
4. **TEST:** type TEST to request a one-time iPXE startup on the next reboot.

The installer does not reboot. It reports the actual **BootXXXX** ID it created or reused; write it down. The number varies by PC.

### B. Build with Linux/WSL and install from Windows

The EFI build requires Linux tools. On Windows-only PCs, install Ubuntu/WSL if needed with **wsl --install -d Ubuntu** in administrator PowerShell, restart if requested, and finish Ubuntu's first-run setup.

In **administrator PowerShell on the target Windows PC**, read only the public PC ID saved by its paired agent:

~~~powershell
$pcId = (Get-Content "$env:ProgramData\RemoteBoot\agent.json" -Raw | ConvertFrom-Json).pc_id
$pcId
~~~

Copy that 32-character value. Do not copy or share the full agent.json file; it contains a secret token.

Open Ubuntu/WSL, install the Ubuntu build dependencies from step 2, and enter the source folder. A Windows path such as C:\Users\YOUR_USER\Downloads\esp32-remote-boot is typically /mnt/c/Users/YOUR_USER/Downloads/esp32-remote-boot inside WSL. Use quotes around paths with spaces.

Build with the real ESP32 IP and the ID you just read:

~~~bash
bash ipxe/build.sh 192.168.1.50 PASTE_PC_ID_HERE
~~~

The build creates **build/PC_ID/ipxe.efi**, **RemoteBoot.efi**, **manifest.json**, and **SHA256SUMS**. Keep them together. If you compiled on a separate Linux machine, copy that complete output folder back to the target Windows PC.

Return to **administrator PowerShell in the Windows source folder**:

~~~powershell
.\installer\windows\install.ps1 -EspAddress '192.168.1.50' -IpxeFile ".\build\$pcId\ipxe.efi" -FullScan
~~~

Review the output and answer **VERIFIED** after checking the build/hash. Choose the correct **DiskNumber** and **PartitionNumber** from the EFI partitions it lists; if several exist and you cannot identify the intended one, stop and consult [EFI discovery](docs/uefi-discovery.md). Type **INSTALL** to install, then **TEST** for a one-time test. Keep the backup path and BootXXXX ID.

**Do not run installer/linux/install.sh or install-agent.sh in WSL to modify the Windows PC.** WSL is only the compiler in this route; the Windows installers handle Windows.

**Checkpoint for either route:** the installer completes, the image targets this PC and ESP32, and you have the backup path plus the Remote Boot entry's ID. On Linux, **efibootmgr -v** lists **Remote Boot iPXE** pointing to **\\EFI\\iPXE\\ipxe.efi**. Do not reuse one PC's image on another PC.

## 9. Test iPXE before making it the default

Save your work and restart the PC. TEST uses **BootNext**, so this first attempt is temporary. Watch for:

1. **Remote Boot**
2. **Configuring [dhcp] ... ok**
3. **http://YOUR_ESP32_IP/boot/PC_ID.ipxe ... ok**
4. The selected/default OS starting. RemoteBoot.efi may set BootNext and perform a second reset; that is expected.

The agent should reconnect after the OS starts. Test every intended OS loader, not just one.

If the PC returns to the ordinary OS menu without showing iPXE, open the **firmware's boot menu**, not GRUB's menu. Try **Remote Boot iPXE**. Some firmware menus omit entries outside BootOrder even when they exist; an available **Boot from EFI file** option can start **EFI/iPXE/ipxe.efi** directly. See [diagnosis](docs/troubleshooting.md) before changing the default boot priority.

**Checkpoint:** iPXE gets a DHCP address, downloads the PC-specific script, and the desired OS actually starts. Leave the normal OS boot priority in place if this test fails.

## 10. Make Remote Boot the first UEFI option and test remote power

After step 9 succeeds, enter BIOS/UEFI boot priorities, put **Remote Boot iPXE first**, retain the existing Windows/Linux entries after it, and save. The installers deliberately preserve the old BootOrder; the temporary TEST alone does not make iPXE permanent.

Boot once more and confirm the default OS starts. Then test from the dashboard:

1. Shut down the PC normally; keep the ESP32 powered and Ethernet connected.
2. Wait until the dashboard reports the PC offline.
3. In the desired OS card, click **Ligar**. It queues the choice and sends Wake-on-LAN.
4. Confirm the PC wakes, starts iPXE, and enters that OS.
5. Repeat after shutting down each installed OS, because each can affect the adapter's WoL configuration.

While the PC is online, use **Reiniciar aqui** to change OS and confirm the request; the current OS agent must permit reboot. **Ligar** does not restart an already running PC. **Forçar WoL** only sends WoL; it does not restart it either. **Desligar** requires the current agent's SHUTDOWN permission.

If a test fails, select the existing Windows/Linux loader from the UEFI menu and diagnose it. Backups are in **/var/backups/remote-boot** on Linux or **%ProgramData%\RemoteBoot\backup-*** on Windows. See [recovery](docs/setup.md).

**Checkpoint:** default boot, remote OS selection, WoL, and the power commands you enabled work on the actual PC.

## 11. Optional: Tailscale and Sinric Pro

**Tailscale:** the ESP32 must run **esp32c3_4mb_microlink**. In the dashboard, enter the Auth Key under **Configuração → Tailscale**, save, and wait for reconnection. The standard firmware cannot join Tailscale; saving a key does not add that feature. Validate the LAN installation first. See [MicroLink/Tailscale](docs/microlink-tailscale.md).

**Sinric Pro:** create an app and Switch devices in the [Sinric portal](https://portal.sinric.pro). Select **one PC** in the dashboard's Sinric settings, save that selection, then set App Key, App Secret, Device IDs, and boot mappings. ON requests the action; OFF does not shut down the PC. Changing the selected PC clears mappings. Shutdown requires a connected agent with permission. See [Sinric setup](docs/sinric.md).

## If something fails

| Message or symptom | What to check first |
|---|---|
| Missing native agent binary | Step 5: executable location and ZIP contents. |
| Pairing code expired | Reopen Parear agent and restart the installer; finish within five minutes. |
| sudo asks for a password | Enter the Linux administrator password in your terminal; dashboard/chat authorization does not authenticate sudo. |
| Bash reports pipefail with an extra carriage return | The script has Windows CRLF line endings. Use a fresh source ZIP or clone with core.autocrlf=false. |
| net0 no such network device | Enable firmware Network Stack/IPv4 PXE. RTL8125 needs the UEFI/SNP driver with this iPXE revision. |
| file:autoexec.ipxe not found | If Remote Boot follows, the embedded script is running; diagnose the later error. |
| DHCP fails | Ethernet cable, router DHCP, firmware driver, VLAN, and the interface selected by iPXE. |
| Script URL cannot be downloaded | Reserved ESP32 IP, network reachability, and PC record. Rebuild if the ESP32 IP/PC identity changed. |
| Exec format error / RemoteBoot failed | Use the current builder and rebuild/reinstall. RemoteBoot.efi must be a real x86-64 EFI application, not a fixture or stale artifact. |
| iPXE not listed in GRUB | It is a UEFI boot option, separate from the GRUB menu. Check the firmware menu and EFI entry. |
| PC boots normally after TEST | TEST is temporary; verify step 9, then configure permanent priority in step 10. |
| PC does not wake | BIOS and last OS WoL settings, correct Ethernet MAC, NIC power, ESP32 power, and LAN broadcast isolation. |
| OS changes but status stays offline | Install/pair an agent in that OS to the same PC record. |

More: [troubleshooting](docs/troubleshooting.md), [agent details](docs/native-agent.md), [API](docs/api.md), [architecture](docs/architecture.md), [security](SECURITY.md).

## Updates and development checks

Ordinary firmware uploads preserve NVS settings. Changing the ESP32 IP, PC identity, or EFI code requires rebuilding and reinstalling that PC's iPXE image. Updating only the ESP32 firmware does not replace the PC's EFI files. There is no OTA update.

From the source folder, with development dependencies installed:

~~~bash
bash tests/run.sh
pio run -e esp32c3_4mb
pio run -e esp32c3_4mb_microlink
npm ci
npx playwright install
npm run test:ui
~~~

The size gate rejects firmware above 90% of the real application partition. Firmware configuration uses two NVS banks with CRC and generation checks. Software tests do not prove physical WoL or boot behavior on every motherboard; validate your hardware as described above and in the [test matrix](docs/testing/multi-pc-agent-pairing.md).

Keep Wi-Fi credentials, agent tokens, firmware backups, private snapshots, and machine-specific UKIs out of Git.
