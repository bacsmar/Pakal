$input a_position, i_data0, i_data1, i_data2, i_data3, i_data4
$output v_texcoord0, v_color0

// A particle the GPU moves (cs_particles), drawn as vs_particle draws one: a square facing the camera, its size and alpha
// from start to end over its life. a_position.xy is the corner (-1..1). The buffer's dead ones, and the ones of the other
// blend (u_gpuParticle.x: 1 this draw is the additive ones), are pushed out of the picture.

#include <bgfx_shader.sh>

uniform vec4 u_gpuParticle;

void main()
{
	float t = min(i_data0.w / max(i_data1.w, 0.0001), 1.0);
	bool alive = i_data0.w < i_data1.w;
	bool mine = (i_data4.x > 0.5) == (u_gpuParticle.x > 0.5);
	float radius = mix(i_data3.x, i_data3.y, t);
	float alpha = clamp(mix(i_data3.z, i_data3.w, t), 0.0, 1.0);
	vec4 view = mul(u_view, vec4(i_data0.xyz + vec3(0.0, radius * 0.5, 0.0), 1.0));
	view.xy += a_position.xy * radius;
	gl_Position = mul(u_proj, view);
	if (!(alive && mine))
	{
		gl_Position = vec4(0.0, 0.0, -2.0, 1.0);
	}
	v_texcoord0 = a_position.xy;
	v_color0 = vec4(i_data2.rgb, alpha);
}
