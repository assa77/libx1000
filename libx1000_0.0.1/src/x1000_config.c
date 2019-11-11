#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <stdlib.h>
#include <linux/limits.h>
#include "x1000_config.h"


struct _configtoken
{
	const char *token;
	unsigned bit;
};

struct _configsection
{
	const char *section;
	const struct _configtoken *ptokens;
};


static const struct _configtoken lockmem_tokens[ ] =
{
	{ "DS", X1000_LOCKMEM_DATASEG },
	{ "HS", X1000_LOCKMEM_HEAPSTACK },
	{ "MM", X1000_LOCKMEM_MMAP },
	{ NULL, 0 }
};

static const struct _configtoken exechook_tokens[ ] =
{
	{ "PI", X1000_EXECHOOK_PREINIT },
	{ "FK", X1000_EXECHOOK_FORK },
	{ "MM", X1000_EXECHOOK_MMAP },
	{ NULL, 0 }
};

static const struct _configsection sections[ X1000_SECTION_MAX ] =
{
	{ "MEMLOCK", lockmem_tokens },
	{ "EXECHOOK", exechook_tokens }
};

static const char *pconfigdirs[ ] =
{
	"/etc/libx1000.so.d/",
	"/proc/self/cwd/",
	 NULL
};

static FILE *open_config_file( )
{
	char linkat[ PATH_MAX ];
	char configname[ PATH_MAX ];
	char *pfilename[ 3 ] = { NULL, "defaults", NULL };
	FILE *f;
	size_t i;
	size_t e;

	if ( readlink( "/proc/self/exe", linkat, PATH_MAX ) < 0 )
		return NULL;

	*pfilename = basename( linkat );

	for ( e = 0; pfilename[ e ]; ++e )
	{
		for ( i = 0; pconfigdirs[ i ]; ++i )
		{
			sprintf( configname, "%s%s.conf",
				pconfigdirs[ i ], pfilename[ e ] );

			if ( ( f = fopen( configname, "r" ) ) )
				return f;
		}
	}

	return NULL;
}

static size_t find_section( const char *psection )
{
	size_t i;

	for ( i = 0; i < X1000_SECTION_MAX; ++i )
		if ( !strcmp( sections[ i ].section, psection ) )
			break;
	return i;
}

static unsigned parse_tokens( size_t section, char *pconfig_tokens )
{
	char *pstoken;
	const struct _configtoken *ptokens = sections[ section ].ptokens, *p;
	unsigned bitmask = 0;

	for ( pstoken = strtok( pconfig_tokens,"|" );
		pstoken; pstoken = strtok( NULL,"|" ) )
	{
		for ( p = ptokens; p->token; ++p )
		{
			if ( !strcmp( pstoken, p->token ) )
			{
				bitmask |= 1 << ( p->bit - 1 );
				break;
			}
		}
	}

	return bitmask;
}

int get_config( unsigned *plockmem, unsigned *pphase )
{
	char buffer[ PATH_MAX ];
	char *psection, *ptokens;
	size_t section_index;
	FILE *f;
	unsigned *r[ X1000_SECTION_MAX ] =
	{
		plockmem,
		pphase
	};
	int res;

	*plockmem = *pphase = 0;

	if ( !( f = open_config_file( ) ) )
		return 0;

	while ( fgets( buffer,PATH_MAX,f ) )
	{
		/* ignore comments */
		if ( buffer[ 0 ] == '#' || buffer[ 0 ] == '\0' )
			continue;

		/* parse strings or ignore */
		if ( ( res = sscanf( buffer, "%m[^'=']=%ms", &psection, &ptokens ) ) != 2 )
		{
			if ( res == 1 )
				free( psection );
			continue;
		}

		if ( ( section_index = find_section( psection ) ) < X1000_SECTION_MAX )
			*r[ section_index ] |= parse_tokens( section_index, ptokens );

		free( psection );
		free( ptokens );
	}

	fclose( f );

	return *plockmem && *pphase;
}
