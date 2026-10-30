// The GPU's particles (BgfxGraphicsSystem: gpu particles): every live one moves one frame, as EffectsSystem's puffs do on
// the CPU. Five vec4 each: position and age; velocity and life; color and gravity (negative rises); start and end
// radius and alpha; additive and bounce. Dead ones (age past life) are left as they are.

#include "bgfx_compute.sh"

BUFFER_RW(u_particles, vec4, 0);
uniform vec4 u_particleSim; // x: seconds this frame, y: how many particles the buffer holds

NUM_THREADS(64, 1, 1)
void main()
{
	uint i = gl_GlobalInvocationID.x;
	if (i >= uint(u_particleSim.y)) return;
	uint b = i * 5u;
	vec4 d0 = u_particles[b];
	vec4 d1 = u_particles[b + 1u];
	if (d0.w >= d1.w) return;
	vec4 d2 = u_particles[b + 2u];
	vec4 d4 = u_particles[b + 4u];
	float dt = u_particleSim.x;
	d0.w += dt;
	d1.y -= d2.w * dt;
	d0.xyz += d1.xyz * dt;
	if (d0.y <= 0.02)
	{
		// on the floor: a chunk bounces and slides, a drop or a puff stays there spreading out
		bool bounce = d4.y > 0.5;
		d0.y = 0.02;
		d1.y = (bounce && d1.y < -1.0) ? -d1.y * 0.35 : 0.0;
		float keep = bounce ? 0.7 : 0.9;
		d1.x *= keep;
		d1.z *= keep;
	}
	u_particles[b] = d0;
	u_particles[b + 1u] = d1;
}
