#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#include "shared/types.h"



//! Initialization. Returns 0 on failure. Configuration is copied.
extern int pp_init( const struct PPConfiguration * configuration );
//! Deinitialization. Returns 0 on failure.
extern int pp_deinit( );
//! Get current configuration.
extern const struct PPConfiguration * pp_get_configuration( );
//! Get physic constants (read/write).
extern struct PPConstants * pp_get_constants( );

//! Update frame.
extern void pp_update( pp_time_t dt );

//! Get alive particles count.
extern int pp_get_alive_particles_count( );
//! Get particles position stream (previous position[x,y], current position[x,y]).
extern void pp_get_particles_position_stream( float * out );

//! Spawn particle at specific position.
extern void pp_particle_spawn_at( int x, int y, int type );



#ifdef __cplusplus
}
#endif
