$input v_texcoord0

// Bloom, its first step: what is bright in the frame, a quarter of its size. Each pixel takes four filtered taps a source
// texel off its center (the 4x4 texels it covers), each kept by how far its brightness goes past the threshold (soft
// knee), so a lone bright pixel does not flicker

#include <bgfx_shader.sh>

SAMPLER2D(s_bloomInput, 0);

uniform vec4 u_bloom;      // xy: one texel of the input, z: the threshold (brightness 0..1), w: the knee's width

// The pass thinks of uv as running downward from the top of the picture. Render targets of OpenGL start at the bottom.
vec2 target_uv(vec2 uv)
{
#if BGFX_SHADER_LANGUAGE_GLSL
	return vec2(uv.x, 1.0 - uv.y);
#else
	return uv;
#endif
}

vec3 bright(vec2 uv)
{
	vec3 color = texture2D(s_bloomInput, target_uv(uv)).rgb;
	float luma = max(color.r, max(color.g, color.b));
	return color * smoothstep(u_bloom.z, u_bloom.z + u_bloom.w, luma);
}

void main()
{
	vec2 uv = v_texcoord0;
	vec2 t = u_bloom.xy;
	vec3 sum = bright(uv + vec2(-t.x, -t.y)) + bright(uv + vec2(t.x, -t.y)) + bright(uv + vec2(-t.x, t.y)) + bright(uv + vec2(t.x, t.y));
	gl_FragColor = vec4(sum * 0.25, 1.0);
}
