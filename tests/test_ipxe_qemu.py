#!/usr/bin/env python3
"""OVMF/iPXE integration on an isolated virtual Ethernet (no host LAN).

IPXE_QEMU=1 IPXE_SOURCE=/path/to/pinned/git OVMF_CODE=/path/OVMF_CODE.4m.fd
OVMF_VARS=/path/OVMF_VARS.4m.fd python3 tests/test_ipxe_qemu.py
Requires QEMU, C toolchain and Python cryptography; builds only in a temporary
directory. The synthetic fixture's PUBLIC TEST ONLY scalar 1 signs the replies.
"""
import json
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import tempfile
import threading
import time
import unittest
from cryptography.hazmat.primitives.asymmetric import ec, utils
from cryptography.hazmat.primitives import hashes
ROOT = Path(__file__).resolve().parents[1]
PIN = '7ada3e0f04d0526747df5e0e46c05c94cbf7a7d1'
FIX = json.loads((ROOT/'tests/fixtures/discovery-v1.json').read_text())
PROFILE = FIX['profile']
SERVER = socket.inet_aton('10.0.2.2'); CLIENT=socket.inet_aton('10.0.2.15')
MAC=b'\x52\x54\x00\x12\x34\x02'
NAME=b'\x18remote-boot-1a1b1c1d1e1f\x05local\0'

