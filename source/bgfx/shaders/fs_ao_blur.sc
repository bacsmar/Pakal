$input v_texcoord0

// Ambient occlusion, its blur (BgfxGraphicsSystem::render_ao): a box of four texels along one direction (the width of fs_ao's 4x4 pattern),
// each weighed down by how far its depth is from the middle one's, so the shade does not bleed from a wall onto the floor in
// front of it or from a character onto the street behind. Run across and then down.

#include <bgfx_shader.sh>
#include "pakal_precision.sh"

SAMPLER2D(s_aoInput, 0);              // the occlusion, half size
PAKAL_SAMPLER2D_HIGH(s_aoDepth, 1);   // the frame's depth, full size (point sampled)

uniform vec4 u_ao0;   // x: 1 / the depth's width, y: 1 / its height, z: near plane, w: far plane
uniform vec4 u_ao2;   // zw: one step along the blur's direction (texels of the occlusion)

vec2 target_uv(vec2 uv)
{
#if BGFX_SHADER_LANGUAGE_GLSL
	return vec2(uv.x, 1.0 - uv.y);
#else
	return uv;
#endif
}

// (the same as fs_ao: the depth is read at the middle of one of its texels, not at the edge between two)
vec2 depth_texel_center(vec2 uv)
{
	return (floor(uv / u_ao0.xy + 0.25) + 0.5) * u_ao0.xy;
}

float linear_depth(vec2 uv)
{
	float d = texture2D(s_aoDepth, target_uv(depth_texel_center(uv))).x;
	return u_ao0.z * u_ao0.w / (u_ao0.w - d * (u_ao0.w - u_ao0.z));
}

void main()
{
	vec2 uv = v_texcoord0;
	vec2 stride = u_ao2.zw;
	float z = linear_depth(uv);
	// depths within a few percent of the middle one's count fully
	float tolerance = 1.0 / (0.03 * z + 0.05);
	// a box of four texels (weights 0.5, 1, 1, 1, 0.5 over five): the width of fs_ao's 4x4 pattern, which then cancels exactly
	float sum = texture2D(s_aoInput, target_uv(uv)).r;
	float weight = 1.0;
	for (int i = 1; i <= 2; ++i)
	{
		float g = i == 1 ? 1.0 : 0.5;
		for (int s = -1; s <= 1; s += 2)
		{
			vec2 suv = uv + stride * float(i * s);
			float w = g * max(1.0 - abs(linear_depth(suv) - z) * tolerance, 0.0);
			sum += texture2D(s_aoInput, target_uv(suv)).r * w;
			weight += w;
		}
	}
	float light = sum / weight;
	gl_FragColor = vec4(light, light, light, 1.0);
}
