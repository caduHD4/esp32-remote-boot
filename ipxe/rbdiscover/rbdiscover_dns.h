#ifndef RBDISCOVER_DNS_H
#define RBDISCOVER_DNS_H
/* Bounded mDNS candidate parser. Addresses are UNTRUSTED until signed RBD1 proof.
 * Standalone header so host tests execute the same parser as the EFI image. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline unsigned int rb_dns_u16 ( const uint8_t *p ) {
	return ( ( unsigned int ) p[0] << 8 ) | p[1];
}

/* Decode one DNS name, with bounded compression traversal and output length. */
static inline int rb_dns_name ( const uint8_t *p, size_t n, size_t *offset,
			       char out[254] ) {
	size_t pos = *offset, used = 0, end = 0;
	unsigned int steps;
	for ( steps = 0 ; steps < 128 ; steps++ ) {
		unsigned int len;
		if ( pos >= n )
			return 0;
		len = p[pos++];
		if ( ( len & 0xc0 ) == 0xc0 ) {
			if ( pos >= n )
				return 0;
			if ( ! end )
				end = pos + 1;
			pos = ( ( len & 0x3f ) << 8 ) | p[pos];
			continue;
		}
		if ( len & 0xc0 )
			return 0;
		if ( ! len ) {
			out[used] = '\0';
			*offset = end ? end : pos;
			return 1;
		}
		if ( len > n - pos || len + ( used ? 1 : 0 ) > 253 - used )
			return 0;
		if ( used )
			out[used++] = '.';
		while ( len-- ) {
			uint8_t ch = p[pos++];
			/* Embedded NUL/dot cannot be an alternate spelling of the target. */
			if ( ! ch || ch == '.' )
				return 0;
			out[used++] = ( ch >= 'A' && ch <= 'Z' ) ? ch + 32 : ch;
		}
	}
	return 0;
}

static inline int rb_ipv4_unicast ( const uint8_t *ip ) {
	return ( ip[0] > 0 && ip[0] < 224 && ip[0] != 127 );
}

/* Exact, bounded DNS response; parse all sections before returning a candidate.
 * No dynamic allocation, recursion, or acceptance of unrelated owner names. */
static inline int rb_mdns_address ( const uint8_t *p, size_t n,
				    const char *hostname, uint8_t address[4] ) {
	size_t pos = 12;
	unsigned int q, records, i;
	uint8_t candidate[4];
	int found = 0;
	char name[254];
	if ( n < 12 || n > 512 || p[0] || p[1] ||
	     ( rb_dns_u16 ( p + 2 ) & 0xfa0f ) != 0x8000 )
		return 0;
	q = rb_dns_u16 ( p + 4 );
	records = rb_dns_u16 ( p + 6 ) + rb_dns_u16 ( p + 8 ) + rb_dns_u16 ( p + 10 );
	if ( q > 8 || records > 32 )
		return 0;
	for ( i = 0 ; i < q ; i++ ) {
		if ( ! rb_dns_name ( p, n, &pos, name ) || n - pos < 4 )
			return 0;
		pos += 4;
	}
	for ( i = 0 ; i < records ; i++ ) {
		unsigned int len;
		if ( ! rb_dns_name ( p, n, &pos, name ) || n - pos < 10 )
			return 0;
		len = rb_dns_u16 ( p + pos + 8 );
		if ( len > n - pos - 10 )
			return 0;
		if ( rb_dns_u16 ( p + pos ) == 1 &&
		     ( rb_dns_u16 ( p + pos + 2 ) & 0x7fff ) == 1 &&
		     ( p[pos+4] || p[pos+5] || p[pos+6] || p[pos+7] ) &&
		     len == 4 && ! strcmp ( name, hostname ) &&
		     rb_ipv4_unicast ( p + pos + 10 ) ) {
			memcpy ( candidate, p + pos + 10, 4 );
			found = 1;
		}
		pos += 10 + len;
	}
	if ( pos != n || ! found )
		return 0;
	memcpy ( address, candidate, 4 );
	return 1;
}

/* Legacy-unicast query (ephemeral source port, QU bit) avoids requiring a
 * multicast receive socket. ESPmDNS responds to that source address/port. */
static inline size_t rb_mdns_query ( const uint8_t id[16], uint8_t out[64] ) {
	static const char hex[] = "0123456789abcdef";
	unsigned int i;
	memset ( out, 0, 64 );
	out[5] = 1;
	out[12] = 24;
	memcpy ( out + 13, "remote-boot-", 12 );
	for ( i = 0 ; i < 6 ; i++ ) {
		out[25 + 2*i] = hex[id[10+i] >> 4];
		out[26 + 2*i] = hex[id[10+i] & 15];
	}
	out[37] = 5;
	memcpy ( out + 38, "local", 5 );
	out[45] = 1;
	out[46] = 0x80;
	out[47] = 1;
	return 48;
}
#endif
