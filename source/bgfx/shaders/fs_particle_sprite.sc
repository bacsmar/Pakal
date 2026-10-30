$input v_texcoord0, v_texcoord1, v_color0

// The CPU particles: a sprite of the particle sheet tinted by the particle's color (its alpha times the sprite's), or,
// without one, the soft round dot of fs_particle

#include <bgfx_shader.sh>

SAMPLER2D(s_texParticle, 0);

void main()
{
	if (v_texcoord1.z > 0.5)
	{
		vec4 texel = texture2D(s_texParticle, v_texcoord1.xy);
		gl_FragColor = vec4(v_color0.rgb * texel.rgb, v_color0.a * texel.a);
	}
	else
	{
		float r = length(v_texcoord0);
		float alpha = v_color0.a * (1.0 - smoothstep(0.35, 1.0, r));
		gl_FragColor = vec4(v_color0.rgb, alpha);
	}
}
