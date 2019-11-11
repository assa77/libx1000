#include <stdint.h>
#include <stdio.h>
#include "x1000_config.h"

int main( int argc, char *argv[ ] )
{
	unsigned exechook = 0, lockmem = 0;

	if ( get_config( &exechook, &lockmem ) < 0 )
		printf( "Failed to get configuration\n" );

	printf( "ExecHook,Lockmem: %u,%u\n", exechook, lockmem );

	return 0;
}
