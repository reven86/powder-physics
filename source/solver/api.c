#include "pch.h"
#include "api.h"
#include "cpu_st/solver_cpu_st.h"
#include "shared/version.h"
#include "shared/utils.h"




struct PPConfiguration sConfiguration;
struct PPConstants sConstants;
struct PPParticleType * spParticleTypes;


pp_time_t sElapsedTime = 0;



int pp_init( const struct PPConfiguration * configuration )
{
	memcpy( &sConfiguration, configuration, sizeof( struct PPConfiguration ) );

	if( sConfiguration.log_fn )
#ifdef _DEBUG
		sConfiguration.log_fn( LOG_INFO, "Initialization of Powder Physics ver. %d.%d.%d (debug).", VER_MAJOR, VER_MINOR, VER_BUILD );
#else
		sConfiguration.log_fn( LOG_INFO, "Initialization of Powder Physics ver. %d.%d.%d.", VER_MAJOR, VER_MINOR, VER_BUILD );
#endif

	sConstants.p_loss = 0.95f;
	sConstants.v_loss = 0.95f;
	sConstants.p_hstep = 4.5f;
	sConstants.v_hstep = 6.0f;
	sConstants.timestep = SECOND / 30;

	spParticleTypes = malloc_log( sizeof( struct PPParticleType ) * 2 );
	if( !spParticleTypes )
		return 0;

	spParticleTypes[1].name = "Water";
	spParticleTypes[1].airloss = 0.64f;
	spParticleTypes[1].airdrag = 0.8f;
	spParticleTypes[1].hotair = 0.00f;
	spParticleTypes[1].vloss = 0.5f;
	spParticleTypes[1].advection = 4.9f;
	spParticleTypes[1].gravity = 0.3f;
	spParticleTypes[1].hconduct = 1.0f;
	spParticleTypes[1].powderfall = 1;
	spParticleTypes[1].liquidfall = 1;
	spParticleTypes[1].collision = 0.0f;
	spParticleTypes[1].initial_temp = 20.0;

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
	sElapsedTime += dt;

	while( sElapsedTime > sConstants.timestep )
	{
		solver_cpu_st_update( sConstants.timestep );
		sElapsedTime -= sConstants.timestep;
	}
}

int pp_get_alive_particles_count( )
{
	return solver_cpu_st_get_alive_particles_count( );
}

void pp_get_particles_position_stream( float * out )
{
	solver_cpu_st_get_particles_position_stream( out );
}

void pp_particle_spawn_at( int x, int y, int type )
{
	solver_cpu_st_spawn_at( x, y, type );
}

void pp_collision_set( int x, int y, int collision_type )
{
	solver_cpu_st_collision_set( x, y, collision_type );
}