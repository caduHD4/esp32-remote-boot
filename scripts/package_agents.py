"""Package verified NativeAOT artifacts; includes only the relevant installer."""
from pathlib import Path
import hashlib
import zipfile

root = Path(__file__).resolve().parents[1]
out = root / "dist"
out.mkdir(exist_ok=True)
checksums = []
for rid, system, executable in (("win-x64", "windows", "remote-boot-agent.exe"), ("linux-x64", "linux", "remote-boot-agent")):
    binary = root / "build/native-artifacts" / f"agent-{rid}" / executable
    if not binary.is_file():
        raise SystemExit(f"Missing verified agent artifact: {rid}")
    archive = out / f"remote-boot-agent-{rid}.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as package:
        package.write(binary, f"build/agent/{rid}/{executable}")
        installer = root / "installer" / system / ("install-agent.ps1" if system == "windows" else "install-agent.sh")
        package.write(installer, f"installer/{system}/{installer.name}")
        package.write(root / "agent/linux/remote-boot.service", "agent/linux/remote-boot.service")
        package.write(root / "docs/native-agent.md", "docs/native-agent.md")
    checksums.append(f"{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}")
(out / "agent-SHA256SUMS").write_text("\n".join(checksums) + "\n", encoding="utf-8")
