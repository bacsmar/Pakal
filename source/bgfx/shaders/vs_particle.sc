$input a_position, a_texcoord0, a_color0
$output v_texcoord0, v_color0

// A particle: a square that faces the camera. a_position is its center (world), a_texcoord0.xy the corner (-1..1) and
// a_texcoord0.z its half size (world units).

#include <bgfx_shader.sh>

void main()
{
	vec4 view = mul(u_view, vec4(a_position, 1.0));
	view.xy += a_texcoord0.xy * a_texcoord0.z;
	gl_Position = mul(u_proj, view);
	v_texcoord0 = a_texcoord0.xy;
	v_color0 = a_color0;
}
