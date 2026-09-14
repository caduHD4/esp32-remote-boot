#!/usr/bin/env python3
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "embed_microlink_config.py"


class EmbedMicrolinkConfigTest(unittest.TestCase):
    def run_generator(self, config=None, create=True):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "config.local.microlink.json"
            output = root / "microlink_config.generated.h"
            if create:
                source.write_text(json.dumps(config), encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(SCRIPT), "--input", str(source), "--output", str(output)],
                text=True,
                capture_output=True,
            )
            return result, output.read_text(encoding="utf-8") if output.exists() else ""

    def test_missing_file_generates_disabled_configuration(self):
        result, header = self.run_generator(create=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("REMOTE_BOOT_MICROLINK_CONFIGURED 0", header)
        self.assertIn('REMOTE_BOOT_MICROLINK_AUTH_KEY ""', header)

    def test_valid_configuration_is_escaped_without_logging_secret(self):
        key = "tskey-auth-k12345678901234567890"
        result, header = self.run_generator({"auth_key": key, "device_name": 'boot-esp32"c3'})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertNotIn(key, result.stderr)
        self.assertIn("REMOTE_BOOT_MICROLINK_CONFIGURED 1", header)
        self.assertIn(key, header)
        self.assertIn('boot-esp32\\"c3', header)

    def test_invalid_auth_key_is_rejected(self):
        result, _ = self.run_generator({"auth_key": "invalid", "device_name": "esp"})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("auth_key", result.stderr)

    def test_control_characters_are_rejected(self):
        result, _ = self.run_generator({"auth_key": "tskey-auth-k1234567890\n", "device_name": "esp"})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("control", result.stderr)

    def test_stale_generated_sdkconfig_is_invalidated(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "config.local.microlink.json"
            output = root / "microlink_config.generated.h"
            defaults = root / "sdkconfig.defaults"
            generated = root / "sdkconfig.esp32c3_4mb_microlink"
            source.write_text(
                json.dumps({
                    "auth_key": "tskey-auth-k12345678901234567890",
                    "device_name": "esp",
                }),
                encoding="utf-8",
            )
            defaults.write_text("CONFIG_ML_H2_BUFFER_SIZE_KB=32\n", encoding="utf-8")
            generated.write_text("CONFIG_ML_H2_BUFFER_SIZE_KB=64\n", encoding="utf-8")

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--input", str(source),
                    "--output", str(output),
                    "--sdkconfig-defaults", str(defaults),
                    "--sdkconfig-generated", str(generated),
                ],
                text=True,
                capture_output=True,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertFalse(generated.exists())


if __name__ == "__main__":
    unittest.main()
