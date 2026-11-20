$input a_position, a_normal, a_texcoord0, a_tangent, a_indices, a_weight
$output v_normal, v_texcoord0, v_worldpos, v_tangent, v_texcoord1

// A skinned model with a metal-rough material (a normal map and an ORM texture): vs_skin's palette, and the tangent frame
// fs_pbr needs, carried through the same bones. Its varyings are fs_pbr's (varying_pbr.def.sc).
#define BGFX_CONFIG_MAX_BONES 64
#include <bgfx_shader.sh>

void main()
{
	mat4 skin = u_model[int(a_indices.x)] * a_weight.x
		+ u_model[int(a_indices.y)] * a_weight.y
		+ u_model[int(a_indices.z)] * a_weight.z
		+ u_model[int(a_indices.w)] * a_weight.w;

	vec4 worldPos = mul(skin, vec4(a_position, 1.0));
	gl_Position = mul(u_viewProj, worldPos);
	v_worldpos = worldPos.xyz;
	v_normal = normalize(mul(skin, vec4(a_normal, 0.0)).xyz);
	v_tangent = normalize(mul(skin, vec4(a_tangent, 0.0)).xyz);
	v_texcoord0 = a_texcoord0;
	v_texcoord1 = vec2(0.0, 0.0); // characters have no lightmap
}
