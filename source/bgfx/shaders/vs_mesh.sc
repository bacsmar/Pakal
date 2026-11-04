$input a_position, a_normal, a_texcoord0, a_texcoord1
$output v_normal, v_texcoord0, v_worldpos, v_texcoord1, v_localpos

#include <bgfx_shader.sh>

uniform vec4 u_lightmap; // x: scale, yz: corner of this instance's place in the lightmap atlas
uniform vec4 u_uvScroll; // xy: the texture's offset now (a material that flows, as water); 0 for the rest

void main()
{
	vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));
	gl_Position = mul(u_viewProj, worldPos);
	v_worldpos = worldPos.xyz;
	v_normal = normalize(mul(u_model[0], vec4(a_normal, 0.0)).xyz);
	v_texcoord0 = a_texcoord0 + u_uvScroll.xy;
	v_texcoord1 = u_lightmap.yz + a_texcoord1 * u_lightmap.x;
	v_localpos = a_position;
}
