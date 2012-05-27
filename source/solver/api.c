#include "pch.h"
#include "api.h"
#include "cpu_st/solver_cpu_st.h"
#include "shared/version.h"
#include "shared/utils.h"
#include "particles/common.h"
#include <assert.h>




struct PPConfiguration sConfiguration;
struct PPConstants sConstants;
struct PPParticleType * spParticleTypes = NULL;



int pp_init( const struct PPConfiguration * configuration )
{
	int i;

	memcpy( &sConfiguration, configuration, sizeof( struct PPConfiguration ) );

	if( sConfiguration.log_fn )
#ifdef _DEBUG
		sConfiguration.log_fn( LOG_INFO, "Initialization of Powder Physics ver. %d.%d.%d (debug).", VER_MAJOR, VER_MINOR, VER_BUILD );
#else
		sConfiguration.log_fn( LOG_INFO, "Initialization of Powder Physics ver. %d.%d.%d.", VER_MAJOR, VER_MINOR, VER_BUILD );
#endif

	assert( sConfiguration.xres % sConfiguration.grid_size == 0 );
	assert( sConfiguration.yres % sConfiguration.grid_size == 0 );
	if( sConfiguration.xres % sConfiguration.grid_size != 0 ||
		sConfiguration.yres % sConfiguration.grid_size != 0 )
	{
		if( sConfiguration.log_fn )
			sConfiguration.log_fn( LOG_ERROR, "xres and yres must be divided evenly by grid_size: xres=%d, yres=%d, grid_size=%d", sConfiguration.xres, sConfiguration.yres, sConfiguration.grid_size );
		return 0;
	}

	sConstants.p_loss = 0.95f;
	sConstants.v_loss = 0.95f;
	sConstants.p_hstep = 4.5f;
	sConstants.v_hstep = 6.0f;

	spParticleTypes = malloc_log( sizeof( struct PPParticleType ) * ( PARTICLE_TYPES + 1 ) );
	if( !spParticleTypes )
		return 0;

	i = 1;
#include "particles/register.inl"
	assert( i == PARTICLE_TYPES + 1 );

	return solver_cpu_st_init( );
}

int pp_deinit( )
{
	free( spParticleTypes );
	return solver_cpu_st_deinit( );
}

const struct PPConfiguration * pp_get_configuration( )
{
	return &sConfiguration;
}

extern struct PPConstants * pp_get_constants( )
{
	return &sConstants;
}

void pp_update( pp_time_t dt )
{
	solver_cpu_st_update( dt );
}

int pp_get_alive_particles_count( )
{
	return solver_cpu_st_get_alive_particles_count( );
}

const struct PPParticleInfo * pp_get_particles_info_stream( )
{
	return solver_cpu_st_get_particles_info_stream( );
}

const struct PPParticlePhysInfo * pp_get_particles_phys_info_stream( )
{
	return solver_cpu_st_get_particles_phys_info_stream( );
}

const struct PPParticlePhysInfo * pp_get_particles_phys_info_stream_last( )
{
	return solver_cpu_st_get_particles_phys_info_stream_last( );
}

struct PPAirParticle * pp_get_air_particle_stream( )
{
	return solver_cpu_st_get_air_particle_stream( );
}

const struct PPAirParticle * pp_get_air_particle_stream_last( )
{
	return solver_cpu_st_get_air_particle_stream_last( );
}

void pp_particle_spawn_at( int x, int y, unsigned int type )
{
	solver_cpu_st_spawn_at( x, y, type );
}

void pp_collision_set( int x, int y, unsigned int collision_type )
{
	solver_cpu_st_collision_set( x, y, collision_type );
}

int pp_get_particle_types_count( )
{
	return PARTICLE_TYPES + 1;
}

const struct PPParticleType * pp_get_particle_type( int index )
{
	return spParticleTypes + index;
}