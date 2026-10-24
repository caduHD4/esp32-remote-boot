#!/usr/bin/env python3
"""Compile and exercise the exact bounded preboot DNS parser with sanitizers."""
import ctypes
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
NAME = b'\x18remote-boot-1a1b1c1d1e1f\x05local\0'
HEADER = struct.pack('!6H', 0, 0x8400, 0, 1, 0, 0)
A = struct.pack('!HHIH', 1, 0x8001, 120, 4) + bytes([10,0,2,2])

class Mdns(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        (d/'test.c').write_text('#include "ipxe/rbdiscover/rbdiscover_dns.h"\nint parse(const void*p,size_t n,void*out){return rb_mdns_address(p,n,"remote-boot-1a1b1c1d1e1f.local",out);}\nsize_t query(const void*id,void*out){return rb_mdns_query(id,out);}\n')
        subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-shared','-fPIC','-I',str(ROOT),str(d/'test.c'),'-o',str(d/'test.so')], check=True)
        cls.lib = ctypes.CDLL(str(d/'test.so'))
        cls.lib.parse.argtypes = [ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p]
    @classmethod
    def tearDownClass(cls): cls.tmp.cleanup()
    def parse(self, packet):
        out = ctypes.create_string_buffer(4)
        rc = self.lib.parse(packet,len(packet),out)
        return rc, out.raw
    def test_query_exact_hostname_qu_unicast(self):
        out = ctypes.create_string_buffer(64)
        ident = bytes.fromhex('101112131415161718191a1b1c1d1e1f')
        n = self.lib.query(ident,out)
        self.assertEqual(out.raw[:n], struct.pack('!6H',0,0,1,0,0,0)+NAME+b'\0\1\x80\1')
    def test_plain_and_compressed_answer(self):
        self.assertEqual(self.parse(HEADER+NAME+A), (1,b'\x0a\0\2\2'))
        packet = struct.pack('!6H',0,0x8400,1,1,0,0)+NAME+b'\0\1\0\1'+b'\xc0\x0c'+A
        self.assertEqual(self.parse(packet), (1,b'\x0a\0\2\2'))
    def test_untrusted_malformed_packets_never_resolve(self):
        valid = HEADER+NAME+A
        bad = [valid[:i] for i in range(len(valid))]
        bad += [HEADER+b'\xc0\x0c'+A, HEADER+b'\xc0\xff'+A,
                HEADER+b'\xff'+NAME+A, valid+b'\0', valid.replace(b'1a1b',b'2a1b'),
                valid[:-4]+b'\xff'*4, valid[:-4]+b'\0\1\2\3',
                valid[:-4]+b'\xe0\0\0\xfb', valid[:2]+b'\0\0'+valid[4:],
                valid[:6]+b'\xff\xff'+valid[8:]]
        for packet in bad:
            with self.subTest(packet=packet.hex()): self.assertEqual(self.parse(packet)[0],0)
    def test_native_address_and_undefined_sanitizers(self):
        d = Path(self.tmp.name)
        (d/'sanitized.c').write_text(r'''#include "ipxe/rbdiscover/rbdiscover_dns.h"
#include <stdlib.h>
int main(void) {
    uint8_t packet[1024], address[4];
    size_t i, n;
    for (n=0;n<=sizeof(packet);n++) {
        for(i=0;i<sizeof(packet);i++) packet[i]=(uint8_t)rand();
        rb_mdns_address(packet,n,"remote-boot-1a1b1c1d1e1f.local",address);
    }
    memset(packet,0,sizeof(packet));
    packet[2]=0x84; packet[7]=1; packet[12]=0xc0; packet[13]=12;
    rb_mdns_address(packet,32,"remote-boot-1a1b1c1d1e1f.local",address);
    return 0;
}
''')
        subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I',str(ROOT),str(d/'sanitized.c'),'-o',str(d/'sanitized')], check=True)
        subprocess.run([str(d/'sanitized')], check=True)
    def test_bounded_random_input(self):
        import random
        rng = random.Random(17)
        for n in range(1025): self.assertEqual(self.parse(rng.randbytes(n))[0],0)

if __name__ == '__main__': unittest.main()
