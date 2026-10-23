/*
 * Remote Boot signed LAN discovery for iPXE.
 *
 * This module is not part of upstream iPXE. It is copied into a task-owned
 * build worktree by ipxe/build.sh.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <ipxe/errfile.h>
#define ERRFILE_rbdiscover ( ERRFILE_OTHER | 0x00320000 )

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/refcnt.h>
#include <ipxe/iobuf.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/retry.h>
#include <ipxe/timer.h>
#include <ipxe/tcpip.h>
#include <ipxe/in.h>
#include <ipxe/ip.h>
#include <ipxe/socket.h>
#include <ipxe/monojob.h>
#include <ipxe/settings.h>
#include <ipxe/random_nz.h>
#include <ipxe/sha256.h>
#include <ipxe/crypto.h>
#include <ipxe/ecdsa.h>
#include <ipxe/asn1.h>
#include <usr/rbdiscover.h>
#include <usr/rbdiscover_dns.h>

/** Discovery UDP port */
#define RBDISCOVER_PORT 49192
/** Query size */
#define RBDISCOVER_QUERY_SIZE 40
/** Reply size */
#define RBDISCOVER_REPLY_SIZE 177
/** Signed prefix size */
#define RBDISCOVER_SIGNED_SIZE 113
/** Retry interval in ticks */
#define RBDISCOVER_RETRY ( TICKS_PER_SEC / 2 )

/** A discovery attempt */
struct rbdiscover {
	/** Reference count */
	struct refcnt refcnt;
	/** Job control interface */
	struct interface job;
	/** Data transfer interface */
	struct interface xfer;
	/** Retransmission timer */
	struct retry_timer timer;
	/** Pinned device ID */
	uint8_t device_id[16];
	/** Pinned public key (SEC1 uncompressed) */
	uint8_t public_key[65];
	/** Cached IPv4 address */
	struct in_addr cache;
	/** Configured local IPv4: multicast must never originate from 0.0.0.0. */
	struct in_addr local;
	/** Query nonce */
	uint8_t nonce[16];
	/** Query packet */
	uint8_t query[RBDISCOVER_QUERY_SIZE];
	/** Overall deadline in ticks */
	unsigned long deadline;
	/** First packet sent */
	int started;
	/** Retry round: cache first, then bounded LAN fallbacks. */
	unsigned int round;
	/** mDNS supplies a candidate only, never an authenticated address. */
	struct in_addr candidate;
	unsigned int candidates;
	char hostname[31];
	uint8_t dns_query[64];
	size_t dns_query_len;
};

/** Result setting name */
static char rbdiscover_setting[] = "rb_ip";

/**
 * Close discovery attempt
 *
 * @v rb		Discovery attempt
 * @v rc		Status code
 */
static void rbdiscover_close ( struct rbdiscover *rb, int rc ) {
	stop_timer ( &rb->timer );
	intf_shutdown ( &rb->xfer, rc );
	intf_shutdown ( &rb->job, rc );
}

/**
 * Send discovery query
 *
 * @v rb		Discovery attempt
 * @ret rc		Return status code
 */
static int rbdiscover_send_to ( struct rbdiscover *rb, struct in_addr address,
				unsigned int port, const void *data, size_t len,
				struct net_device *netdev ) {
	struct sockaddr_in dest;
	struct xfer_metadata meta;
	memset ( &dest, 0, sizeof ( dest ) );
	dest.sin_family = AF_INET;
	dest.sin_port = htons ( port );
	dest.sin_addr = address;
	memset ( &meta, 0, sizeof ( meta ) );
	meta.dest = ( struct sockaddr * ) &dest;
	meta.netdev = netdev;
	return xfer_deliver_raw_meta ( &rb->xfer, data, len, &meta );
}

/* The first round is cache-only. Later rounds also use limited/directed LAN
 * broadcasts and mDNS. A failed stale-cache ARP/send is not fatal. */
