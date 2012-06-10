#include "pch.h"
#include "solver_cpu_st.h"
#include "shared/utils.h"
#include "shared/types.h"
#include <assert.h>
#include <math.h>



extern struct PPConfiguration sConfiguration;
extern struct PPConstants sConstants;
extern struct PPParticleType * spParticleTypes;

struct PPParticleInfo * spParticlesInfo = NULL;
struct PPParticlePhysInfo * spParticlesPhysInfo = NULL;
struct PPParticlePhysInfo * spParticlesPhysInfoLast = NULL;
int sParticleFirstFree;
int sParticleAliveCount;


struct PPAirParticle * spAir = NULL;
struct PPAirParticle * spAirLast = NULL;
int sGridX;
int sGridY;

float sAirKernel[9];


//! Particle map entry.
struct PPParticleMap
{
	unsigned int type : 8;		//!< Particle type. Used for caching.
	unsigned int index : 23;	//!< Index of particle in particles array.
	unsigned int collision : 1;	//!< Is this particle is collision particle?
} * spParticleMap = NULL;





int solver_cpu_st_init( )
{
	int num_parts;
    int i, j;
    float s = 0.0f;

	num_parts = sConfiguration.xres * sConfiguration.yres;

	if( sConfiguration.log_fn )
		sConfiguration.log_fn( LOG_INFO, "Single threading CPU solver is being initialized." );

	spParticlesInfo = malloc_log( sizeof( struct PPParticleInfo ) * num_parts );
	if( spParticlesInfo == NULL )
	{
		solver_cpu_st_deinit( );
		return 0;
	}

	spParticlesPhysInfo = malloc_log( sizeof( struct PPParticlePhysInfo ) * num_parts );
	if( spParticlesPhysInfo == NULL )
	{
		solver_cpu_st_deinit( );
		return 0;
	}

	spParticlesPhysInfoLast = malloc_log( sizeof( struct PPParticlePhysInfo ) * num_parts );
	if( !spParticlesPhysInfoLast )
	{
		solver_cpu_st_deinit( );
		return 0;
	}

	spParticleMap = malloc_log( sizeof( struct PPParticleMap ) * num_parts );
	if( !spParticleMap )
	{
		solver_cpu_st_deinit( );
		return 0;
	}
	memset( spParticleMap, 0, sizeof( struct PPParticleMap ) * num_parts );

	for(i=0; i< num_parts - 1; i++)
        spParticlesInfo[i].life = i+1;
    spParticlesInfo[num_parts - 1].life = -1;
	sParticleFirstFree = 0;
	sParticleAliveCount = 0;

	sGridX = sConfiguration.xres / sConfiguration.grid_size;
	sGridY = sConfiguration.yres / sConfiguration.grid_size;

	num_parts = sGridX * sGridY;
	spAir = malloc_log( sizeof( struct PPAirParticle ) * num_parts );
	if( !spAir )
	{
		solver_cpu_st_deinit( );
		return 0;
	}

	spAirLast = malloc_log( sizeof( struct PPAirParticle ) * num_parts );
	if( !spAirLast )
	{
		solver_cpu_st_deinit( );
		return 0;
	}

	memset( spAir, 0, sizeof( struct PPAirParticle ) * num_parts );
	memset( spAirLast, 0, sizeof( struct PPAirParticle ) * num_parts );

    for(j=-1; j<2; j++)
        for(i=-1; i<2; i++)
        {
            sAirKernel[(i+1)+3*(j+1)] = expf(-2.0f*(i*i+j*j));
            s += sAirKernel[(i+1)+3*(j+1)];
        }
    s = 1.0f / s;
    for(j=-1; j<2; j++)
        for(i=-1; i<2; i++)
            sAirKernel[(i+1)+3*(j+1)] *= s;

	return 1;
}

int solver_cpu_st_deinit( )
{
	free( spParticlesInfo );
	free( spParticlesPhysInfo );
	free( spParticlesPhysInfoLast );
	free( spAir );
	free( spAirLast );
	free( spParticleMap );
	return 1;
}

