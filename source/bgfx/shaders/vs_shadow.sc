$input a_position
$output v_dummy

#include <bgfx_shader.sh>

// Depth only, from the light: the sun's view of the static models
void main()
{
	gl_Position = mul(u_viewProj, mul(u_model[0], vec4(a_position, 1.0)));
	v_dummy = 0.0;
}
