$input v_texcoord0, v_color0

// A soft round dot: full in the middle, fading to nothing at the rim

#include <bgfx_shader.sh>

void main()
{
	float r = length(v_texcoord0);
	float alpha = v_color0.a * (1.0 - smoothstep(0.35, 1.0, r));
	gl_FragColor = vec4(v_color0.rgb, alpha);
}
