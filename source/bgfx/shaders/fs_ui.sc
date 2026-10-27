$input v_color0, v_texcoord0

// Premultiplied alpha: the vertex color and the texture (a white texel when there is none) are both premultiplied

#include <bgfx_shader.sh>

SAMPLER2D(s_uiTexture, 0);

void main()
{
	gl_FragColor = v_color0 * texture2D(s_uiTexture, v_texcoord0);
}
