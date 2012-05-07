#pragma once

#include "shared/types.h"




int solver_cpu_st_init( );
int solver_cpu_st_deinit( );
void solver_cpu_st_spawn_at( int x, int y, int type );
void solver_cpu_st_update( pp_time_t dt );
int solver_cpu_st_get_alive_particles_count( );
void solver_cpu_st_get_particles_position_stream( float * out );

void solver_cpu_st_collision_set( int x, int y, int collision_type );