import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "embed_local_wifi.py"


class EmbedLocalWifiTest(unittest.TestCase):
    def run_generator(self, config):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "config.local.json"
            output = root / "local_wifi.h"
            source.write_text(json.dumps(config), encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(SCRIPT), "--input", str(source), "--output", str(output)],
                text=True,
                capture_output=True,
            )
            return result, output.read_text(encoding="utf-8") if output.exists() else ""

    def test_generates_escaped_compile_time_credentials(self):
        result, header = self.run_generator({"ssid": 'Casa "5G"', "wifi_password": "abc\\123"})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('#define REMOTE_BOOT_LOCAL_WIFI_SSID "Casa \\"5G\\""', header)
        self.assertIn('#define REMOTE_BOOT_LOCAL_WIFI_PASSWORD "abc\\\\123"', header)

    def test_rejects_missing_ssid(self):
        result, _ = self.run_generator({"wifi_password": "senha"})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("ssid", result.stderr)

    def test_generates_disabled_header_when_local_config_is_absent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "local_wifi.h"
            result = subprocess.run(
                [sys.executable, str(SCRIPT), "--input", str(root / "config.local.json"), "--output", str(output)],
                text=True,
                capture_output=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('#define REMOTE_BOOT_LOCAL_WIFI_SSID ""', output.read_text(encoding="utf-8"))

    def test_rejects_ssid_longer_than_32_utf8_bytes(self):
        result, _ = self.run_generator({"ssid": "é" * 17, "wifi_password": "password"})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("32 bytes", result.stderr)

    def test_rejects_control_characters_that_break_generated_header(self):
        result, _ = self.run_generator({"ssid": "Casa\nInvalida", "wifi_password": "password"})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("control characters", result.stderr)


if __name__ == "__main__":
    unittest.main()
