$input a_position, a_texcoord0
$output v_texcoord0

#include <bgfx_shader.sh>

// Depth from the light with the texture's alpha cut out (a shadow proxy's quads: MeshComponent3D_Bgfx::set_shadow_only)
void main()
{
	gl_Position = mul(u_viewProj, mul(u_model[0], vec4(a_position, 1.0)));
	v_texcoord0 = a_texcoord0;
}
