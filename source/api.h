#ifndef __POWDER_API_H__
#define __POWDER_API_H__


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
//! Get raw particles info stream (read only).
extern const struct PPParticleInfo * pp_get_particles_info_stream( );
//! Get raw particles current physical info stream (read only).
extern const struct PPParticlePhysInfo * pp_get_particles_phys_info_stream( );
//! Get raw particles previous physical info stream (read only).
extern const struct PPParticlePhysInfo * pp_get_particles_phys_info_stream_last( );
//! Get raw air particles stream (read/write).
extern struct PPAirParticle * pp_get_air_particle_stream( );
//! Get raw air particles previous stream (read only).
extern const struct PPAirParticle * pp_get_air_particle_stream_last( );


//! Get number of registered particle types.
extern int pp_get_particle_types_count( );
//! Get particle type.
extern const struct PPParticleType * pp_get_particle_type( int index );
//! Find particle type by name. Returns -1 in case of failure. Complexity is O(N).
extern int pp_find_particle_type( const char * name );


//! Spawn particle at specific position.
extern void pp_particle_spawn_at( int x, int y, unsigned int type );

//! Insert collision at specific position.
extern void pp_collision_set( int x, int y, unsigned int collision_type );



#endif // __POWDER_API_H__