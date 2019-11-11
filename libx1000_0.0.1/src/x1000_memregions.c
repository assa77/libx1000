#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <string.h>
#include "x1000_memregions.h"
#include "x1000_config.h"


static struct dseg *pself_layout = NULL;


static int read_mapping( FILE *mapfile,
	long long unsigned *addr,
	long long unsigned *endaddr,
	char *permissions,
	long long unsigned *offset,
	char *device,
	long long unsigned *inode,
	char *filename )
{
	char fmap[ PATH_MAX + 20 ];
	int ret;

	if ( !fgets( fmap, PATH_MAX + 20, mapfile ) )
		return -1;

	*addr = *endaddr = *offset = *inode = 0;
	*permissions = *device = *filename = '\0';

	if ( ( ret = sscanf( fmap, "%llx-%llx %s %llx %s %llx %s",
		addr, endaddr, permissions, offset, device, inode, filename ) ) == EOF )
		return -1;

	return ret;
}

static int free_self_layout( )
{
	struct dseg *pnext, *tmp;

	for ( pnext = pself_layout; pnext; pnext = tmp )
	{
		tmp = pnext->next;
		free( pnext );
	}
	pself_layout = NULL;

	return 0;
}

__attribute__((destructor))
static void _free_self_layout( )
{
	/* for neatness sake, free memory on unload. */
	free_self_layout ( );
}

static int init_self_layout( )
{
	FILE *f = 0;
	long long unsigned addr, endaddr, offset, inode;
	char permissions[ 10 ];
	char device[ 10 ];
	char filename[ PATH_MAX ];
	struct dseg **pnext;
	int ret;

	if ( !( f = fopen( "/proc/self/maps", "r" ) ) )
		return -1;

	pnext = &pself_layout;

	while ( ( ret = read_mapping( f, &addr, &endaddr, permissions,
		&offset, device, &inode, filename ) ) != -1 )
	{
		/*
		 * Find everything with rw permissions
		 */
		if ( !strcmp( permissions, "rw-p" ) )
		{
			/* found a data segement */
			( *pnext ) = ( struct dseg * )malloc( sizeof( struct dseg ) );

			( *pnext )->data = addr;
			( *pnext )->size = endaddr - addr;

			if ( inode )
				( *pnext )->type = 1 << ( X1000_LOCKMEM_DATASEG - 1 );
			else if ( *filename == '[' )
				( *pnext )->type = 1 << ( X1000_LOCKMEM_HEAPSTACK - 1 );
			else
				( *pnext )->type = 1 << ( X1000_LOCKMEM_MMAP - 1 );

			( *pnext )->next = NULL;

			pnext = &( *pnext )->next;
		}
	}

	fclose( f );

	return 0;
}

int get_mlayout( struct dseg **playout )
{
	/* free previous queries. */
	if ( pself_layout )
		free_self_layout( );

	if ( init_self_layout( ) )
		return -1;

	*playout = pself_layout;

	return 0;
}