void kill_part( struct PPParticleInfo * pi, int x, int y, unsigned int i )
{
	pi->type = 0;
	pi->life = sParticleFirstFree;
	sParticleFirstFree = i;

	if( x >= 0 && x < sConfiguration.xres && y >= 0 && y < sConfiguration.yres )
	{
		assert( spParticleMap[ y * sConfiguration.xres + x ].index == i );
		assert( spParticlesInfo + spParticleMap[ y * sConfiguration.xres + x ].index == pi );

		spParticleMap[ y * sConfiguration.xres + x ].type = 0;
	}

	sParticleAliveCount--;
}

void update_air( pp_time_t dt )
{
    int x, y, i, j;
    float dp, dx, dy, f;
	float avgx, avgy, avgp;
	float sdt = FLT_SECOND * dt;
	float p_loss_factor = ( float ) pow( sConstants.p_loss, ( float ) dt * FLT_SECOND );
	float v_loss_factor = ( float ) pow( sConstants.v_loss, ( float ) dt * FLT_SECOND );
	struct PPAirParticle * air;
	struct PPAirParticle * air_last, * tmp;

	air = spAirLast;
	spAirLast = spAir;
	spAir = air;

	air = spAir;
	air_last = spAirLast;
    for( y = 0; y < sGridY; y++ )
        for( x = 0; x < sGridX; x++, air++, air_last++ )
        {
			air->type = air_last->type;
			if( air_last->type )
			{
				air->vx = air->vy = air->p = 0.0f;
				continue;
			}

            avgx = 0.0f;
            avgy = 0.0f;
            avgp = 0.0f;
            for( j = -1; j < 2; j++ )
                for( i = -1; i < 2; i++ )
				{
                    if( y + j >= 0 && y + j < sGridY &&
                        x + i >= 0 && x + i < sGridX )
					{
	                    f = sAirKernel[i + 1 + ( j + 1 ) * 3];
						tmp = air_last + sGridX * j + i;
						avgx += tmp->vx * f;
						avgy += tmp->vy * f;
						avgp += tmp->p * f;
					}
				}

			dp = dx = dy = 0.0f;
			if( y > 0 && y < sGridY - 1 )
			{
				dp += ( air_last + sGridX )->vy - ( air_last - sGridX )->vy;
	            dy = ( air_last + sGridX )->p - ( air_last - sGridX )->p;
			}

			if( x > 0 && x < sGridX - 1 )
			{
				dp += ( air_last + 1 )->vx - ( air_last - 1 )->vx;
	            dx = ( air_last + 1 )->p - ( air_last - 1 )->p;
			}

			air->vx = avgx * v_loss_factor - dx * sConstants.v_hstep * sdt;
			air->vy = avgy * v_loss_factor - dy * sConstants.v_hstep * sdt;
			air->p = avgp * p_loss_factor - dp * sConstants.p_hstep * sdt;
		}
}

int try_move( int i, int x, int y, int nx, int ny )
{
	i;

	if( x == nx && y == ny )
		return 1;

	if( nx < 0 || ny < 0 || nx >= sConfiguration.xres || ny >= sConfiguration.yres )
		return 1;

	if( spParticleMap[ ny * sConfiguration.xres + nx ].type )
		return 0;

	return 1;
}

#ifdef _DEBUG
int incollision( const struct PPParticlePhysInfo * p )
{
	int nx = ( int )floorf( p->x );
	int ny = ( int )floorf( p->y );

	if( nx < 0 || ny < 0 || nx >= sConfiguration.xres || ny >= sConfiguration.yres )
		return 0;

	nx /= sConfiguration.grid_size;
	ny /= sConfiguration.grid_size;

	if( spAir[ ny * sGridX + nx ].type )
		return 1;

	return 0;
}
#endif

__inline float frand( )
{
	return ( float ) rand( ) / RAND_MAX;
}

