#!/usr/bin/env python3
"""Exercise the real iPXE receive handler across a synchronous crypto deadline.

Only its clock/crypto/storage/I/O boundaries are adapted; the handler itself is
extracted unchanged from the checked-in extension. Crypto interoperability is
covered separately by the real iPXE/OVMF fixture test.
"""
import ctypes
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
FIXTURE = json.loads((ROOT/'tests/fixtures/discovery-v1.json').read_text())

class Result(ctypes.Structure):
    _fields_ = [('stores',ctypes.c_int),('verifies',ctypes.c_int),
                ('closes',ctypes.c_int),('frees',ctypes.c_int),
                ('finished',ctypes.c_ulong)]

class Deadline(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        d = Path(cls.temp.name)
        source = (ROOT/'ipxe/rbdiscover/rbdiscover.c').read_text()
        start = source.index('static int rbdiscover_deliver (')
        end = source.index('/** Discovery data transfer operations */', start)
        handler = source[start:end]
        fixture = ','.join(str(b) for b in bytes.fromhex(FIXTURE['reply_hex']))
        adapter = r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <arpa/inet.h>
#include "ipxe/rbdiscover/rbdiscover_dns.h"
#define RBDISCOVER_PORT 49192
#define RBDISCOVER_REPLY_SIZE 177
struct io_buffer { void *data; size_t len; };
struct xfer_metadata { struct sockaddr *src; };
struct rbdiscover {
    unsigned long deadline;
    uint8_t nonce[16], device_id[16], public_key[65];
    unsigned int candidates;
    struct in_addr candidate;
    char hostname[31];
    uint8_t query[40];
};
struct result { int stores, verifies, closes, frees; unsigned long finished; };
static struct result result;
static unsigned long now, crypto_ticks;
static int crypto_rc;
static unsigned long currticks(void) { return now; }
static size_t iob_len(struct io_buffer *p) { return p->len; }
static void free_iob(struct io_buffer *p) { (void)p; result.frees++; }
static int rbdiscover_verify(const uint8_t *p) {
    (void)p; result.verifies++; now += crypto_ticks; return crypto_rc;
}
static int rbdiscover_store(const void *p) { (void)p; result.stores++; return 0; }
static void rbdiscover_close(struct rbdiscover *p,int rc) {
    (void)p; (void)rc; result.closes++;
}
static int rbdiscover_send_to(struct rbdiscover *p,struct in_addr a,
    unsigned int port,const void *data,size_t len,void *netdev) {
    (void)p; (void)a; (void)port; (void)data; (void)len; (void)netdev; return 0;
}
'''
        driver = r'''
void run(unsigned long start,unsigned long deadline,unsigned long duration,
         int verification_rc,struct result *out) {
    uint8_t fixture[]={FIXTURE};
    struct rbdiscover rb;
    struct sockaddr_in peer;
    struct io_buffer iob={fixture,sizeof(fixture)};
    struct xfer_metadata meta={(struct sockaddr *)&peer};
    memset(&result,0,sizeof(result)); memset(&rb,0,sizeof(rb));
    memset(&peer,0,sizeof(peer));
    rb.deadline=deadline;
    memcpy(rb.nonce,fixture+8,16); memcpy(rb.device_id,fixture+24,16);
    memcpy(rb.public_key,fixture+48,65);
    peer.sin_family=AF_INET; peer.sin_port=htons(RBDISCOVER_PORT);
    memcpy(&peer.sin_addr,fixture+40,4);
    now=start; crypto_ticks=duration; crypto_rc=verification_rc;
    rbdiscover_deliver(&rb,&iob,&meta);
    result.finished=now; *out=result;
}
'''.replace('FIXTURE',fixture)
        (d/'handler.c').write_text(adapter+handler+driver)
        subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-shared','-fPIC',
                        '-I',str(ROOT),str(d/'handler.c'),'-o',str(d/'handler.so')],check=True)
        cls.lib=ctypes.CDLL(str(d/'handler.so'))
        cls.lib.run.argtypes=[ctypes.c_ulong,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_int,ctypes.POINTER(Result)]
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_handler(self,start,deadline,duration,rc=0):
        result=Result();self.lib.run(start,deadline,duration,rc,ctypes.byref(result))
        self.assertEqual(result.frees,1)
        return result
    def test_valid_verification_finishes_before_deadline(self):
        r=self.run_handler(90,100,9)
        self.assertEqual((r.verifies,r.stores,r.closes,r.finished),(1,1,1,99))
    def test_verification_crosses_deadline_without_mutating_result(self):
        r=self.run_handler(99,100,2)
        self.assertEqual((r.verifies,r.stores,r.closes,r.finished),(1,0,0,101))
    def test_verification_finishes_exactly_at_deadline(self):
        r=self.run_handler(99,100,1)
        self.assertEqual((r.verifies,r.stores,r.closes,r.finished),(1,0,0,100))
    def test_expired_packet_never_starts_crypto(self):
        r=self.run_handler(100,100,0)
        self.assertEqual((r.verifies,r.stores,r.closes),(0,0,0))
    def test_invalid_signature_cannot_mutate_result(self):
        r=self.run_handler(90,100,1,-1)
        self.assertEqual((r.verifies,r.stores,r.closes),(1,0,0))
    def test_elapsed_comparison_survives_tick_wrap(self):
        maximum=2**(ctypes.sizeof(ctypes.c_ulong)*8)-1
        r=self.run_handler(maximum-5,4,10)
        self.assertEqual((r.verifies,r.stores,r.closes,r.finished),(1,0,0,4))
if __name__=='__main__':unittest.main()
