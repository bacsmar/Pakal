$input a_position, a_normal, a_texcoord0, a_tangent, a_texcoord1, i_data0, i_data1, i_data2, i_data3
$output v_normal, v_texcoord0, v_worldpos, v_tangent, v_texcoord1

#include <bgfx_shader.sh>

// vs_pbr for many copies of one model in a single draw: each copy's model matrix comes with the instance (i_data0..3, its
// columns) instead of u_model. Instanced models have no lightmap (v_texcoord1 stays at the atlas' unused corner)
uniform vec4 u_uvScroll; // xy: the texture's offset now (a material that flows, as water); 0 for the rest

void main()
{
	mat4 model = mtxFromCols(i_data0, i_data1, i_data2, i_data3);
	vec4 worldPos = mul(model, vec4(a_position, 1.0));
	gl_Position = mul(u_viewProj, worldPos);
	v_worldpos = worldPos.xyz;
	v_normal = normalize(mul(model, vec4(a_normal, 0.0)).xyz);
	v_tangent = normalize(mul(model, vec4(a_tangent, 0.0)).xyz);
	v_texcoord0 = a_texcoord0 + u_uvScroll.xy;
	v_texcoord1 = vec2(0.0, 0.0);
}
