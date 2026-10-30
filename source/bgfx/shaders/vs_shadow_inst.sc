$input a_position, i_data0, i_data1, i_data2, i_data3
$output v_dummy

#include <bgfx_shader.sh>

// Depth only, from the light, for many copies of one model in a single draw (see vs_pbr_inst)
void main()
{
	mat4 model = mtxFromCols(i_data0, i_data1, i_data2, i_data3);
	gl_Position = mul(u_viewProj, mul(model, vec4(a_position, 1.0)));
	v_dummy = 0.0;
}
