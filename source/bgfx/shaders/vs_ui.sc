$input a_position, a_color0, a_texcoord0
$output v_color0, v_texcoord0

// The game's interface (RmlUi): 2D vertices in pixels, moved by u_uiOffset (the geometry's translation) and by the
// transform of the element (u_model), then into the screen with the view's orthographic projection.

#include <bgfx_shader.sh>

uniform vec4 u_uiOffset;

void main()
{
	vec4 position = mul(u_model[0], vec4(a_position + u_uiOffset.xy, 0.0, 1.0));
	gl_Position = mul(u_viewProj, position);
	v_color0 = a_color0;
	v_texcoord0 = a_texcoord0;
}
