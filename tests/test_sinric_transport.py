#!/usr/bin/env python3
"""Characterize the repo-owned bounded TLS adjustment without a live server."""
import hashlib
import runpy
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('transport',root/'scripts/bound_sinric_tls.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class TransportTest(unittest.TestCase):
 def test_each_new_client_receives_seconds_timeout(self):
  source=b'            _client.ssl = new WEBSOCKETS_NETWORK_SSL_CLASS();\r\n            _client.tcp = _client.ssl;\r\n'
  digest=hashlib.sha256(source).hexdigest()
  patched=module.patch_bytes(source,digest)
  self.assertEqual(module.patch_bytes(patched,digest),patched)
  with tempfile.TemporaryDirectory() as temp:
   cpp=Path(temp)/'test.cpp';binary=Path(temp)/'test'
   cpp.write_text('#include <cassert>\n#define ESP32\nstruct SSL {unsigned timeout=120;void setHandshakeTimeout(unsigned n){timeout=n;}};\n#define WEBSOCKETS_NETWORK_SSL_CLASS SSL\nstruct Client{SSL* ssl;SSL* tcp;};\nvoid test(){Client _client{};\n'+patched.decode()+'assert(_client.ssl->timeout==3);delete _client.ssl;}\nint main(){test();test();}\n')
   subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',str(binary)],check=True)
   subprocess.run([str(binary)],check=True)
 def test_clean_does_not_access_missing_dependency(self):
  class CleanEnvironment:
   def IsCleanTarget(self):return True
   def subst(self,value):raise AssertionError('clean accessed dependency')
  context={'env':CleanEnvironment(),'Import':lambda name:None}
  runpy.run_path(str(root/'scripts/bound_sinric_tls.py'),init_globals=context)
 def test_unknown_dependency_fails_closed(self):
  with self.assertRaises(ValueError):module.patch_bytes(b'changed upstream file')
 def test_duplicate_allocation_site_rejected(self):
  source=(b'            _client.ssl = new WEBSOCKETS_NETWORK_SSL_CLASS();\r\n')*2
  with self.assertRaises(ValueError):module.patch_bytes(source,hashlib.sha256(source).hexdigest())
if __name__=='__main__':unittest.main()
