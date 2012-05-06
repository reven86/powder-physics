#include "pch.h"
#include "solver_cpu_st.h"
#include "shared/utils.h"
#include "shared/types.h"



extern struct PPConfiguration sConfiguration;
extern struct PPConstants sConstants;
extern struct PPParticleType * spParticleTypes;

struct PPParticleInfo * spParticlesInfo;
struct PPParticlePhysInfo * spParticlesPhysInfo;
struct PPParticlePhysInfo * spParticlesPhysInfoLast;
int sParticleFirstFree;
int sParticleAliveCount;


//! Air particle.
struct PPAirParticle
{
	float vx;		//!< X velocity.
	float vy;		//!< Y velocity.
	float p;		//!< Pressure.
};

struct PPAirParticle * spAir;
struct PPAirParticle * spAirLast;
int sGridX;
int sGridY;

float sAirKernel[9];


//! Particle map entry.
struct PPParticleMap
{
	unsigned int type : 8;		//!< Particle type. Used for caching.
	unsigned int index : 24;	//!< Index of particle in particles array.
} * spParticleMap;





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
	return 1;
}

void kill_part2( struct PPParticleInfo * pi, int nx, int ny, int i )
{
  pi->type = 0;
  pi->life = sParticleFirstFree;
  sParticleFirstFree = i;

  if( nx >= 0 && nx < sConfiguration.xres && ny >= 0 && ny < sConfiguration.yres )
	  spParticleMap[ ny * sConfiguration.xres + nx ].type = 0;

  sParticleAliveCount--;
}