void solver_cpu_st_update( pp_time_t dt )
{
	struct PPParticleInfo * parti;
	struct PPParticlePhysInfo * partp, *partpl;
	struct PPAirParticle * air;
	struct PPParticleType * ptype;
	int i, j, k, r, npart;
	int x, y, nx = 0, ny = 0;
	int gridx, gridy;
	int found, savestagnant;
	float loss_factor;
	float sdt = FLT_SECOND * dt;
	float accum_heat;
	float savex, savey;
	float dx, dy;
	int heat_count;
	int processed_count = 0;
	float maxv = 0.0f;

	update_air( dt );

	partp = spParticlesPhysInfo;
	spParticlesPhysInfo = spParticlesPhysInfoLast;
	spParticlesPhysInfoLast = partp;

	npart = sConfiguration.xres * sConfiguration.yres;

	parti = spParticlesInfo;
	partpl = spParticlesPhysInfoLast;
	partp = spParticlesPhysInfo;

	for( i = 0; i < npart && processed_count < sParticleAliveCount; i++, parti++, partp++, partpl++ )
	{
		if( !parti->type )
			continue;

		x = ( int )floorf( partpl->x );
		y = ( int )floorf( partpl->y );

		if( parti->life >= 0 )
		{
			parti->life -= dt;
			if( parti->life <= 0 )
			{
				kill_part( parti, x, y, i );
				continue;
			}
		}

		gridx = x / sConfiguration.grid_size;
		gridy = y / sConfiguration.grid_size;

		air = spAir + gridy * sGridX + gridx;
		ptype = spParticleTypes + parti->type;

		assert( ptype->move_type != MT_IMMOVABLE );
		assert( !( x < 0 || y < 0 || x >= sConfiguration.xres || y >= sConfiguration.yres ||
			air->type ) );

		//
		// handle temperature
		//

		if( ptype->hconduct > 0.0f )
		{
			accum_heat = 0.0f;
			heat_count = 0;
			for( nx = -1; nx < 2; nx++ )
			{
				for( ny = -1; ny < 2; ny++ )
				{
					if( x + nx >= 0 && y + ny >= 0 && x + nx < sConfiguration.xres && y + ny < sConfiguration.yres )
					{
						if( !spParticleMap[ ( y + ny ) * sConfiguration.xres + nx + x ].type )
							continue;

						heat_count++;
						accum_heat += ( spParticlesPhysInfoLast + spParticleMap[ ( y + ny ) * sConfiguration.xres + nx + x ].index )->temp;
					}
				}
			}

			partp->temp = partpl->temp;
			if( heat_count > 1 )
				partp->temp += ( accum_heat / heat_count - partpl->temp ) * ptype->hconduct * sdt;
		}

		//
		// handle velocity
		//

		loss_factor = ( float ) pow( ptype->airloss, ( float ) dt * FLT_SECOND );

		air->vx *= loss_factor;
		air->vy *= loss_factor;

		air->vx += ptype->airdrag * partpl->vx * sdt;
		air->vy += ptype->airdrag * partpl->vy * sdt;

		if( ptype->hotair > 0 && gridy > 0 && gridy < sGridY - 1 && 
			gridx > 0 && gridx < sGridX - 1 )
		{
			for( j = -1; j < 2; j++ )
				for( k = -1; k < 2; k++ )
					( air + j * sGridX + k )->p += ptype->hotair * sdt * sAirKernel[k + 1 + ( j + 1 ) * 3];
		}

		loss_factor = ( float ) pow( ptype->vloss, ( float ) dt * FLT_SECOND );

		partp->vx = partpl->vx * loss_factor + ptype->advection * air->vx * sdt;
		partp->vy = partpl->vy * loss_factor + ( ptype->advection * air->vy + ptype->gravity ) * sdt;

		if(ptype->diffusion > 0.0f)
		{
			partp->vx += ptype->diffusion * ( frand( ) * 2.0f - 1.0f ) * sdt;
			partp->vy += ptype->diffusion * ( frand( ) * 2.0f - 1.0f ) * sdt;
		}

		//
		// handle particle update, based on its type
		//

#include "particles/update_cpu_st.inl"

		//
		// handle position
		//

		// trace against collisions
		dx = partp->vx * sdt;
		dy = partp->vy * sdt;
		maxv = max( fabsf( dx ), fabsf( dy ) );
		k = ( int ) maxv + 1;
		dx /= k;
		dy /= k;
		partp->x = partpl->x;
		partp->y = partpl->y;
		for( j = 0; j < k; j++ )
		{
			partp->x += dx;
			partp->y += dy;
			nx = ( int )floorf( partp->x );
			ny = ( int )floorf( partp->y );
			if( nx < 0 || nx >= sConfiguration.xres || 
				ny < 0 || ny >= sConfiguration.yres )
				break;

			if( spParticleMap[ ny * sConfiguration.xres + nx ].collision )
				break;
		}

		if( x == nx && y == ny )
		{
			processed_count++;
			continue;
		}

		savestagnant = parti->stagnant;
		parti->stagnant = 0;
		parti->freefall = 1;
		if( !try_move( i, x, y, nx, ny ) )
		{
			parti->freefall = 0;
			savex = partp->x;
			savey = partp->y;
			partp->x = partpl->x;
			partp->y = partpl->y;
			assert( !incollision( partp ) );
			if( ptype->move_type == MT_NORMAL )
			{
				parti->stagnant = 1;
			}
			else
			{
				assert( ptype->move_type == MT_POWDER || ptype->move_type == MT_LIQUID );

				if( nx != x && try_move( i, x, y, nx, y ) )
				{
					partp->x = savex;
					assert( !incollision( partp ) );
				}
				else if( ny != y && try_move( i, x, y, x, ny ) )
				{
					partp->y = savey;
					assert( !incollision( partp ) );
				}
				else
				{
					r = ( rand() & 1 ) * 2 - 1;
					if( ny != y && try_move( i, x, y, x + r, ny ) )
					{
						partp->x += r;
						partp->y = savey;
						assert( !incollision( partp ) );
					}
					else if( ny != y && try_move( i, x, y, x - r, ny ) )
					{
						partp->x -= r;
						partp->y = savey;
						assert( !incollision( partp ) );
					}
					else if( nx != x && try_move( i, x, y, nx, y + r ) )
					{
						partp->x = savex;
						partp->y += r;
						assert( !incollision( partp ) );
					}
					else if( nx != x && try_move( i, x, y, nx, y - r ) )
					{
						partp->x = savex;
						partp->y -= r;
						assert( !incollision( partp ) );
					}
					else if( ptype->move_type == MT_LIQUID && partp->vy > fabs( partp->vx ) )
					{
						found = 0;
						k = savestagnant ? 10 : 50;
						k = ( int ) ( k );
						for( j = x + r; j >= 0 && j >= x - k && j < x + k && j < sConfiguration.xres; j += r )
						{
							if( spParticleMap[ y * sConfiguration.xres + j ].type
								&& spParticleMap[ y * sConfiguration.xres + j ].type != parti->type )
								break;

							if( try_move( i, x, y, j, ny ) )
							{
								partp->x += j - x;
								partp->y += ny - y;
								x = j;
								y = ny;
								found = 1;
								assert( !incollision( partp ) );
								break;
							}
							if( try_move( i, x, y, j, y ) )
							{
								partp->x += j - x;
								x = j;
								found = 1;
								assert( !incollision( partp ) );
								break;
							}
						}
						r = partp->vy > 0 ? 1 : -1;
						if( found )
						{
							for( j = y + r; j >= 0 && j < sConfiguration.yres && j >= y - k && j < y + k; j += r)
							{
								if( spParticleMap[ j * sConfiguration.xres + x ].type
									&& spParticleMap[ j * sConfiguration.xres + x ].type != parti->type )
								{
									found = 0;
									break;
								}
								if( try_move( i, x, y, x, j ) )
								{
									partp->y += j - y;
									assert( !incollision( partp ) );
									break;
								}
							}
						}

						if( !found )
							parti->stagnant = 1;
					}
					else
					{
						parti->stagnant = 1;
					}
				}
			}
			partp->vx *= ptype->collision;
			partp->vy *= ptype->collision;
		}

		x = ( int )floorf( partpl->x );
		y = ( int )floorf( partpl->y );
		nx = ( int )floorf( partp->x );
		ny = ( int )floorf( partp->y );
		if( nx < 0 || ny < 0 ||
			nx >= sConfiguration.xres ||
			ny >= sConfiguration.yres )
		{
			kill_part( parti, x, y, i );
			continue;
		}

		assert( !incollision( partp ) );

		spParticleMap[ y * sConfiguration.xres + x ].type = 0;
		spParticleMap[ ny * sConfiguration.xres + nx ].type = parti->type;
		spParticleMap[ ny * sConfiguration.xres + nx ].index = i;

		processed_count++;
	}
}

