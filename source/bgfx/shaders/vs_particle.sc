$input a_position, a_texcoord0, a_texcoord1, a_color0
$output v_texcoord0, v_texcoord1, v_color0

// A particle: a square that faces the camera. a_position is its center (world), a_texcoord0.xy the corner (-1..1) and
// a_texcoord0.z its half size (world units). With a_texcoord0.w = 1 the square lies flat on the floor instead (a spot of
// light or a stain: it spreads along world x and z). A flat one is pulled toward the eye along its own ray (it stays where it
// is on the screen): it still shows over a curb or a sidewalk a little higher than the floor it was put on.
// a_texcoord1: x the sprite (a cell of the particle sheet, counted from 1; 0 the soft dot), y its turn (radians).
// v_texcoord1: the sheet's uv at this corner, and z 1 for a sprite.

#include <bgfx_shader.sh>

uniform vec4 u_particleSheet; // x columns, y rows of the sprites' sheet

void main()
{
	float c = cos(a_texcoord1.y);
	float s = sin(a_texcoord1.y);
	vec2 corner = vec2(a_texcoord0.x * c - a_texcoord0.y * s, a_texcoord0.x * s + a_texcoord0.y * c);
	if (a_texcoord0.w > 0.5)
	{
		vec3 world = a_position + vec3(corner.x, 0.0, corner.y) * a_texcoord0.z;
		vec4 view = mul(u_view, vec4(world, 1.0));
		float distance = max(length(view.xyz), 0.5);
		view.xyz *= (distance - 0.2) / distance;
		gl_Position = mul(u_proj, view);
	}
	else
	{
		vec4 view = mul(u_view, vec4(a_position, 1.0));
		view.xy += corner * a_texcoord0.z;
		gl_Position = mul(u_proj, view);
	}
	v_texcoord0 = a_texcoord0.xy;
	float cell = max(a_texcoord1.x - 1.0, 0.0);
	float columns = max(u_particleSheet.x, 1.0);
	vec2 size = vec2(1.0 / columns, 1.0 / max(u_particleSheet.y, 1.0));
	vec2 origin = vec2(mod(cell, columns), floor(cell / columns)) * size;
	// the corner's texel: -1..1 to the cell (v down the sheet)
	v_texcoord1 = vec3(origin + (vec2(a_texcoord0.x, -a_texcoord0.y) * 0.5 + 0.5) * size, step(0.5, a_texcoord1.x));
	v_color0 = a_color0;
}