void update_air2( pp_time_t dt )
{
    int x, y, i, j;
    float dp, dx, dy, f, tx, ty;
	float avgx, avgy, avgp;
	float sdt = FLT_SECOND * dt;
	float p_loss_factor = ( float ) pow( sConstants.p_loss, ( float ) dt * FLT_SECOND );
	float v_loss_factor = ( float ) pow( sConstants.v_loss, ( float ) dt * FLT_SECOND );
	struct PPAirParticle * air;
	struct PPAirParticle * air_last;

	air = spAirLast;
	spAirLast = spAir;
	spAir = air;

	air = spAir;
	air_last = spAirLast;
    for( y = 0; y < sGridY; y++ )
        for( x = 0; x< sGridX; x++, air++, air_last++ )
        {
            avgx = 0.0f;
            avgy = 0.0f;
            avgp = 0.0f;
            for( j = -1; j < 2; j++ )
                for( i = -1; i < 2; i++ )
				{
                    f = sAirKernel[i + 1 + ( j + 1 ) * 3];
                    if( y + j > 0 && y + j < sGridY - 1 &&
                        x + i > 0 && x + i < sGridX - 1 )
                    {
                        avgx += ( air_last + sGridX * j + i )->vx * f;
                        avgy += ( air_last + sGridX * j + i )->vy * f;
                        avgp += ( air_last + sGridX * j + i )->p * f;
                    }
                    else
					{
                        avgx += air_last->vx * f;
                        avgy += air_last->vy * f;
                        avgp += air_last->p * f;
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

            tx = x - dx * sdt;
            ty = y - dy * sdt;
            i = (int)tx;
            j = (int)ty;
            tx -= i;
            ty -= j;
            if(i > 1 && i < sGridX - 3 &&
               j > 1 && j < sGridY - 3)
            {
				avgx *= sConstants.v_smooth;
                avgy *= sConstants.v_smooth;

				avgx += ( 1.0f - sConstants.v_smooth ) * ( 1.0f - tx ) * ( 1.0f - ty ) * air_last->vx;
                avgy += ( 1.0f - sConstants.v_smooth ) * ( 1.0f - tx ) * ( 1.0f - ty ) * air_last->vy;

				avgx += ( 1.0f - sConstants.v_smooth ) * tx * ( 1.0f - ty ) * ( air_last + 1 )->vx;
                avgy += ( 1.0f - sConstants.v_smooth ) * tx * ( 1.0f - ty ) * ( air_last + 1 )->vy;

				avgx += ( 1.0f - sConstants.v_smooth ) * ( 1.0f - tx ) * ty * ( air_last + sGridX )->vx;
                avgy += ( 1.0f - sConstants.v_smooth ) * ( 1.0f - tx ) * ty * ( air_last + sGridX )->vy;

                avgx += ( 1.0f - sConstants.v_smooth ) * tx * ty * ( air_last + sGridX + 1 )->vx;
                avgy += ( 1.0f - sConstants.v_smooth ) * tx * ty * ( air_last + sGridX + 1 )->vy;
            }

			air->vx = avgx * v_loss_factor + dx * sConstants.v_hstep * sdt;
			air->vy = avgy * v_loss_factor + dy * sConstants.v_hstep * sdt;
			air->p = avgp * p_loss_factor + dp * sConstants.p_hstep * sdt;
		}
}

int try_move2( int i, int x, int y, int nx, int ny )
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

void solver_cpu_st_update( pp_time_t dt )
{
	struct PPParticleInfo * parti;
	struct PPParticlePhysInfo * partp, *partpl;
	struct PPAirParticle * air;
	struct PPParticleType * ptype;
	int i, j, k, r, npart;
	int x, y, nx, ny;
	int gridx, gridy;
	int found, savestagnant;
	float loss_factor;
	float sdt = FLT_SECOND * dt;
	float rgrid = 1.0f / sConfiguration.grid_size;
	float accum_heat;
	float savex, savey;
	int heat_count;
	int processed_count = 0;

	update_air2( dt );

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

		x = ( int )( partpl->x + 0.5f );
		y = ( int )( partpl->y + 0.5f );

		if( parti->life >= 0 )
		{
			parti->life -= dt;
			if( parti->life <= 0 )
			{
				kill_part2( parti, x, y, i );
				continue;
			}
		}

		if( x < 0 || y < 0 || x >= sConfiguration.xres || y >= sConfiguration.yres )
		{
			kill_part2( parti, x, y, i );
			continue;
		}

		gridx = x / sConfiguration.grid_size;
		gridy = y / sConfiguration.grid_size;

		air = spAir + gridy * sGridX + gridx;
		ptype = spParticleTypes + parti->type;

		// handle velocity

		loss_factor = ( float ) pow( ptype->airloss, ( float ) dt * FLT_SECOND * rgrid );

		air->vx *= loss_factor;
		air->vy *= loss_factor;

		air->vx += ptype->airdrag * rgrid * partpl->vx * sdt;
		air->vy += ptype->airdrag * rgrid * partpl->vy * sdt;

		if( gridy > 0 && gridy < sGridY - 1 && 
			gridx > 0 && gridx < sGridX - 1 )
		{
			for( j = -1; j < 2; j++ )
				for( k = -1; k < 2; k++ )
					( air + j * sGridX + k )->p += ptype->hotair * rgrid * sdt * sAirKernel[k + 1 + ( j + 1 ) * 3];
		}

		loss_factor = ( float ) pow( ptype->vloss, ( float ) dt * FLT_SECOND );

		partp->vx = partpl->vx * loss_factor + ptype->advection * rgrid * air->vx * sdt;
		partp->vy = partpl->vy * loss_factor + ( ptype->advection * rgrid * air->vy + ptype->gravity ) * sdt;

		partp->x = partpl->x + partp->vx * sdt;
		partp->y = partpl->y + partp->vy * sdt;

		// handle temperature
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
						if( !spParticleMap[ ( y + ny ) * sGridX + nx ].type )
							continue;

						heat_count++;
						accum_heat += ( spParticlesPhysInfoLast + spParticleMap[ ( y + ny ) * sGridX + nx ].index )->temp;
					}
				}
			}

			partp->temp = partpl->temp;
			if( heat_count > 0 )
				partp->temp += ( accum_heat / heat_count - partpl->temp ) * ptype->hconduct * sdt;
		}

		// handle position
		nx = ( int )( partp->x + 0.5f );
		ny = ( int )( partp->y + 0.5f );

		savestagnant = parti->stagnant;
		parti->stagnant = 0;
		if( !try_move2( i, x, y, nx, ny ) )
		{
			savex = partp->x;
			savey = partp->y;
			partp->x = partpl->x;
			partp->y = partpl->y;
			if( !ptype->powderfall )
			{
				parti->stagnant = 1;
			}
			else
			{
				if( nx != x && try_move2( i, x, y, nx, y ) )
				{
					partp->x = savex;
				}
				else if( ny != y && try_move2( i, x, y, x, ny ) )
				{
					partp->y = savey;
				}
				else
				{
					r = ( rand() & 1 ) * 2 - 1;
					if( ny != y && try_move2( i, x, y, x + r, ny ) )
					{
						partp->x += r;
						partp->y = savey;
					}
					else if( ny!=y && try_move2( i, x, y, x - r, ny ) )
					{
						partp->x -= r;
						partp->y = savey;
					}
					else if( nx != x && try_move2( i, x, y, nx, y + r ) )
					{
						partp->x = savex;
						partp->y += r;
					}
					else if( nx != x && try_move2( i, x, y, nx, y - r ) )
					{
						partp->x = savex;
						partp->y -= r;
					}
					else if( ptype->liquidfall && partp->vy > fabs( partp->vx ) )
					{
						found = 0;
						k = savestagnant ? 10 : 50;
						for( j = x + r; j >= 0 && j >= x - k && j < x + k && j < sConfiguration.xres; j += r )
						{
							if( try_move2( i, x, y, j, ny ) )
							{
								partp->x += j - x;
								partp->y += ny - y;
								found = 1;
								break;
							}
							if( try_move2( i, x, y, j, y ) )
							{
								partp->x += j - x;
								found = 1;
								break;
							}
						}
						r = partp->vy > 0 ? 1 : -1;
						if( found )
							for( j = y + r; j >= 0 && j < sConfiguration.yres && j >= y - k && j < y + k; j += r)
							{
								if( try_move2( i, x, y, x, j ) )
								{
									partp->y += j - y;
									break;
								}
							}
						else
							parti->stagnant = 1;
					}
					else
					{
						parti->stagnant = 1;
					}
				}
				partp->vx *= ptype->collision;
				partp->vy *= ptype->collision;
			}
		}

		nx = ( int )( partp->x + 0.5f );
		ny = ( int )( partp->y + 0.5f );
		if( nx < sConfiguration.grid_size || ny < sConfiguration.grid_size ||
			nx >= sConfiguration.xres - sConfiguration.grid_size ||
			ny >= sConfiguration.yres - sConfiguration.grid_size )
		{
			kill_part2( parti, x, y, i );
			continue;
		}

		spParticleMap[ y * sConfiguration.xres + x ].type = 0;
		spParticleMap[ ny * sConfiguration.xres + nx ].type = parti->type;
		spParticleMap[ ny * sConfiguration.xres + nx ].index = i;

		processed_count++;
	}
}

void solver_cpu_st_spawn_at( int x, int y, int type )
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

	sParticleAliveCount++;
}

int solver_cpu_st_get_alive_particles_count( )
{
	return sParticleAliveCount;
}

void solver_cpu_st_get_particles_position_stream( float * out )
{
	int i, stored_count;
	int npart = sConfiguration.xres * sConfiguration.yres;
	struct PPParticleInfo * parti = spParticlesInfo;
	struct PPParticlePhysInfo * partpl = spParticlesPhysInfoLast;
	struct PPParticlePhysInfo * partp = spParticlesPhysInfo;

	i = 0; stored_count = 0;
	while( i < npart && stored_count < sParticleAliveCount )
	{
		if( parti->type )
		{
			*out++ = partpl->x;
			*out++ = partpl->y;
			*out++ = partp->x;
			*out++ = partp->y;
			stored_count++;
		}
		i++;
		parti++;
		partp++;
		partpl++;
	}
}