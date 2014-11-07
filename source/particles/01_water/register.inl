assert( i == 01 );

spParticleTypes[i].name = "water";
spParticleTypes[i].airloss = 0.64f;
spParticleTypes[i].airdrag = 0.8f;
spParticleTypes[i].hotair = 0.00f;
spParticleTypes[i].vloss = 0.5f;
spParticleTypes[i].advection = 4.9f;
spParticleTypes[i].gravity = 86.0f;
spParticleTypes[i].hconduct = 0.0f;//1.0f;
spParticleTypes[i].move_type = MT_LIQUID;
spParticleTypes[i].collision = 0.0f;
spParticleTypes[i].initial_temp = 20.0;
spParticleTypes[i].diffusion = 0.0;

i++;