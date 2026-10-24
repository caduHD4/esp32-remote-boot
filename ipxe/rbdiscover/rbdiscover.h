#ifndef _USR_RBDISCOVER_H
#define _USR_RBDISCOVER_H

/** @file
 *
 * Remote Boot signed LAN discovery
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

extern int rbdiscover ( const char *device_id_hex, const char *public_key_hex,
			const char *cache_ipv4, unsigned int timeout_ms );

#endif /* _USR_RBDISCOVER_H */