void solver_cpu_st_spawn_at( int x, int y, unsigned int type )
{
	struct PPParticleMap * pmap = spParticleMap + y * sConfiguration.xres + x;
	struct PPParticleInfo * parti;
	struct PPParticlePhysInfo * partp, * partpl;
	int index;

	if( pmap->type )
		return;

	if( sParticleFirstFree < 0 )
		return;

	index = sParticleFirstFree;
	parti = spParticlesInfo + index;
	partp = spParticlesPhysInfo + index;
	partpl = spParticlesPhysInfoLast + index;
	sParticleFirstFree = parti->life;

	parti->type = type;
	parti->life = -1;
	partp->x = ( float ) x;
	partp->y = ( float ) y;
	partp->vx = partp->vy = 0.0f;
	partp->temp = spParticleTypes[ type ].initial_temp;
	*partpl = *partp;

	spParticleMap[ y * sConfiguration.xres + x ].index = index;
	spParticleMap[ y * sConfiguration.xres + x ].type = type;
	spParticleMap[ y * sConfiguration.xres + x ].collision = 0;

	sParticleAliveCount++;
}

int solver_cpu_st_get_alive_particles_count( )
{
	return sParticleAliveCount;
}

const struct PPParticleInfo * solver_cpu_st_get_particles_info_stream( )
{
	return spParticlesInfo;
}

