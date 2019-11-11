#include <stdint.h>
#include <stdio.h>
#include "x1000_memregions.h"

#define TO_UINT32( __x )	( ( uint32_t )( __x ) )

int main( int argc, char *argv[ ] )
{
	struct dseg *next;

	if ( get_mlayout( &next ) < 0 )
		printf( "Failed to get memory layout\n" );

	for ( ; next; next = next->next )
	{
		printf( "Data,Type,Size: 0x%08x,%lu,%llu\n",
			TO_UINT32( next->data ), ( unsigned long )next->type,
			( unsigned long long )next->size );
	}

	return 0;
}
