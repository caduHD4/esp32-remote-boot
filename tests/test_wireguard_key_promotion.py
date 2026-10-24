#!/usr/bin/env python3
"""The first accepted next-key packet must mark replay state in promoted storage."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
wg=(root/'components/wireguard_lwip/src/wireguard.c').read_text()
interface=(root/'components/wireguard_lwip/src/wireguardif.c').read_text()
def function(source,name):
 start=source.index(name);brace=source.index('{',start);depth=1;end=brace+1
 while depth:
  if source[end]=='{':depth+=1
  if source[end]=='}':depth-=1
  end+=1
 return source[start:end]+'\n'
functions=''.join(function(wg,name) for name in ('void keypair_destroy(', 'void keypair_update(', 'bool wireguard_check_replay('))
start=interface.index('// Might need to shuffle next key --> current keypair')
end=interface.index('// Check to see if we should rekey',start)
promotion=interface[start:end]
prefix=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
struct wireguard_keypair {bool valid;uint64_t replay_counter;uint32_t replay_bitmap;};
struct wireguard_peer {struct wireguard_keypair curr_keypair,prev_keypair,next_keypair;};
static void crypto_zero(void *p,size_t n){memset(p,0,n);}
'''
suffix=r'''
int main(void){
 struct wireguard_peer peer={0};peer.next_keypair.valid=true;
 struct wireguard_keypair *active=receive_key(&peer,&peer.next_keypair);
 assert(active==&peer.curr_keypair&&active->valid);
 assert(wireguard_check_replay(active,0));
 active=receive_key(&peer,&peer.curr_keypair);
 assert(!wireguard_check_replay(active,0));
 assert(wireguard_check_replay(active,1));
 assert(!wireguard_check_replay(active,1));
 peer.prev_keypair.valid=true;
 assert(receive_key(&peer,&peer.prev_keypair)==&peer.prev_keypair);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='wg-key-promotion-') as temp:
 source=Path(temp)/'promotion.c';binary=Path(temp)/'promotion'
 source.write_text(prefix+functions+'static struct wireguard_keypair *receive_key(struct wireguard_peer *peer,struct wireguard_keypair *keypair){\n'+promotion+'return keypair;\n}\n'+suffix)
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror',str(source),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
print('PASS: actual key promotion/replay code rejects replay of the first next-key packet')