const struct PPParticlePhysInfo * solver_cpu_st_get_particles_phys_info_stream( )
{
	return spParticlesPhysInfo;
}

const struct PPParticlePhysInfo * solver_cpu_st_get_particles_phys_info_stream_last( )
{
	return spParticlesPhysInfoLast;
}

struct PPAirParticle * solver_cpu_st_get_air_particle_stream( )
{
	return spAir;
}

const struct PPAirParticle * solver_cpu_st_get_air_particle_stream_last( )
{
	return spAirLast;
}

void solver_cpu_st_collision_set( int x, int y, unsigned int collision_type )
{
	int gridx;
	int gridy;
	int i, j;
	int cnt;

	if( x < 0 || x >= sConfiguration.xres || y < 0 || y >= sConfiguration.yres )
		return;

	gridx = x / sConfiguration.grid_size;
	gridy = y / sConfiguration.grid_size;

	if( collision_type )
	{
		if( spParticleMap[ y * sConfiguration.xres + x ].type )
			return;

		assert( spParticleTypes[ collision_type ].move_type == MT_IMMOVABLE );

		spParticleMap[ y * sConfiguration.xres + x ].index = 0;
		spParticleMap[ y * sConfiguration.xres + x ].type = collision_type;
		spParticleMap[ y * sConfiguration.xres + x ].collision = 1;

		cnt = 0;
		for( j = gridy * sConfiguration.grid_size; j < ( gridy + 1 ) * sConfiguration.grid_size; j++ )
			for( i = gridx * sConfiguration.grid_size; i < ( gridx + 1 ) * sConfiguration.grid_size; i++ )
				if( spParticleMap[ j * sConfiguration.xres + i ].type == collision_type )
					cnt++;

		if( cnt == sConfiguration.grid_size * sConfiguration.grid_size )
			spAir[ gridy * sGridX + gridx ].type = collision_type;
	}
	else
	{
		if( spAir[ gridy * sGridX + gridx ].type )
		{
			spParticleMap[ y * sConfiguration.xres + x ].type = 0;
			spParticleMap[ y * sConfiguration.xres + x ].collision = 0;
			spAir[ gridy * sGridX + gridx ].type = 0;
		}
		else
		{
			if( spParticleMap[ y * sConfiguration.xres + x ].type )
				if( spParticleMap[ y * sConfiguration.xres + x ].collision )
				{
					spParticleMap[ y * sConfiguration.xres + x ].type = 0;
					spParticleMap[ y * sConfiguration.xres + x ].collision = 0;
				}
				else
				{
					kill_part( spParticlesInfo + spParticleMap[ y * sConfiguration.xres + x ].index, x, y, spParticleMap[ y * sConfiguration.xres + x ].index );
				}
		}
	}
}