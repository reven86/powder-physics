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
    unsigned int stagnant : 1;  //!< Cached value of particle stagnant state.
	unsigned int index : 22;	//!< Index of particle in particles array.
	unsigned int collision : 1;	//!< Is this particle collision particle?
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

__inline int try_move( int i, int x, int y, int nx, int ny )
{
	i;

	if( x == nx && y == ny )
		return 1;

	if( nx < 1 || ny < 1 || nx >= sConfiguration.xres - 1 || ny >= sConfiguration.yres - 1 )
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

__inline int fast_ftol( float x )
{
    //return ( int ) floorf( x );

    unsigned e = (0x7F + 31) - ((* (unsigned*) &x & 0x7F800000) >> 23);
    unsigned m = 0x80000000 | (* (unsigned*) &x << 8);

    return (int)((m >> e) & -(e < 32));
}

void solver_cpu_st_update( pp_time_t dt )
{
	struct PPParticleInfo * parti;
	struct PPParticlePhysInfo * partp, *partpl;
	struct PPAirParticle * air;
	struct PPParticleType * ptype;
    struct PPParticleMap * tempp = NULL, * n, * ne, * e, * se, * s, * sw, * w, * nw;
	int i, j, k, r, npart;
	int x, y, nx = 0, ny = 0;
	int gridx, gridy;
	int found, savestagnant;
	float loss_factor;
	float sdt = FLT_SECOND * dt;
	float accum_heat;
	float savex, savey;
	float dx, dy, absdx, absdy;
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

		x = fast_ftol( partpl->x );
		y = fast_ftol( partpl->y );

        assert( ( int ) spParticleMap[ y * sConfiguration.xres + x ].index == i );
        assert( spParticleMap[ y * sConfiguration.xres + x ].stagnant == parti->stagnant );

		if( parti->life >= 0 )
		{
			parti->life -= dt;
			if( parti->life <= 0 )
			{
				kill_part( parti, x, y, i );
				continue;
			}
		}

        assert( !( x < 1 || y < 1 || x >= sConfiguration.xres - 1 || y >= sConfiguration.yres - 1 ) );

        tempp = spParticleMap + y * sConfiguration.xres + x;
        n = tempp - sConfiguration.xres;
        ne = n + 1;
        e = tempp + 1;
        se = e + sConfiguration.xres;
        s = se - 1;
        sw = s - 1;
        w = tempp - 1;
        nw = n - 1;

		//
		// handle temperature
		//

		ptype = spParticleTypes + parti->type;
		if( ptype->hconduct > 0.0f )
		{
			accum_heat = 0.0f;
			heat_count = 0;

            if( n->type )
            {
                accum_heat += ( spParticlesPhysInfoLast + n->index )->temp;
                heat_count++;
            }
            if( ne->type )
            {
                accum_heat += ( spParticlesPhysInfoLast + ne->index )->temp;
                heat_count++;
            }
            if( e->type )
            {
                accum_heat += ( spParticlesPhysInfoLast + e->index )->temp;
                heat_count++;
            }
            if( se->type )
            {
                accum_heat += ( spParticlesPhysInfoLast + se->index )->temp;
                heat_count++;
            }
            if( s->type )
            {
                accum_heat += ( spParticlesPhysInfoLast + s->index )->temp;
                heat_count++;
            }
            if( sw->type )
            {
                accum_heat += ( spParticlesPhysInfoLast + sw->index )->temp;
                heat_count++;
            }
            if( w->type )
            {
                accum_heat += ( spParticlesPhysInfoLast + w->index )->temp;
                heat_count++;
            }
            if( nw->type )
            {
                accum_heat += ( spParticlesPhysInfoLast + nw->index )->temp;
                heat_count++;
            }

			if( heat_count > 0 )
				partp->temp = partpl->temp + ( accum_heat / heat_count - partpl->temp ) * ptype->hconduct * sdt;
		}
        else
        {
            heat_count = n->type ? 1 : 0;
            if( ne->type )
                heat_count++;
            if( e->type )
                heat_count++;
            if( se->type )
                heat_count++;
            if( s->type )
                heat_count++;
            if( sw->type )
                heat_count++;
            if( w->type )
                heat_count++;
            if( nw->type )
                heat_count++;
        }

        parti->blocked = heat_count == 8 &&
            n->stagnant && ne->stagnant && e->stagnant &&
            se->stagnant && s->stagnant && sw->stagnant &&
            w->stagnant && nw->stagnant;

		//
		// handle particle update, based on its type and neighbours
		//

#include "particles/update_cpu_st.inl"

		partp->x = partpl->x;
		partp->y = partpl->y;
        if( parti->blocked )
        {
            parti->stagnant = 1;
			parti->freefall = 0;
    		spParticleMap[ y * sConfiguration.xres + x ].stagnant = parti->stagnant;
			processed_count++;
            continue;
        }

        gridx = x / sConfiguration.grid_size;
		gridy = y / sConfiguration.grid_size;

		air = spAir + gridy * sGridX + gridx;

		assert( ptype->move_type != MT_IMMOVABLE );
		assert( !air->type );

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
		// handle position
		//

		// trace against collisions
		dx = partp->vx * sdt;
		dy = partp->vy * sdt;
        absdx = fabsf( dx );
        absdy = fabsf( dy );
		maxv = absdx > absdy ? absdx : absdy;
		k = fast_ftol( maxv + 1 );
		dx /= k;
		dy /= k;
		for( j = 0; j < k; j++ )
		{
			partp->x += dx;
			partp->y += dy;
			nx = fast_ftol( partp->x );
			ny = fast_ftol( partp->y );
			if( nx < 1 || nx >= sConfiguration.xres - 1 || 
				ny < 1 || ny >= sConfiguration.yres - 1 )
				break;

            tempp = spParticleMap + ny * sConfiguration.xres + nx;
			if( tempp->collision )
				break;
		}

		if( nx < 1 || nx >= sConfiguration.xres - 1 || 
			ny < 1 || ny >= sConfiguration.yres - 1 )
        {
			kill_part( parti, x, y, i );
			continue;
        }

		if( x == nx && y == ny )
		{
			processed_count++;
			continue;
		}

		savestagnant = parti->stagnant;
		parti->stagnant = 0;
		parti->freefall = 1;
		if( tempp->type )
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
						partp->x = ( float )( x + r );
						partp->y = savey;
						assert( !incollision( partp ) );
					}
					else if( ny != y && try_move( i, x, y, x - r, ny ) )
					{
						partp->x = ( float )( x - r );
						partp->y = savey;
						assert( !incollision( partp ) );
					}
					else if( nx != x && try_move( i, x, y, nx, y + r ) )
					{
						partp->x = savex;
						partp->y = ( float )( y + r );
						assert( !incollision( partp ) );
					}
					else if( nx != x && try_move( i, x, y, nx, y - r ) )
					{
						partp->x = savex;
						partp->y = ( float )( y - r );
						assert( !incollision( partp ) );
					}
					else if( ptype->move_type == MT_LIQUID && partp->vy > fabs( partp->vx ) )
					{
						found = 0;
						k = savestagnant ? 10 : 50;
                        tempp = spParticleMap + y * sConfiguration.xres + x;
						for( j = x + r; j >= 0 && j >= x - k && j < x + k && j < sConfiguration.xres; j += r )
						{
                            tempp += r;
							if( tempp->type && tempp->type != parti->type )
								break;

							if( try_move( i, x, y, j, ny ) )
							{
								partp->x = ( float )( j );
								partp->y = ( float )( ny );
								x = j;
								y = ny;
								found = 1;
								assert( !incollision( partp ) );
								break;
							}
							if( try_move( i, x, y, j, y ) )
							{
								partp->x = ( float )( j );
								x = j;
								found = 1;
								assert( !incollision( partp ) );
								break;
							}
						}
						if( found )
						{
    						r = partp->vy > 0 ? 1 : -1;
                            tempp = spParticleMap + y * sConfiguration.xres + x;
							for( j = y + r; j >= 0 && j < sConfiguration.yres && j >= y - k && j < y + k; j += r )
							{
                                tempp += r * sConfiguration.xres;
    							if( tempp->type && tempp->type != parti->type )
								{
									found = 0;
									break;
								}
								if( try_move( i, x, y, x, j ) )
								{
									partp->y = ( float )( j );
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

		x = fast_ftol( partpl->x );
		y = fast_ftol( partpl->y );
		nx = fast_ftol( partp->x );
		ny = fast_ftol( partp->y );

        if( x == nx && y == ny )
        {
    		spParticleMap[ y * sConfiguration.xres + x ].stagnant = parti->stagnant;
    		processed_count++;
            continue;
        }

        if( nx < 1 || ny < 1 ||
			nx >= sConfiguration.xres - 1 ||
			ny >= sConfiguration.yres - 1 )
		{
			kill_part( parti, x, y, i );
			continue;
		}

		assert( !incollision( partp ) );
        assert( spParticleMap[ ny * sConfiguration.xres + nx ].type == 0 );
        assert( ( int ) spParticleMap[ y * sConfiguration.xres + x ].index == i );

		spParticleMap[ y * sConfiguration.xres + x ].type = 0;
		spParticleMap[ ny * sConfiguration.xres + nx ].type = parti->type;
		spParticleMap[ ny * sConfiguration.xres + nx ].index = i;
		spParticleMap[ ny * sConfiguration.xres + nx ].stagnant = parti->stagnant;

    	processed_count++;
    }

#ifdef _DEBUG
    // check consistency
    processed_count = 0;
	parti = spParticlesInfo;
	partpl = spParticlesPhysInfoLast;
	partp = spParticlesPhysInfo;

	for( i = 0; i < npart && processed_count < sParticleAliveCount; i++, parti++, partp++, partpl++ )
	{
		if( !parti->type )
			continue;

		x = fast_ftol( partp->x );
		y = fast_ftol( partp->y );

        assert( ( int ) spParticleMap[ y * sConfiguration.xres + x ].index == i );

        processed_count++;
    }
#endif
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
    parti->stagnant = 0;
    parti->blocked = 0;
    parti->freefall = 1;
	partp->x = ( float ) x;
	partp->y = ( float ) y;
	partp->vx = partp->vy = 0.0f;
	partp->temp = spParticleTypes[ type ].initial_temp;
	*partpl = *partp;

	pmap->index = index;
	pmap->type = type;
	pmap->collision = 0;
    pmap->stagnant = 0;

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
        spParticleMap[ y * sConfiguration.xres + x ].stagnant = 1;

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