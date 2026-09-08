from pathlib import Path
import shutil,zipfile,subprocess
root=Path(__file__).resolve().parents[1];out=root/'dist';out.mkdir(exist_ok=True)
shutil.copy2(root/'.pio/build/esp32c3_4mb/firmware.bin',out/'esp32c3-4mb-firmware.bin')
for name in ['bootloader.bin','partitions.bin','size-report.json']:shutil.copy2(root/'.pio/build/esp32c3_4mb'/name,out/name)
shutil.copy2(root/'uefi/remote-boot/RemoteBoot.efi',out/'RemoteBoot.efi')
tracked=subprocess.check_output(['git','ls-files','-z'],cwd=root).decode().split('\0')
def archive(name,include):
    with zipfile.ZipFile(out/name,'w',zipfile.ZIP_DEFLATED) as z:
        for f in tracked:
            if f and include(f):z.write(root/f,'esp32-remote-boot/'+f)
archive('source.zip',lambda f:True)
# Include builders, agents and documentation; each installer archive is self-contained source.
archive('installer-linux.zip',lambda f:not f.startswith(('installer/windows/','agent/windows/','.github/')))
archive('installer-windows.zip',lambda f:not f.startswith(('.github/',)))
(out/'FLASHING.txt').write_text('Prefer pio run -e esp32c3_4mb -t upload from source.\nfirmware.bin is APP only at 0x10000; do NOT flash it at 0.\nFor ESP32-C3: bootloader.bin at 0x0, partitions.bin at 0x8000, firmware.bin at 0x10000.\nNo OTA. See source README and THIRD_PARTY.md before redistribution.\nNo universal ipxe.efi: build it for your ESP IPv4 address.\n')
shutil.copy2(root/'THIRD_PARTY.md',out/'THIRD_PARTY.md')
