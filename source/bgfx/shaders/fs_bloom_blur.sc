$input v_texcoord0

// Bloom, its blur: nine texels along one direction with a Gaussian's weights, in five filtered taps (each pair of side
// texels is one tap between them). Run across and then down; a wider step spreads the glow further

#include <bgfx_shader.sh>

SAMPLER2D(s_bloomInput, 0);

uniform vec4 u_bloom;      // xy: one step along the blur's direction (texels of the input times the spread)

// The pass thinks of uv as running downward from the top of the picture. Render targets of OpenGL start at the bottom.
vec2 target_uv(vec2 uv)
{
#if BGFX_SHADER_LANGUAGE_GLSL
	return vec2(uv.x, 1.0 - uv.y);
#else
	return uv;
#endif
}

void main()
{
	vec2 uv = v_texcoord0;
	vec2 stride = u_bloom.xy;
	vec3 sum = texture2D(s_bloomInput, target_uv(uv)).rgb * 0.2270270;
	sum += texture2D(s_bloomInput, target_uv(uv + stride * 1.3846154)).rgb * 0.3162162;
	sum += texture2D(s_bloomInput, target_uv(uv - stride * 1.3846154)).rgb * 0.3162162;
	sum += texture2D(s_bloomInput, target_uv(uv + stride * 3.2307692)).rgb * 0.0702703;
	sum += texture2D(s_bloomInput, target_uv(uv - stride * 3.2307692)).rgb * 0.0702703;
	gl_FragColor = vec4(sum, 1.0);
}
