assert( i == 03 );

spParticleTypes[i].name = "steam";
spParticleTypes[i].airloss = 0.99f;
spParticleTypes[i].airdrag = 0.6f;
spParticleTypes[i].hotair = 10.0f;
spParticleTypes[i].vloss = 0.3f;
spParticleTypes[i].advection = 2.0f;
spParticleTypes[i].gravity = -15.0f;
spParticleTypes[i].hconduct = 1.0f;
spParticleTypes[i].move_type = MT_NORMAL;
spParticleTypes[i].collision = -0.9f;
spParticleTypes[i].initial_temp = 120.0f;
spParticleTypes[i].diffusion = 100.0f;

i++;