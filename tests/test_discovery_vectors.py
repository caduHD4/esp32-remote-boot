#!/usr/bin/env python3
"""Verify shared synthetic vectors through the real C framing helper + P-256.
Requires the system C compiler and Python cryptography; never reads device data.
"""
import ctypes
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec, utils
root = Path(__file__).resolve().parents[1]
v = json.loads((root / 'tests/fixtures/discovery-v1.json').read_text())
p = v['profile']
with tempfile.TemporaryDirectory(prefix='rb-vectors-') as td:
    source = Path(td)/'verify.c'
    source.write_text('#include "discovery_protocol.h"\nint framing(const unsigned char *p, size_t n, const unsigned char *nonce,const unsigned char *id,const unsigned char *key,const unsigned char *ip){return rb_discovery_valid_reply(p,n,nonce,id,key,ip,80,81);}\n')
    output = Path(td)/'verify.so'
    subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror','-I'+str(root/'shared'),str(source),'-o',str(output)],check=True)
    lib = ctypes.CDLL(str(output))
    lib.framing.argtypes = [ctypes.c_char_p,ctypes.c_size_t,*([ctypes.c_char_p]*4)]
    def verify(data, nonce=bytes.fromhex(v['nonce_hex']), device=bytes.fromhex(p['device_id']), public=bytes.fromhex(p['public_key']), ip=bytes(map(int,v['source_ipv4'].split('.')))):
        if not lib.framing(data,len(data),nonce,device,public,ip):
            return False
        try:
            key = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(),data[48:113])
            signature = utils.encode_dss_signature(int.from_bytes(data[113:145],'big'),int.from_bytes(data[145:177],'big'))
            key.verify(signature,data[:113],ec.ECDSA(hashes.SHA256()))
            return True
        except (ValueError,InvalidSignature):
            return False
    good=bytes.fromhex(v['reply_hex'])
    assert hashlib.sha256(bytes.fromhex(p['public_key'])).hexdigest()==p['fingerprint']
    assert verify(good)
    assert not verify(good,nonce=b'\xff'*16)
    assert not verify(good,device=b'\xff'*16)
    assert not verify(good,ip=bytes([192,168,50,81]))
    for bad in v['invalid']:
        assert not verify(bytes.fromhex(bad['reply_hex'])),bad['name']
print('PASS shared C framing + ECDSA signed vectors, nonce replay, identity pin and source-address binding')
