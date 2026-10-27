$input v_dir

#include <bgfx_shader.sh>

uniform vec4 u_skyTop;
uniform vec4 u_skyHorizon;

void main()
{
	float t = clamp(normalize(v_dir).y * 0.5 + 0.5, 0.0, 1.0);
	gl_FragColor = vec4(mix(u_skyHorizon.rgb, u_skyTop.rgb, t), 1.0);
}
