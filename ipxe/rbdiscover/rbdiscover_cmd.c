/*
 * The "rbdiscover" command.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <ipxe/errfile.h>
#define ERRFILE_rbdiscover ( ERRFILE_OTHER | 0x00320000 )

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <usr/rbdiscover.h>

/** "rbdiscover" options */
struct rbdiscover_options {};

/** "rbdiscover" option list */
static struct option_descriptor rbdiscover_opts[] = {};

/** "rbdiscover" command descriptor */
static struct command_descriptor rbdiscover_cmd =
	COMMAND_DESC ( struct rbdiscover_options, rbdiscover_opts, 3, 4,
		       "<device_id> <public_key> <cache_ipv4> [timeout_ms]" );

/**
 * The "rbdiscover" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int rbdiscover_exec ( int argc, char **argv ) {
	struct rbdiscover_options opts;
	unsigned int timeout_ms = 10000;
	const char *device_id;
	const char *public_key;
	const char *cache_ipv4;
	char *end;
	int rc;

	if ( ( rc = parse_options ( argc, argv, &rbdiscover_cmd, &opts ) ) != 0 )
		return rc;
	device_id = argv[optind];
	public_key = argv[optind + 1];
	cache_ipv4 = argv[optind + 2];
	if ( argc > ( optind + 3 ) ) {
		timeout_ms = strtoul ( argv[optind + 3], &end, 10 );
		if ( ( *end != '\0' ) || ( timeout_ms == 0 ) )
			return -EINVAL;
	}
	return rbdiscover ( device_id, public_key, cache_ipv4, timeout_ms );
}

/** The "rbdiscover" command */
COMMAND ( rbdiscover, rbdiscover_exec );
