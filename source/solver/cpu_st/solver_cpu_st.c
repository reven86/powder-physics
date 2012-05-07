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


//! Collision map.
struct PPCollisionMap
{
	unsigned int type : 8;		//!< Collision type.
} * spCollisionMap;




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

	spCollisionMap = malloc_log( sizeof( struct PPCollisionMap ) * num_parts );
	if( !spCollisionMap )
	{
		solver_cpu_st_deinit( );
		return 0;
	}

	memset( spAir, 0, sizeof( struct PPAirParticle ) * num_parts );
	memset( spAirLast, 0, sizeof( struct PPAirParticle ) * num_parts );
	memset( spCollisionMap, 0, sizeof( struct PPCollisionMap ) * num_parts );

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
	free( spCollisionMap );
	return 1;
}

void kill_part( struct PPParticleInfo * pi, int nx, int ny, int i )
{
  pi->type = 0;
  pi->life = sParticleFirstFree;
  sParticleFirstFree = i;

  if( nx >= 0 && nx < sConfiguration.xres && ny >= 0 && ny < sConfiguration.yres )
	  spParticleMap[ ny * sConfiguration.xres + nx ].type = 0;

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
	struct PPAirParticle * air_last;
	struct PPCollisionMap * cmap;

	air = spAirLast;
	spAirLast = spAir;
	spAir = air;

	air = spAir;
	air_last = spAirLast;
	cmap = spCollisionMap;
    for( y = 0; y < sGridY; y++ )
        for( x = 0; x< sGridX; x++, air++, air_last++, cmap++ )
        {
            avgx = 0.0f;
            avgy = 0.0f;
            avgp = 0.0f;
            for( j = -1; j < 2; j++ )
                for( i = -1; i < 2; i++ )
				{
                    f = sAirKernel[i + 1 + ( j + 1 ) * 3];
                    if( y + j > 0 && y + j < sGridY - 1 &&
                        x + i > 0 && x + i < sGridX - 1 &&
						!( cmap + sGridX * j + i )->type
						)
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

			air->vx = cmap->type ? 0.0f : avgx * v_loss_factor - dx * sConstants.v_hstep * sdt;
			air->vy = cmap->type ? 0.0f : avgy * v_loss_factor - dy * sConstants.v_hstep * sdt;
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

	nx /= sConfiguration.grid_size;
	ny /= sConfiguration.grid_size;

	if( spCollisionMap[ ny * sGridX + nx ].type )
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

		x = ( int )( partpl->x + 0.5f );
		y = ( int )( partpl->y + 0.5f );

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

		if( x < 0 || y < 0 || x >= sConfiguration.xres || y >= sConfiguration.yres ||
			spCollisionMap[ gridy * sGridX + gridx ].type )
		{
			kill_part( parti, x, y, i );
			continue;
		}

		air = spAir + gridy * sGridX + gridx;
		ptype = spParticleTypes + parti->type;

		//
		// handle velocity
		//

		loss_factor = ( float ) pow( ptype->airloss, ( float ) dt * FLT_SECOND );

		air->vx *= loss_factor;
		air->vy *= loss_factor;

		air->vx += ptype->airdrag * partpl->vx * sdt;
		air->vy += ptype->airdrag * partpl->vy * sdt;

		if( gridy > 0 && gridy < sGridY - 1 && 
			gridx > 0 && gridx < sGridX - 1 )
		{
			for( j = -1; j < 2; j++ )
				for( k = -1; k < 2; k++ )
					( air + j * sGridX + k )->p += ptype->hotair * sdt * sAirKernel[k + 1 + ( j + 1 ) * 3];
		}

		loss_factor = ( float ) pow( ptype->vloss, ( float ) dt * FLT_SECOND );

		partp->vx = partpl->vx * loss_factor + ptype->advection * air->vx * sdt;
		partp->vy = partpl->vy * loss_factor + ( ptype->advection * air->vy + ptype->gravity * sConfiguration.yres ) * sdt;

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

		//
		// handle position
		//

		// trace against collisions
		dx = partp->vx * sdt;
		dy = partp->vy * sdt;
		maxv = max( fabsf( dx ), fabsf( dy ) );
		k = ( int ) maxv + 1;
		maxv = 1.0f / maxv;
		dx /= k;
		dy /= k;
		partp->x = partpl->x;
		partp->y = partpl->y;
		for( j = 0; j < k; j++ )
		{
			partp->x += dx;
			partp->y += dy;
			nx = ( int )( partp->x + 0.5f );
			ny = ( int )( partp->y + 0.5f );
			if( nx < 0 || nx >= sConfiguration.xres || 
				ny < 0 || ny >= sConfiguration.yres ||
				spCollisionMap[ ( ny / sConfiguration.grid_size ) * sGridX + nx / sConfiguration.grid_size ].type )
			{
				partp->x = ( float ) nx;
				partp->y = ( float ) ny;
				break;
			}
		}

		if( x == nx && y == ny )
		{
			processed_count++;
			continue;
		}

		savestagnant = parti->stagnant;
		parti->stagnant = 0;
		if( !try_move( i, x, y, nx, ny ) )
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
				if( nx != x && try_move( i, x, y, nx, y ) )
				{
					partp->x = savex;
				}
				else if( ny != y && try_move( i, x, y, x, ny ) )
				{
					partp->y = savey;
				}
				else
				{
					r = ( rand() & 1 ) * 2 - 1;
					if( ny != y && try_move( i, x, y, x + r, ny ) )
					{
						partp->x += r;
						partp->y = savey;
					}
					else if( ny!=y && try_move( i, x, y, x - r, ny ) )
					{
						partp->x -= r;
						partp->y = savey;
					}
					else if( nx != x && try_move( i, x, y, nx, y + r ) )
					{
						partp->x = savex;
						partp->y += r;
					}
					else if( nx != x && try_move( i, x, y, nx, y - r ) )
					{
						partp->x = savex;
						partp->y -= r;
					}
					else if( ptype->liquidfall && partp->vy > fabs( partp->vx ) )
					{
						found = 0;
						k = savestagnant ? sConfiguration.xres / 50 : sConfiguration.xres / 10;
						for( j = x + r; j >= 0 && j >= x - k && j < x + k && j < sConfiguration.xres; j += r )
						{
							if( spParticleMap[ y * sConfiguration.xres + j ].type &&
								spParticleMap[ y * sConfiguration.xres + j ].type != parti->type ||
								spCollisionMap[ y / sConfiguration.grid_size * sGridX + j / sConfiguration.grid_size ].type )
								break;

							if( try_move( i, x, y, j, ny ) )
							{
								partp->x += j - x;
								partp->y += ny - y;
								x = j;
								y = ny;
								found = 1;
								break;
							}
							if( try_move( i, x, y, j, y ) )
							{
								partp->x += j - x;
								x = j;
								found = 1;
								break;
							}
						}
						r = partp->vy > 0 ? 1 : -1;
						if( found )
						{
							k = savestagnant ? sConfiguration.yres / 50 : sConfiguration.yres / 10;
							for( j = y + r; j >= 0 && j < sConfiguration.yres && j >= y - k && j < y + k; j += r)
							{
								if( spParticleMap[ j * sConfiguration.xres + x ].type &&
									spParticleMap[ j * sConfiguration.xres + x ].type != parti->type ||
									spCollisionMap[ j / sConfiguration.grid_size * sGridX + x / sConfiguration.grid_size ].type )
								{
									found = 0;
									break;
								}
								if( try_move( i, x, y, x, j ) )
								{
									partp->y += j - y;
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
				partp->vx *= ptype->collision;
				partp->vy *= ptype->collision;
			}
		}

		x = ( int )( partpl->x + 0.5f );
		y = ( int )( partpl->y + 0.5f );
		nx = ( int )( partp->x + 0.5f );
		ny = ( int )( partp->y + 0.5f );
		if( nx < sConfiguration.grid_size || ny < sConfiguration.grid_size ||
			nx >= sConfiguration.xres - sConfiguration.grid_size ||
			ny >= sConfiguration.yres - sConfiguration.grid_size )
		{
			kill_part( parti, x, y, i );
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
			*out++ = partp->x + 1.0f;	// make line at least 1px length
			*out++ = partp->y;
			stored_count++;
		}
		i++;
		parti++;
		partp++;
		partpl++;
	}
}

void solver_cpu_st_collision_set( int x, int y, int collision_type )
{
	int gridx = x / sConfiguration.grid_size;
	int gridy = y / sConfiguration.grid_size;

	if( gridx < 0 || gridx >= sGridX || gridy < 0 || gridy >= sGridY )
		return;

	spCollisionMap[ gridy * sGridX + gridx ].type = collision_type;
}