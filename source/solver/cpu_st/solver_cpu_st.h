#pragma once

#include "shared/types.h"




int solver_cpu_st_init( );
int solver_cpu_st_deinit( );
void solver_cpu_st_update( pp_time_t dt );

int solver_cpu_st_get_alive_particles_count( );
const struct PPParticleInfo * solver_cpu_st_get_particles_info_stream( );
const struct PPParticlePhysInfo * solver_cpu_st_get_particles_phys_info_stream( );
const struct PPParticlePhysInfo * solver_cpu_st_get_particles_phys_info_stream_last( );
struct PPAirParticle * solver_cpu_st_get_air_particle_stream( );
const struct PPAirParticle * solver_cpu_st_get_air_particle_stream_last( );

void solver_cpu_st_spawn_at( int x, int y, unsigned int type );
void solver_cpu_st_collision_set( int x, int y, unsigned int collision_type );