static int rbdiscover_send ( struct rbdiscover *rb ) {
	struct ipv4_miniroute *route;
	struct in_addr address;
	unsigned int routes = 0;
	rbdiscover_send_to ( rb, rb->cache, RBDISCOVER_PORT,
			     rb->query, sizeof ( rb->query ), NULL );
	if ( rb->round++ == 0 )
		return 0;
	if ( rb->candidate.s_addr )
		rbdiscover_send_to ( rb, rb->candidate, RBDISCOVER_PORT,
				     rb->query, sizeof ( rb->query ), NULL );
	/* Cap route iteration even on machines with many interfaces. */
	list_for_each_entry ( route, &ipv4_miniroutes, list ) {
		if ( ++routes > 8 )
			break;
		if ( route->address.s_addr != rb->local.s_addr || route->gateway.s_addr )
			continue;
		address.s_addr = INADDR_BROADCAST;
		rbdiscover_send_to ( rb, address, RBDISCOVER_PORT, rb->query,
				     sizeof ( rb->query ), route->netdev );
		if ( route->netmask.s_addr &&
		     route->netmask.s_addr != INADDR_BROADCAST ) {
			address.s_addr = route->address.s_addr | ~route->netmask.s_addr;
			rbdiscover_send_to ( rb, address, RBDISCOVER_PORT, rb->query,
					     sizeof ( rb->query ), route->netdev );
		}
		inet_aton ( "224.0.0.251", &address );
		rbdiscover_send_to ( rb, address, 5353, rb->dns_query,
				     rb->dns_query_len, route->netdev );
	}
	return 0;
}

static void rbdiscover_retry ( struct rbdiscover *rb ) {
	unsigned long now = currticks();
	unsigned long remaining;
	if ( ( long ) ( now - rb->deadline ) >= 0 ) {
		rbdiscover_close ( rb, -ETIMEDOUT );
		return;
	}
	remaining = rb->deadline - now;
	start_timer_fixed ( &rb->timer,
		( remaining < RBDISCOVER_RETRY ) ? remaining : RBDISCOVER_RETRY );
}

/**
 * Handle data transfer window change
 *
 * @v rb		Discovery attempt
 */
static void rbdiscover_window_changed ( struct rbdiscover *rb ) {
	if ( rb->started || ( xfer_window ( &rb->xfer ) == 0 ) )
		return;
	rb->started = 1;
	/* The overall deadline was set before opening the socket. */
	if ( rbdiscover_send ( rb ) != 0 ) {
		rbdiscover_close ( rb, -EIO );
		return;
	}
	rbdiscover_retry ( rb );
}

/**
 * Handle timer expiry
 *
 * @v timer		Retry timer
 * @v over		Failure indicator
 */
static void rbdiscover_expired ( struct retry_timer *timer, int over ) {
	struct rbdiscover *rb =
		container_of ( timer, struct rbdiscover, timer );

	if ( over || ( ( long ) ( currticks() - rb->deadline ) >= 0 ) ) {
		rbdiscover_close ( rb, -ETIMEDOUT );
		return;
	}
	if ( ! rb->started ) {
		if ( xfer_window ( &rb->xfer ) == 0 ) {
			rbdiscover_retry ( rb );
			return;
		}
		rb->started = 1;
	}
	if ( rbdiscover_send ( rb ) != 0 ) {
		rbdiscover_close ( rb, -EIO );
		return;
	}
	rbdiscover_retry ( rb );
}

/**
 * Convert an IEEE-P1363 r||s signature to DER
 *
 * @v sig		P1363 signature (64 bytes)
 * @v der		DER output buffer (at least 72 bytes)
 * @ret len		DER length
 */
static int rbdiscover_signature_der ( const uint8_t *sig, uint8_t *der ) {
	const uint8_t *r = sig;
	const uint8_t *s = sig + 32;
	int rlen = 32;
	int slen = 32;
	int rpad;
	int spad;
	int len;
	int pos = 0;

	while ( ( rlen > 1 ) && ( r[0] == 0 ) ) {
		r++;
		rlen--;
	}
	while ( ( slen > 1 ) && ( s[0] == 0 ) ) {
		s++;
		slen--;
	}
	rpad = ( r[0] & 0x80 ) ? 1 : 0;
	spad = ( s[0] & 0x80 ) ? 1 : 0;
	len = 2 + rlen + rpad + 2 + slen + spad;
	der[pos++] = 0x30;
	der[pos++] = len;
	der[pos++] = 0x02;
	der[pos++] = rlen + rpad;
	if ( rpad )
		der[pos++] = 0;
	memcpy ( der + pos, r, rlen );
	pos += rlen;
	der[pos++] = 0x02;
	der[pos++] = slen + spad;
	if ( spad )
		der[pos++] = 0;
	memcpy ( der + pos, s, slen );
	pos += slen;
	return pos;
}

