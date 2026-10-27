$input a_position
$output v_dir

#include <bgfx_shader.sh>

void main()
{
	// Direction only (w = 0): the sky ignores camera translation.
	v_dir = mul(u_view, vec4(a_position, 0.0)).xyz;
	vec4 pos = mul(u_proj, vec4(v_dir, 1.0));
	// Put the sky on the far plane.
	gl_Position = pos.xyww;
}
