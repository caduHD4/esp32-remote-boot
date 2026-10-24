#!/usr/bin/env python3
"""Public builder-profile rejection tests (no device credentials)."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('profile', ROOT / 'scripts/discovery_profile.py')
profile = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile)
FIXTURE = json.loads((ROOT / 'tests/fixtures/discovery-v1.json').read_text())['profile']

class Profiles(unittest.TestCase):
    def load(self, value):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'profile.json'
            p.write_text(value if isinstance(value, str) else json.dumps(value))
            return profile.load(p)
    def test_valid_shared_profile(self):
        self.assertEqual(self.load(FIXTURE), FIXTURE)
    def test_invalid_contract_fields(self):
        changes = [('mode_revision', 2**32), ('mode_revision', True),
                   ('last_ipv4', '255.255.255.255'), ('last_ipv4', '0.1.2.3'),
                   ('last_ipv4', '127.2.3.4'), ('last_ipv4', '224.0.0.251'),
                   ('hostname', 'remote-boot-other.local'), ('hostname', 23),
                   ('fingerprint', '0'*64), ('pc_id', '  '+'00'*15),
                   ('bearer', 'synthetic-secret'),
                   ('public_key', '04' + '00'*64)]
        for key, value in changes:
            with self.subTest(key=key, value=value):
                p = copy.deepcopy(FIXTURE); p[key] = value
                if key == 'public_key':
                    import hashlib
                    p['fingerprint'] = hashlib.sha256(bytes.fromhex(value)).hexdigest()
                with self.assertRaises(SystemExit): self.load(p)
    def test_unbound_zero_id_is_not_a_pinned_profile(self):
        p = copy.deepcopy(FIXTURE)
        p['device_id'] = '0'*32
        p['hostname'] = 'remote-boot-000000000000.local'
        with self.assertRaises(SystemExit): self.load(p)
    def test_duplicate_fields_rejected(self):
        raw = json.dumps(FIXTURE).replace('"profile_version": 1', '"profile_version": 2, "profile_version": 1')
        with self.assertRaises(SystemExit): self.load(raw)

if __name__ == '__main__': unittest.main()
