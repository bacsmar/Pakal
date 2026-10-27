$input a_position, a_indices, a_weight
$output v_dummy

#define BGFX_CONFIG_MAX_BONES 64
#include <bgfx_shader.sh>

// The sun's view of the characters: the same bone palette as vs_skin
void main()
{
	mat4 skin = u_model[int(a_indices.x)] * a_weight.x
		+ u_model[int(a_indices.y)] * a_weight.y
		+ u_model[int(a_indices.z)] * a_weight.z
		+ u_model[int(a_indices.w)] * a_weight.w;
	gl_Position = mul(u_viewProj, mul(skin, vec4(a_position, 1.0)));
	v_dummy = 0.0;
}
