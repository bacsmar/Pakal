$input a_position
$output v_dir

#include <bgfx_shader.sh>

void main()
{
	// Direction only (w = 0): the sky ignores camera translation. The fragment works with the world's direction (the
	// horizon, the sun and the clouds stay put as the camera turns)
	vec3 viewDir = mul(u_view, vec4(a_position, 0.0)).xyz;
	v_dir = a_position;
	vec4 pos = mul(u_proj, vec4(viewDir, 1.0));
	// Put the sky on the far plane.
	gl_Position = pos.xyww;
}
