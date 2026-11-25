$input v_normal, v_color0, v_worldpos, v_texcoord0

#include <bgfx_shader.sh>

// Grass made on the GPU (vs_grass): solid, no texture, no alpha test (the GPU's hidden surface removal works on it). Lit by
// the sky and the ground's ambient and a wrapped sun, darker at the root (the shade between the blades); the sun as
// vs_grass found it at the blade's root (v_color0.a: 1 lit, 0 in a shadow).
uniform vec4 u_lightDir;      // xyz: the direction the light travels
uniform vec4 u_lightColor;    // rgb
uniform vec4 u_ambient;       // rgb: from above
uniform vec4 u_ambientGround; // rgb: from below

void main()
{
	vec3 n = normalize(v_normal);
	float t = v_texcoord0.x;
	vec3 toLight = -u_lightDir.xyz;
	float sun = clamp((dot(n, toLight) + 0.45) / 1.45, 0.0, 1.0);
	vec3 ambient = mix(u_ambientGround.rgb, u_ambient.rgb, 0.5 + 0.5 * n.y);
	float shade = mix(0.4, 1.0, t);
	gl_FragColor = vec4(v_color0.rgb * (ambient * shade + u_lightColor.rgb * sun * v_color0.a * mix(0.55, 1.0, t)), 1.0);
}
