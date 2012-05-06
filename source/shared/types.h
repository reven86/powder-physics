#pragma once



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
	float v_smooth;			//!< Velocity smooth transfer. Default is 0.7f.
};




// Time represented in integers so 1 second equals to 10000.
typedef int pp_time_t;
#define SECOND 10000
#define FLT_SECOND 0.0001f


// Particle struct is split into several small structures
// in order to speed up data transfer operations during computations.

//! Particle common info. Dead particles maintain a linked list.
struct PPParticleInfo
{
	unsigned int type : 7;		//!< Particle's type (>0). If particle is dead, 'type' is 0.
	unsigned int stagnant : 1;	//!< Particle is stagnant.
	pp_time_t life : 24;		//!< Particle remaining lifetime. If less than zero, particle is always alive. If particle is dead, used as index of next dead particle.
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
	unsigned int powderfall : 1;	//!< Particle is falling like a powder.
	unsigned int liquidfall : 1;	//!< Particle is falling like a liquid, allowed to moving in horizontal directions.
};
