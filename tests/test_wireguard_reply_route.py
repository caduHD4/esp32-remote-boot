#!/usr/bin/env python3
"""Exercise production WG endpoint/routing code across deferred IP delivery."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'components/wireguard_lwip/src/wireguardif.c').read_text()
update = source[source.index('static void update_peer_addr('):source.index('static struct wireguard_peer *peer_lookup_by_allowed_ip')]
output = source[source.index('static err_t wireguardif_peer_output_path('):source.index('\t// In magicsock mode, use external UDP output callback')]
output += '\nreturn DIRECT;\n}\nstatic err_t wireguardif_peer_output(struct netif *n,struct pbuf *p,struct wireguard_peer *w){return wireguardif_peer_output_path(n,p,w,false); }\n'
prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define WG_DEBUG(...) ((void)0)
#define ERR_MEM (-1)
#define ERR_RTE (-2)
#define DIRECT 8
#define RELAY 7
typedef int err_t;typedef uint16_t u16_t;
typedef struct {uint32_t addr;} ip_addr_t;
struct wireguard_peer {ip_addr_t ip;u16_t port;bool prefer_derp;uint8_t public_key[32];};
struct pbuf {u16_t tot_len;uint8_t bytes[4];};
typedef err_t (*relay_fn)(const uint8_t*,const uint8_t*,size_t,void*);
struct wireguard_device {bool force_derp_output;relay_fn derp_output_fn;void* derp_output_ctx;};
struct netif {void *state;};
static bool fail_alloc;
static bool ip_addr_isany(const ip_addr_t *ip){return ip->addr==0;}
static void *mem_malloc(size_t n){return fail_alloc?NULL:malloc(n);}
static void mem_free(void *p){free(p);}
static void pbuf_copy_partial(struct pbuf *p,void *out,size_t n,size_t offset){memcpy(out,p->bytes+offset,n);}
static int relayed;
static err_t relay(const uint8_t *key,const uint8_t *data,size_t n,void *ctx){(void)key;(void)ctx;assert(n==4&&data[0]==42);++relayed;return RELAY;}
'''
suffix = r'''
int main(void){
 struct wireguard_device device={.derp_output_fn=relay};struct netif netif={&device};
 struct pbuf data={4,{42,0,0,0}};
 struct wireguard_peer phone={.ip={0x0a00010e},.port=51820};
 struct wireguard_peer desktop={.ip={0x0a00014e},.port=51820};
 const ip_addr_t derp={0},mobile={0xc0000201};
 assert(wireguardif_peer_output(&netif,&data,&phone)==DIRECT);
 // Accepted relayed ingress must govern the later TCP reply, despite cached Wi-Fi.
 update_peer_addr(&phone,&derp,0);
 assert(phone.ip.addr==0x0a00010e&&phone.port==51820);
 assert(wireguardif_peer_output(&netif,&data,&desktop)==DIRECT);
 assert(wireguardif_peer_output(&netif,&data,&phone)==RELAY);
 assert(wireguardif_peer_output(&netif,&data,&phone)==RELAY&&relayed==2);
 // Explicit UDP discovery probes must bypass the preference for this packet only.
 assert(wireguardif_peer_output_path(&netif,&data,&phone,true)==DIRECT);
 assert(wireguardif_peer_output(&netif,&data,&phone)==RELAY);
 // A subsequently accepted direct packet upgrades the same peer again.
 update_peer_addr(&phone,&mobile,12345);
 assert(phone.ip.addr==mobile.addr&&phone.port==12345);
 assert(wireguardif_peer_output(&netif,&data,&phone)==DIRECT);
 device.force_derp_output=true;
 assert(wireguardif_peer_output_path(&netif,&data,&phone,true)==RELAY);
 assert(wireguardif_peer_output(&netif,&data,&phone)==RELAY);
 fail_alloc=true;assert(wireguardif_peer_output(&netif,&data,&phone)==ERR_MEM);fail_alloc=false;
 device.derp_output_fn=NULL;assert(wireguardif_peer_output(&netif,&data,&phone)==ERR_RTE);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='wg-reply-route-') as temp:
    cpp=Path(temp)/'route.c';binary=Path(temp)/'route'
    cpp.write_text(prefix+update+output+suffix)
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
transport = source[source.index('static void wireguardif_process_data_message('):source.index('void wireguardif_network_rx(')]
assert transport.index('wireguard_check_replay') < transport.index('update_peer_addr')
assert 'if (!pbuf->tot_len && wireguard_check_replay(keypair, nonce))' in transport
accepted = transport[transport.index('if (dest_ok) {'):]
assert accepted.index('update_peer_addr') < accepted.index('device->netif->input')
print('PASS: actual WG reply selector survives queued relay replies, separates peers, upgrades direct paths and preserves forced relay/failures')
