#ifndef __POWDER_TYPES_H__
#define __POWDER_TYPES_H__




//! Log levels
enum PPLogLevel
{
	LOG_ERROR,
	LOG_WARNING,
	LOG_INFO,
	LOG_DEBUG
};

//! Log function.
typedef void (* PPLogFn) ( enum PPLogLevel level, const char * format, ... );




// Time represented in integers so 1 second equals to 10000.
typedef int pp_time_t;
#define SECOND 10000
#define FLT_SECOND 0.0001f




//! Configuration of the World
struct PPConfiguration
{
	int xres;		//!< X resolution. Total number of particles is xres * yres.
	int yres;		//!< Y resolution. Total number of particles is xres * yres.
	int grid_size;	//!< Size of one cell. Actual grid resolution is (xres / grid_size) x (yres / grid_size).

	PPLogFn	log_fn;	//!< Log function. If NULL, logging is disabled.
};



//! Physic constants
struct PPConstants
{
	float p_loss;			//!< Pressure loss per second. Default is 0.999f.
	float v_loss;			//!< Velocity loss per second. Default is 0.999f.
	float p_hstep;			//!< Pressure half dependency of velocity gradient per second. Default is 0.15f.
	float v_hstep;			//!< Velocity half dependency of velocity gradient per second. Default is 0.2f.
};




// Particle struct is split into several small structures
// in order to speed up data transfer operations during computations.

//! Particle common info. Dead particles maintain a linked list.
struct PPParticleInfo
{
	unsigned int type : 6;		//!< Particle's type (>0). If particle is dead, 'type' is 0.
	unsigned int stagnant : 1;	//!< Particle is stagnant.
    unsigned int blocked : 1;   //!< Particle is blocked by 8 neighbours. Only its heat is updated.
	unsigned int freefall : 1;	//!< Particle is free falling, e.g. not collided in last frame.
	pp_time_t life : 23;		//!< Particle remaining lifetime. If less than zero, particle is always alive. If particle is dead, used as index of next dead particle.
};

//! Particle physic info.
struct PPParticlePhysInfo
{
	float x;				//!< X coordinate.
	float y;				//!< Y coordinate.
	float vx;				//!< X velocity.
	float vy;				//!< Y velocity.
	float temp;				//!< Temperature.
};



enum PPMoveType
{
	MT_IMMOVABLE,		//!< Collision particle, doesn't participate in particles update.
	MT_NORMAL,			//!< Standard rigid particle.
	MT_POWDER,			//!< Powder particle.
	MT_LIQUID,			//!< Liquid particle.
};



//! Particle type information.
struct PPParticleType
{
	const char * name;				//!< Name.
	float airloss;					//!< Airloss per second.
	float airdrag;					//!< Airdrag per second.
	float hotair;					//!< How particle affect air pressure of neighbour grid cells per second.
	float vloss;					//!< Velocity loss per second.
	float advection;				//!< Advection per second.
	float gravity;					//!< Gravity per second.
	float hconduct;					//!< Heat conduct.
	float collision;				//!< Velocity change factor on collision.
	float initial_temp;				//!< Initital temperature.
	float diffusion;				//!< Chaotic part of velocity calculation.
	unsigned int move_type : 2;		//!< Particle move behavior (one of PPMoveType).
};



//! Air particle.
struct PPAirParticle
{
	unsigned int type : 8;	//!< Collision type.
	float vx;				//!< X velocity.
	float vy;				//!< Y velocity.
	float p;				//!< Pressure.
};


#endif // __POWDER_TYPES_H__