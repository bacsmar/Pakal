$input v_normal, v_texcoord0, v_worldpos, v_texcoord1, v_localpos

#include <bgfx_shader.sh>
#include "pakal_lighting.sh"

SAMPLER2D(s_texColor, 0);

uniform vec4 u_lightDir;   // xyz: direction the light travels (from the light into the scene)
uniform vec4 u_lightColor; // rgb: light color
uniform vec4 u_ambient;    // rgb: ambient color from above (the sky)
uniform vec4 u_ambientGround; // rgb: ambient color from below (the light bounced by the ground)
uniform vec4 u_tint;       // material color multiplier
uniform vec4 u_material;   // x: alpha test reference (0 = off), y: keep the texture alpha, z: unlit
uniform vec4 u_ground;     // x: how much a second, turned read of the texture breaks its tiling, y: the macro variation (0: off)

// Value noise in 3D (0..1): smooth bumps one unit apart
float clay_hash(vec3 p)
{
	p = fract(p * 0.3183099 + vec3(0.71, 0.113, 0.419));
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float clay_noise(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	vec3 u = f * f * (3.0 - 2.0 * f);
	float a = mix(mix(clay_hash(i), clay_hash(i + vec3(1.0, 0.0, 0.0)), u.x),
		mix(clay_hash(i + vec3(0.0, 1.0, 0.0)), clay_hash(i + vec3(1.0, 1.0, 0.0)), u.x), u.y);
	float b = mix(mix(clay_hash(i + vec3(0.0, 0.0, 1.0)), clay_hash(i + vec3(1.0, 0.0, 1.0)), u.x),
		mix(clay_hash(i + vec3(0.0, 1.0, 1.0)), clay_hash(i + vec3(1.0, 1.0, 1.0)), u.x), u.y);
	return mix(a, b, u.z);
}

// Hand-shaped clay: broad dents and finer marks of the fingers
float clay_surface(vec3 p)
{
	return 0.65 * clay_noise(p) + 0.35 * clay_noise(p * 2.7 + vec3(3.1, 7.7, 1.3));
}

void main()
{
	vec4 texel = texture2D(s_texColor, v_texcoord0);
	if (u_ground.x + u_ground.y > 0.0)
	{
		texel = ground_detile(texel, texture2D(s_texColor, ground_second_uv(v_texcoord0)), v_worldpos.xz, u_ground);
	}
	texel = paint_texel(texel, v_texcoord0);
	vec4 albedo = texel * u_tint;
	// the cutout follows the texture alone: a faded model (u_tint.a) keeps its leaves
	if (u_material.x > 0.0 && texel.a < u_material.x)
	{
		discard;
	}
	// a model faded by dithering leaves out the pixels of a 4x4 pattern above its opacity (u_material.w)
	if (u_material.w > 0.0)
	{
		vec2 p = mod(floor(gl_FragCoord.xy), 4.0);
		vec2 a = mod(p, 2.0);
		vec2 b = floor(p * 0.5);
		float inner = 2.0 * a.x + 3.0 * a.y - 4.0 * a.x * a.y;
		float outer = 2.0 * b.x + 3.0 * b.y - 4.0 * b.x * b.y;
		if ((4.0 * inner + outer + 0.5) / 16.0 > u_material.w)
		{
			discard;
		}
	}

	vec3 n = normalize(v_normal);
	float clay = u_style.z;
	float mottle = 1.0;
	if (clay > 0.0)
	{
		// Plasticine: the surface dents of clay shaped by hand bend the normal (the slope of the noise, taken in the model's own
		// space so the marks stay put as it moves), and the color is a little uneven where it was pressed
		vec3 p = v_localpos * u_style.w;
		float e = 0.12;
		float h = clay_surface(p);
		vec3 slope = vec3(clay_surface(p + vec3(e, 0.0, 0.0)) - h, clay_surface(p + vec3(0.0, e, 0.0)) - h,
			clay_surface(p + vec3(0.0, 0.0, e)) - h) / e;
		n = normalize(n - clay * 0.32 * (slope - n * dot(slope, n)));
		mottle = 1.0 + clay * 0.14 * (h - 0.5);
	}
	float visibility = sun_visibility(v_worldpos, n);
	// The light wraps a little past the terminator so shadows are soft (clay, more: the light soaks into it); the ambient
	// leans from the ground color to the sky's
	float wrap = mix(0.25, 0.6, clay);
	float ndotl = clamp((dot(n, -normalize(u_lightDir.xyz)) + wrap) / (1.0 + wrap), 0.0, 1.0);
	vec3 ambient = ambient_light(v_worldpos, n, u_ambient.rgb, u_ambientGround.rgb);
	apply_lightmap(v_texcoord1, ambient, visibility);
	vec3 lit = ambient + u_lightColor.rgb * ndotl * visibility;
	// clay lets a little warm, deep light through on its shadow side
	lit += clay * 0.18 * u_lightColor.rgb * albedo.rgb * (1.0 - ndotl);
	vec3 color = albedo.rgb * mottle * mix(lit, vec3_splat(1.0), u_material.z);

	// Plasticine is matte: a broad, faint sheen where the sun hits, and the edges a touch darker (no plastic highlight)
	if (clay > 0.0 && u_material.z < 0.5)
	{
		vec3 eye = mul(u_invView, vec4(0.0, 0.0, 0.0, 1.0)).xyz;
		vec3 v = normalize(eye - v_worldpos);
		vec3 h = normalize(v - normalize(u_lightDir.xyz));
		float sheen = pow(max(dot(n, h), 0.0), 10.0) * visibility;
		float edge = pow(1.0 - max(dot(n, v), 0.0), 2.0);
		color += clay * 0.1 * u_lightColor.rgb * sheen;
		color *= 1.0 - clay * 0.18 * edge;
	}
	// The plastic toy look: a tight, bright highlight of the sun and a faint sheen at the edges (unlit materials keep none)
	else if (u_style.x > 0.0 && u_material.z < 0.5)
	{
		vec3 eye = mul(u_invView, vec4(0.0, 0.0, 0.0, 1.0)).xyz;
		vec3 v = normalize(eye - v_worldpos);
		vec3 h = normalize(v - normalize(u_lightDir.xyz));
		float highlight = pow(max(dot(n, h), 0.0), u_style.y) * visibility;
		float sheen = pow(1.0 - max(dot(n, v), 0.0), 3.0) * 0.25;
		color += u_style.x * (u_lightColor.rgb * highlight + ambient * sheen);
	}

	// Opaque materials write alpha 1: their texture alpha is a mask of the source art (gloss),
	// not coverage, and writing it would leave holes in captures of the frame.
	gl_FragColor = vec4(color, mix(1.0, albedo.a, u_material.y));
}