/**
 * Verify a signed discovery reply
 *
 * @v reply		Reply packet
 * @ret rc		Return status code
 */
static int rbdiscover_verify ( const uint8_t *reply ) {
	static const uint8_t prefix[] = {
		0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86,
		0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a,
		0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
		0x42, 0x00, 0x04,
	};
	uint8_t key[ sizeof ( prefix ) + 64 ];
	uint8_t der[72];
	struct sha256_digest_data ctx;
	uint8_t hash[SHA256_DIGEST_SIZE];
	struct asn1_cursor key_cursor;
	struct asn1_cursor sig_cursor;
	int siglen;
	int rc;

	memcpy ( key, prefix, sizeof ( prefix ) );
	memcpy ( key + sizeof ( prefix ), reply + 49, 64 );
	key_cursor.data = key;
	key_cursor.len = sizeof ( key );
	digest_init ( &sha256_algorithm, &ctx );
	digest_update ( &sha256_algorithm, &ctx, reply, RBDISCOVER_SIGNED_SIZE );
	digest_final ( &sha256_algorithm, &ctx, hash );
	siglen = rbdiscover_signature_der ( reply + 113, der );
	sig_cursor.data = der;
	sig_cursor.len = siglen;
	rc = pubkey_verify ( &ecdsa_algorithm, &key_cursor, &sha256_algorithm,
			     hash, &sig_cursor );
	if ( rc != 0 )
		DBGC ( reply, "RBDISCOVER signature rejected: %s\n", strerror ( rc ) );
	return rc;
}

/**
 * Store the verified IPv4 address
 *
 * @v address		IPv4 address (network byte order)
 * @ret rc		Return status code
 */
static int rbdiscover_store ( const void *address ) {
	struct settings *settings;
	struct setting setting;
	int rc;

	memset ( &setting, 0, sizeof ( setting ) );
	if ( ( rc = parse_setting_name ( rbdiscover_setting,
					 autovivify_child_settings, &settings,
					 &setting ) ) != 0 )
		return rc;
	setting.type = &setting_type_ipv4;
	return store_setting ( settings, &setting, address, address ? sizeof ( struct in_addr ) : 0 );
}

/**
 * Handle discovery reply
 *
 * @v rb		Discovery attempt
 * @v iobuf		I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int rbdiscover_deliver ( struct rbdiscover *rb, struct io_buffer *iobuf,
				struct xfer_metadata *meta ) {
	const uint8_t *reply = iobuf->data;
	struct sockaddr *src = meta->src;
	struct sockaddr_in *sin;
	int rc;

	/* Expired replies cannot revive a previous attempt. */
	if ( ( long ) ( currticks() - rb->deadline ) >= 0 )
		goto ignore;
	if ( src && src->sa_family == AF_INET &&
	     ntohs ( ( ( struct sockaddr_in * ) src )->sin_port ) == 5353 ) {
		struct in_addr candidate;
		if ( rb->candidates < 4 &&
		     rb_mdns_address ( reply, iob_len ( iobuf ), rb->hostname,
				       ( uint8_t * ) &candidate ) &&
		     candidate.s_addr != rb->candidate.s_addr ) {
			rb->candidate = candidate;
			rb->candidates++;
			rbdiscover_send_to ( rb, candidate, RBDISCOVER_PORT,
					     rb->query, sizeof ( rb->query ), NULL );
		}
		goto ignore;
	}
	if ( iob_len ( iobuf ) != RBDISCOVER_REPLY_SIZE )
		goto ignore;
	if ( ( memcmp ( reply, "RBD1", 4 ) != 0 ) || ( reply[4] != 2 ) ||
	     ( reply[5] != 0 ) || ( reply[6] != 0 ) || ( reply[7] != 177 ) )
		goto ignore;
	if ( memcmp ( reply + 8, rb->nonce, sizeof ( rb->nonce ) ) != 0 )
		goto ignore;
	if ( memcmp ( reply + 24, rb->device_id, sizeof ( rb->device_id ) ) != 0 )
		goto ignore;
	if ( reply[48] != 0x04 )
		goto ignore;
	if ( memcmp ( reply + 48, rb->public_key, sizeof ( rb->public_key ) ) != 0 )
		goto ignore;
	if ( ! rb_ipv4_unicast ( reply + 40 ) )
		goto ignore;
	if ( ( reply[44] != 0 ) || ( reply[45] != 80 ) ||
	     ( reply[46] != 0 ) || ( reply[47] != 81 ) )
		goto ignore;
	if ( ( src == NULL ) || ( src->sa_family != AF_INET ) )
		goto ignore;
	sin = ( struct sockaddr_in * ) src;
	if ( ntohs ( sin->sin_port ) != RBDISCOVER_PORT )
		goto ignore;
	if ( memcmp ( &sin->sin_addr, reply + 40, sizeof ( sin->sin_addr ) ) != 0 )
		goto ignore;
	if ( rbdiscover_verify ( reply ) != 0 )
		goto ignore;
	rc = rbdiscover_store ( reply + 40 );
	rbdiscover_close ( rb, rc );
	free_iob ( iobuf );
	return 0;

 ignore:
	free_iob ( iobuf );
	return 0;
}

