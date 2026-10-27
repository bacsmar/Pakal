$input a_position, a_normal, a_texcoord0, a_indices, a_weight
$output v_normal, v_texcoord0, v_worldpos, v_texcoord1, v_localpos

// Size of the u_model palette. bgfx_shader.sh defaults to 1, which leaves every bone but the
// first unset; must match kMaxPaletteEntries in MeshComponent3D_Bgfx.cpp.
#define BGFX_CONFIG_MAX_BONES 64
#include <bgfx_shader.sh>

// Bone palette: u_model[i] already includes the object transform (set by MeshComponent3D_Bgfx)
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
	v_texcoord0 = a_texcoord0;
	v_texcoord1 = vec2(0.0, 0.0); // characters have no lightmap
	v_localpos = a_position;
}
