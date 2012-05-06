#include "pch.h"
#include "utils.h"
#include "types.h"
#include <stdlib.h>



extern struct PPConfiguration sConfiguration;



void * malloc_log( int size )
{
	void * res = malloc( size );
	if( !res )
		if( sConfiguration.log_fn )
			sConfiguration.log_fn( LOG_ERROR, "malloc failed: size=%d", size );

	return res;
}