/** Discovery data transfer operations */
static struct interface_operation rbdiscover_xfer_op[] = {
	INTF_OP ( xfer_deliver, struct rbdiscover *, rbdiscover_deliver ),
	INTF_OP ( xfer_window_changed, struct rbdiscover *,
		  rbdiscover_window_changed ),
	INTF_OP ( intf_close, struct rbdiscover *, rbdiscover_close ),
};

/** Discovery data transfer descriptor */
static struct interface_descriptor rbdiscover_xfer_desc =
	INTF_DESC_PASSTHRU ( struct rbdiscover, xfer, rbdiscover_xfer_op, job );

/** Discovery job operations */
static struct interface_operation rbdiscover_job_op[] = {
	INTF_OP ( intf_close, struct rbdiscover *, rbdiscover_close ),
};

/** Discovery job descriptor */
static struct interface_descriptor rbdiscover_job_desc =
	INTF_DESC_PASSTHRU ( struct rbdiscover, job, rbdiscover_job_op, xfer );

/**
 * Parse lowercase/uppercase hexadecimal
 *
 * @v text		Text
 * @v data		Output buffer
 * @v len		Output length
 * @ret rc		Return status code
 */
static int rbdiscover_hex ( const char *text, uint8_t *data, size_t len ) {
	size_t i;
	int high;
	int low;

	if ( strlen ( text ) != ( len * 2 ) )
		return -EINVAL;
	for ( i = 0 ; i < len ; i++ ) {
		high = ( text[ i * 2 ] >= '0' && text[ i * 2 ] <= '9' ) ?
			( text[ i * 2 ] - '0' ) :
			( ( text[ i * 2 ] | 0x20 ) >= 'a' && ( text[ i * 2 ] | 0x20 ) <= 'f' ) ?
			( ( text[ i * 2 ] | 0x20 ) - 'a' + 10 ) : -1;
		low = ( text[ i * 2 + 1 ] >= '0' && text[ i * 2 + 1 ] <= '9' ) ?
			( text[ i * 2 + 1 ] - '0' ) :
			( ( text[ i * 2 + 1 ] | 0x20 ) >= 'a' && ( text[ i * 2 + 1 ] | 0x20 ) <= 'f' ) ?
			( ( text[ i * 2 + 1 ] | 0x20 ) - 'a' + 10 ) : -1;
		if ( ( high < 0 ) || ( low < 0 ) )
			return -EINVAL;
		data[i] = ( high << 4 ) | low;
	}
	return 0;
}

/**
 * Discover the current ESP IPv4 using a pinned identity
 *
 * @v device_id_hex	Pinned device ID (32 hex)
 * @v public_key_hex	Pinned SEC1 public key (130 hex)
 * @v cache_ipv4	Cached IPv4 address
 * @v timeout_ms	Total discovery budget in milliseconds
 * @ret rc		Return status code
 */