def checksum(data):
    if len(data)%2: data+=b'\0'
    value=sum(struct.unpack('!%dH'%(len(data)//2), data))
    while value>>16: value=(value&65535)+(value>>16)
    return (~value)&65535

def ip_packet(src,dst,protocol,payload):
    header=struct.pack('!BBHHHBBH4s4s',0x45,0,len(payload)+20,0,0,64,protocol,0,src,dst)
    return header[:10]+struct.pack('!H',checksum(header))+header[12:]+payload

class VirtualLan:
    def __init__(self, mode):
        self.mode=mode; self.frames=[]; self.queries=[]; self.mdns=0; self.http=[]; self.error=None
        self.listener=socket.socket(); self.listener.bind(('127.0.0.1',0)); self.listener.listen(1)
        self.listener.settimeout(.2); self.port=self.listener.getsockname()[1]
        self.stop=threading.Event(); self.conn=None
        self.thread=threading.Thread(target=self.run,daemon=True);self.thread.start()
    def close(self):
        self.stop.set()
        if self.conn:
            try: self.conn.shutdown(socket.SHUT_RDWR)
            except OSError: pass
        self.listener.close(); self.thread.join(2)
        if self.conn:self.conn.close()
    def send(self, dst_mac, etype, packet):
        frame=dst_mac+MAC+struct.pack('!H',etype)+packet
        self.conn.sendall(struct.pack('!I',len(frame))+frame)
    def udp(self,dmac,src,dst,sport,dport,data):
        udp=struct.pack('!HHHH',sport,dport,8+len(data),0)+data
        self.send(dmac,0x800,ip_packet(src,dst,17,udp))
    def tcp(self,dmac,dst,dport,seq,ack,flags,data=b''):
        tcp=struct.pack('!HHIIBBHHH',80,dport,seq,ack,0x50,flags,65535,0,0)+data
        pseudo=SERVER+dst+struct.pack('!BBH',0,6,len(tcp))
        tcp=tcp[:16]+struct.pack('!H',checksum(pseudo+tcp))+tcp[18:]
        self.send(dmac,0x800,ip_packet(SERVER,dst,6,tcp))
    def frame(self, f):
        self.frames.append(f.hex())
        if len(f)<14:return
        mac=f[6:12];typ=f[12:14];p=f[14:]
        if typ==b'\x08\x06' and len(p)>=28 and p[6:8]==b'\0\1':
            # A virtual router, only on this socket-backed L2 network.
            reply=p[:6]+b'\0\2'+MAC+p[24:28]+p[8:14]+p[14:18]
            self.send(mac,0x806,reply);return
        if typ!=b'\x08\0' or len(p)<20:return
        h=(p[0]&15)*4;src=p[12:16];dst=p[16:20];proto=p[9];p=p[h:]
        if proto==17 and len(p)>=8:
            sport,dport,length,_=struct.unpack('!4H',p[:8]);data=p[8:length]
            if dport==67 and len(data)>=240:
                opts=data[240:]; request=b'\x35\x01\x03' in opts
                body=bytearray(data[:240]);body[0]=2;body[16:20]=CLIENT;body[20:24]=SERVER
                options=b'\x35\x01'+bytes([5 if request else 2])+b'\x36\x04'+SERVER+b'\x33\x04\0\0\x0e\x10\x01\x04\xff\xff\xff\0\x03\x04'+SERVER+b'\x06\x04'+SERVER+b'\xff'
                self.udp(b'\xff'*6,SERVER,b'\xff'*4,67,68,bytes(body)+options)
            elif dport==5353 and len(data)>=12:
                self.mdns+=1
                if self.mode in ('mdns','mdns-spoof'):
                    address=SERVER if self.mode=='mdns' else socket.inet_aton('10.0.2.3')
                    answer=struct.pack('!6H',0,0x8400,0,1,0,0)+NAME+struct.pack('!HHIH',1,0x8001,120,4)+address
                    self.udp(mac,SERVER,src,5353,sport,answer)
            elif dport==49192 and len(data)==40:
                self.queries.append((time.monotonic(),socket.inet_ntoa(dst)))
                broadcast=dst in (b'\xff'*4,socket.inet_aton('10.0.2.255'))
                respond=((self.mode in ('cache','invalid-first') and dst==SERVER) or
                         (self.mode=='broadcast' and broadcast) or
                         (self.mode in ('mdns','wrong-key') and dst==SERVER) or
                         (self.mode=='mdns-spoof' and dst==socket.inet_aton('10.0.2.3')))
                if respond:
                    key=ec.derive_private_key(2 if self.mode in ('wrong-key','mdns-spoof') else 1,ec.SECP256R1())
                    # Wrong-key adversary claims the pinned public point but cannot sign for it.
                    reply=bytearray(bytes.fromhex(FIX['reply_hex'])[:113]);reply[8:24]=data[8:24];reply[40:44]=dst if not broadcast else SERVER
                    der=key.sign(reply,ec.ECDSA(hashes.SHA256()));r,s=utils.decode_dss_signature(der)
                    reply+=r.to_bytes(32,'big')+s.to_bytes(32,'big')
                    if self.mode=='invalid-first' and len(self.queries)==1:
                        # Each field mutation is independently RE-SIGNED; a
                        # missing framing/binding guard must cause early HTTP.
                        def sign(prefix, signer=key):
                            rr,ss=utils.decode_dss_signature(signer.sign(prefix,ec.ECDSA(hashes.SHA256())))
                            return prefix+rr.to_bytes(32,'big')+ss.to_bytes(32,'big')
                        prefix=bytes(reply[:113])
                        mutations=[(0,ord('X')),(4,3),(5,1),(7,176),(8,prefix[8]^1),
                                   (24,prefix[24]^1),(40,0),(40,127),(40,224),
                                   (45,81),(47,80),(48,3)]
                        for offset,value in mutations:
                            bad=bytearray(prefix);bad[offset]=value
                            self.udp(mac,bytes(bad[40:44]),src,49192,sport,sign(bytes(bad)))
                        key2=ec.derive_private_key(2,ec.SECP256R1())
                        from cryptography.hazmat.primitives import serialization
                        wrong=prefix[:48]+key2.public_key().public_bytes(serialization.Encoding.X962,serialization.PublicFormat.UncompressedPoint)
                        self.udp(mac,SERVER,src,49192,sport,sign(wrong,key2))
                        self.udp(mac,socket.inet_aton('10.0.2.3'),src,49192,sport,reply)
                        self.udp(mac,SERVER,src,49193,sport,reply)
                        self.udp(mac,SERVER,src,49192,sport,reply[:-1])
                        self.udp(mac,SERVER,src,49192,sport,reply+b'\0')
                    else:
                        self.udp(mac,reply[40:44],src,49192,sport,reply)
        elif proto==6 and len(p)>=20 and dst==SERVER:
            sport,dport,seq,ack=struct.unpack('!HHII',p[:12]);flags=p[13];data=p[(p[12]>>4)*4:]
            if dport!=80:return
            if flags&2:self.tcp(mac,src,sport,10000,seq+1,0x12)
            elif data:
                self.http.append(data)
                script=b'#!ipxe\necho RBDISCOVER_QEMU_CONTINUATION\nexit\n'
                response=b'HTTP/1.1 200 OK\r\nContent-Length: '+str(len(script)).encode()+b'\r\nConnection: close\r\n\r\n'+script
                self.tcp(mac,src,sport,ack,seq+len(data),0x19,response)
    def run(self):
        try:
            while not self.stop.is_set():
                try:self.conn,_=self.listener.accept();break
                except socket.timeout:continue
            if not self.conn:return
            self.conn.settimeout(.2);buffer=b''
            while not self.stop.is_set():
                try: chunk=self.conn.recv(65536)
                except socket.timeout:continue
                if not chunk:break
                buffer+=chunk
                while len(buffer)>=4:
                    n=struct.unpack('!I',buffer[:4])[0]
                    if n>65536:raise ValueError('oversized virtual ethernet frame')
                    if len(buffer)<n+4:break
                    self.frame(buffer[4:n+4]);buffer=buffer[n+4:]
        except (OSError,EOFError) as e:
            if not self.stop.is_set():self.error=e
        except BaseException as e:self.error=e

@unittest.skipUnless(os.environ.get('IPXE_QEMU')=='1','set IPXE_QEMU=1 for isolated OVMF integration')
class QemuDiscovery(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory(prefix='rb-qemu-');cls.work=Path(cls.temp.name)
        cls.logroot=Path(os.environ.get('IPXE_QEMU_LOG_DIR',str(ROOT/'.pio/test-qemu-logs')));cls.logroot.mkdir(parents=True,exist_ok=True)
        cls.src=cls.work/'ipxe';cls.src.mkdir()
        archive=subprocess.run(['git','-C',os.environ['IPXE_SOURCE'],'archive',PIN],capture_output=True,check=True)
        subprocess.run(['tar','-xf','-','-C',str(cls.src)],input=archive.stdout,check=True)
        cls.src=cls.src/'src'
        for name,dest in [('rbdiscover.c','usr/rbdiscover.c'),('rbdiscover_cmd.c','hci/commands/rbdiscover_cmd.c'),('rbdiscover.h','include/usr/rbdiscover.h'),('rbdiscover_dns.h','include/usr/rbdiscover_dns.h')]:
            shutil.copyfile(ROOT/'ipxe/rbdiscover'/name,cls.src/dest)
        with (cls.src/'config/config.c').open('a') as f:f.write('\nREQUIRE_OBJECT ( rbdiscover_cmd );\n')
        (cls.src/'config/local/console.h').write_text('#define CONSOLE_SERIAL\n#undef CONSOLE_EFI\n#undef CONSOLE_FRAMEBUFFER\n')
        # Test-only command appended to private copy: invokes actual iPXE ECDSA
        # implementation on canonical shared P1363 fixture, then a mutation.
        packet=', '.join('0x'+FIX['reply_hex'][i:i+2] for i in range(0,len(FIX['reply_hex']),2))
        with (cls.src/'usr/rbdiscover.c').open('a') as f:
            f.write('\n#include <ipxe/command.h>\nstatic int rbfixture_exec(int argc __unused,char **argv __unused){\nuint8_t reply[]={'+packet+'};\nif(rbdiscover_verify(reply)) return -EINVAL;\nreply[113]^=1;\nif(!rbdiscover_verify(reply)) return -EINVAL;\nprintf("RBDISCOVER_SHARED_FIXTURE_OK\\n"); return 0;}\nCOMMAND(rbfixture,rbfixture_exec);\n')
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def case(self,mode,success,cache='10.0.2.99'):
        script=(ROOT/'ipxe/remote-boot-blind.ipxe.in').read_text()
        for a,b in {'@DEVICE_ID@':PROFILE['device_id'],'@PUBLIC_KEY@':PROFILE['public_key'],'@LAST_IPV4@':cache,'@PC_ID@':PROFILE['pc_id'],'@TIMEOUT@':'3000'}.items():script=script.replace(a,b)
        script=script.replace('ifconf --','rbfixture || goto failed\nifconf --').replace('dhcp --timeout 3000','dhcp --timeout 10000')
        embed=self.work/'test.ipxe';embed.write_text(script)
        build=subprocess.run(['make','-C',str(self.src),'-j'+os.environ.get('JOBS','4'),'bin-x86_64-efi/ipxe.efi','EMBED='+str(embed),'NO_WERROR=1'],capture_output=True)
        (self.logroot/(mode+'-build.log')).write_bytes(build.stdout+build.stderr)
        self.assertEqual(build.returncode,0,(build.stdout+build.stderr)[-3000:])
        disk=self.work/'esp';(disk/'EFI/BOOT').mkdir(parents=True,exist_ok=True)
        shutil.copyfile(self.src/'bin-x86_64-efi/ipxe.efi',disk/'EFI/BOOT/BOOTX64.EFI')
        varsfile=self.work/'vars.fd';shutil.copyfile(os.environ['OVMF_VARS'],varsfile)
        serial=self.logroot/(mode+'-serial.log')
        serial.unlink(missing_ok=True)
        lan=VirtualLan(mode)
        args=[os.environ.get('QEMU','qemu-system-x86_64'),'-machine','q35,accel=tcg','-m','256','-display','none','-serial','file:'+str(serial),'-monitor','none','-no-reboot',
              '-drive','if=pflash,format=raw,readonly=on,file='+os.environ['OVMF_CODE'],'-drive','if=pflash,format=raw,file='+str(varsfile),
              '-drive','format=raw,file=fat:rw:'+str(disk),'-netdev','socket,id=n,connect=127.0.0.1:'+str(lan.port),'-device','e1000,netdev=n,romfile=']
        proc=subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        completed_at=None
        try:
            deadline=time.monotonic()+35
            while time.monotonic()<deadline and proc.poll() is None:
                log=serial.read_text(errors='replace') if serial.exists() else ''
                if 'RBDISCOVER_QEMU_CONTINUATION' in log or 'Remote Boot discovery or loader failure' in log:
                    completed_at=time.monotonic()
                    break
                time.sleep(.1)
        finally:
            proc.terminate()
            try:stdout,stderr=proc.communicate(timeout=3)
            except subprocess.TimeoutExpired:proc.kill();stdout,stderr=proc.communicate()
            lan.close()
        (self.logroot/(mode+'-qemu.log')).write_bytes(stdout+stderr)
        completion_elapsed=(completed_at-lan.queries[0][0]) if completed_at is not None and lan.queries else None
        details={'completion_elapsed_s':completion_elapsed,'frames':lan.frames,'queries':lan.queries,'mdns':lan.mdns,'http':[x.decode(errors='replace') for x in lan.http],'error':str(lan.error)}
        (self.logroot/(mode+'-network.json')).write_text(json.dumps(details,indent=2))
        log=serial.read_text(errors='replace')
        self.assertIsNone(lan.error,details)
        self.assertIn('RBDISCOVER_SHARED_FIXTURE_OK',log)
        self.assertTrue(lan.queries,log)
        self.assertEqual(lan.queries[0][1],cache)
        if success:
            self.assertIn('RBDISCOVER_QEMU_CONTINUATION',log,details)
            self.assertIn('/boot/'+PROFILE['pc_id']+'.ipxe',b''.join(lan.http).decode())
        else:
            self.assertFalse(lan.http,details)
            self.assertIn('Remote Boot discovery or loader failure',log)
            self.assertIsNotNone(completion_elapsed,details)
            # Measure command completion, not merely when retries stop. The
            # serial observer adds at most one100ms polling interval.
            self.assertGreaterEqual(completion_elapsed,2.8,details)
            self.assertLess(completion_elapsed,3.5,details)
        self.assertLess(lan.queries[-1][0]-lan.queries[0][0],3.5)
        if mode.startswith('mdns'):self.assertGreater(lan.mdns,0)
        if mode=='invalid-first':self.assertGreater(lan.queries[-1][0]-lan.queries[0][0],.4)
    def test_01_cache_signature_and_continuation(self):self.case('cache',True,'10.0.2.2')
    def test_02_changed_address_broadcast(self):self.case('broadcast',True)
    def test_03_changed_address_mdns_when_broadcast_blocked(self):self.case('mdns',True)
    def test_04_wrong_signature_rejected(self):self.case('wrong-key',False,'10.0.2.2')
    def test_05_mdns_spoof_is_only_untrusted_candidate(self):self.case('mdns-spoof',False)
    def test_06_no_response_total_deadline(self):self.case('none',False)
    def test_07_signed_invalid_framing_then_valid(self):self.case('invalid-first',True,'10.0.2.2')
if __name__=='__main__':unittest.main()