int rbdiscover ( const char *device_id_hex, const char *public_key_hex,
		const char *cache_ipv4, unsigned int timeout_ms ) {
	struct rbdiscover *rb;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
	} peer;
	struct sockaddr_in local;
	struct ipv4_miniroute *route;
	unsigned int routes = 0;
	unsigned long ticks;
	int rc;

	rb = zalloc ( sizeof ( *rb ) );
	if ( ! rb )
		return -ENOMEM;
	ref_init ( &rb->refcnt, NULL );
	intf_init ( &rb->job, &rbdiscover_job_desc, &rb->refcnt );
	intf_init ( &rb->xfer, &rbdiscover_xfer_desc, &rb->refcnt );
	timer_init ( &rb->timer, rbdiscover_expired, &rb->refcnt );
	if ( ( rc = rbdiscover_hex ( device_id_hex, rb->device_id,
				     sizeof ( rb->device_id ) ) ) != 0 )
		goto err;
	if ( ( rc = rbdiscover_hex ( public_key_hex, rb->public_key,
				     sizeof ( rb->public_key ) ) ) != 0 )
		goto err;
	if ( ( rb->public_key[0] != 0x04 ) ||
	     ( ! inet_aton ( cache_ipv4, &rb->cache ) ) ) {
		rc = -EINVAL;
		goto err;
	}
	if ( ! rb_ipv4_unicast ( ( const uint8_t * ) &rb->cache ) ) {
		rc = -EINVAL;
		goto err;
	}
	rb->dns_query_len = rb_mdns_query ( rb->device_id, rb->dns_query );
	memcpy ( rb->hostname, rb->dns_query + 13, 24 );
	memcpy ( rb->hostname + 24, ".local", 7 );
	if ( ( rc = rbdiscover_store ( NULL ) ) != 0 )
		goto err;
	if ( get_random_nz ( rb->nonce, sizeof ( rb->nonce ) ) != 0 ) {
		rc = -EIO;
		goto err;
	}
	memcpy ( rb->query, "RBD1", 4 );
	rb->query[4] = 1;
	rb->query[5] = 0;
	rb->query[6] = 0;
	rb->query[7] = RBDISCOVER_QUERY_SIZE;
	memcpy ( rb->query + 8, rb->nonce, sizeof ( rb->nonce ) );
	memcpy ( rb->query + 24, rb->device_id, sizeof ( rb->device_id ) );
	if ( timeout_ms < 1000 )
		timeout_ms = 1000;
	if ( timeout_ms > 60000 )
		timeout_ms = 60000;
	ticks = ( ( unsigned long ) timeout_ms * TICKS_PER_SEC ) / 1000;
	rb->deadline = currticks() + ticks;
	memset ( &peer, 0, sizeof ( peer ) );
	peer.sin.sin_family = AF_INET;
	peer.sin.sin_port = htons ( RBDISCOVER_PORT );
	peer.sin.sin_addr = rb->cache;
	/* UDP's ephemeral port remains library-owned, but binding its address is
	 * essential: iPXE deliberately has no multicast route from which to infer
	 * a source IPv4. The script configures net0 before entering discovery. */
	list_for_each_entry ( route, &ipv4_miniroutes, list ) {
		if ( ++routes > 8 )
			break;
		if ( rb_ipv4_unicast ( ( const uint8_t * ) &route->address ) ) {
			rb->local = route->address;
			break;
		}
	}
	if ( ! rb->local.s_addr ) {
		rc = -ENETUNREACH;
		goto err;
	}
	memset ( &local, 0, sizeof ( local ) );
	local.sin_family = AF_INET;
	local.sin_addr = rb->local;
	if ( ( rc = xfer_open_socket ( &rb->xfer, SOCK_DGRAM, &peer.sa,
				       ( struct sockaddr * ) &local ) ) != 0 ) {
		DBGC ( rb, "RBDISCOVER %p could not open socket: %s\n",
		       rb, strerror ( rc ) );
		goto err;
	}
	intf_plug_plug ( &rb->job, &monojob );
	ref_put ( &rb->refcnt );
	start_timer_fixed ( &rb->timer, 0 );
	return monojob_wait ( NULL, 0 );

 err:
	ref_put ( &rb->refcnt );
	return rc;
}
