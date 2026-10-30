// Shared by the lit mesh shaders: the sun's shadow
#ifndef PAKAL_LIGHTING_SH
#define PAKAL_LIGHTING_SH

#include "pakal_precision.sh"

PAKAL_SAMPLER2DSHADOW_HIGH(s_shadowMap, 4);   // compared by the sampler (BGFX_SAMPLER_COMPARE_LEQUAL), filtered

uniform mat4 u_shadowMtx;     // world to the sun's view, with the texture's offset and scale (and depth) already applied
uniform vec4 u_shadowParams;  // x: strength (0: no shadows), y: depth bias, z: 1 / map size, w: offset along the normal (meters)

PAKAL_SAMPLER3D_HIGH(s_giVolume, 5);

uniform vec4 u_giOrigin;      // xyz: corner of the baked grid (world space), w: how much of the ambient comes from it (0: none)
uniform vec4 u_giSize;        // xyz: size of the grid in world space (the sign flips an axis)
uniform vec4 u_lightmap;      // xyz: this instance's place in the atlas (the vertex shader uses it), w: 0 none, 1 + baked shadow strength

// The ambient light at a point: the flat sky/ground blend, or the baked global illumination (light bounced around the scene,
// dark in alleys and under walls). The baked grid holds half of the light, so the sample is doubled.
vec3 ambient_light(vec3 worldPos, vec3 n, vec3 sky, vec3 ground)
{
	vec3 hemisphere = mix(ground, sky, n.y * 0.5 + 0.5);
	// (a lightmapped surface takes its ambient from the lightmap: the grid is not read for it)
	if (u_giOrigin.w > 0.0 && u_lightmap.w <= 0.0)
	{
		vec3 uvw = vec3(worldPos.x - u_giOrigin.x, worldPos.z - u_giOrigin.z, worldPos.y - u_giOrigin.y) / vec3(u_giSize.x, u_giSize.z, u_giSize.y);
		vec3 baked = texture3D(s_giVolume, clamp(uvw, vec3_splat(0.001), vec3_splat(0.999))).rgb * 2.0 * (0.8 + 0.2 * n.y);
		return mix(hemisphere, baked, u_giOrigin.w);
	}
	return hemisphere;
}

SAMPLER2D(s_lightmap, 6);

SAMPLER2D(s_texPaint, 7);
uniform vec4 u_paintWall;     // rgb: this building's walls (MeshComponent3D_Bgfx::set_paint), w: 1 painted, 0 as authored
uniform vec4 u_paintRoof;     // rgb: its roof

// A building in colors of its own: where its paint mask marks walls (red) or roof (green), the color shaded as the texture
// was (its light over a wall's or a roof's usual light: the siding's lines and the shingles stay)
vec4 paint_texel(vec4 texel, vec2 uv)
{
	if (u_paintWall.w < 0.5)
	{
		return texel;
	}
	vec3 mask = texture2D(s_texPaint, uv).rgb;
	float light = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
	// the brightness the paint is shaded against: a swatch's own (blue), else plaster's and a roof's
	float swatch = step(0.02, mask.b);
	float wallRef = mix(0.72, max(mask.b, 0.02), swatch);
	float roofRef = mix(0.45, max(mask.b, 0.02), swatch);
	texel.rgb = mix(texel.rgb, u_paintWall.rgb * min(light / wallRef, 1.2), mask.r);
	texel.rgb = mix(texel.rgb, u_paintRoof.rgb * min(light / roofRef, 1.2), mask.g);
	return texel;
}

uniform vec4 u_style;         // x: the plastic highlight's strength (0: none), y: its shininess (the toy look); z: plasticine
                              // (0: none, 1: all of it), w: its dents per unit of the model's own space

// The light baked per texel for the fixed base (lightmaps): the ambient light at this spot (sky and the sun's bounce, with
// the occlusion of walls and corners), stored as the square root of half of it; and how much of the sun reaches it.
// Replaces the ambient light, and darkens the sun where the baked shadow is darker than the shadow map (beyond its
// reach, or with shadows off).
void apply_lightmap(vec2 uv, inout vec3 ambient, inout float visibility)
{
	if (u_lightmap.w > 0.0)
	{
		vec4 baked = texture2D(s_lightmap, uv);
		ambient = baked.rgb * baked.rgb * 2.0;
		visibility = min(visibility, mix(1.0, baked.a, u_lightmap.w - 1.0));
	}
}

// 1 in the sun, down to (1 - strength) in shadow: the depth the sun saw, compared around the pixel. The sampler compares
// and blends 2x2 texels itself: four taps half a texel off the pixel cover its 3x3 texels with a tent's weights (softer
// than the nine plain taps it replaces, which cost an older phone's GPU half its frame)
float sun_visibility(vec3 worldPos, vec3 n)
{
	float visibility = 1.0;
	if (u_shadowParams.x > 0.0)
	{
		vec4 sp = mul(u_shadowMtx, vec4(worldPos + n * u_shadowParams.w, 1.0));
		vec3 proj = sp.xyz / sp.w;
		if (proj.x > 0.0 && proj.x < 1.0 && proj.y > 0.0 && proj.y < 1.0 && proj.z < 1.0)
		{
			float depth = proj.z - u_shadowParams.y;
			float half_ = 0.5 * u_shadowParams.z;
			float lit = shadow2D(s_shadowMap, vec3(proj.xy + vec2(-half_, -half_), depth))
				+ shadow2D(s_shadowMap, vec3(proj.xy + vec2(half_, -half_), depth))
				+ shadow2D(s_shadowMap, vec3(proj.xy + vec2(-half_, half_), depth))
				+ shadow2D(s_shadowMap, vec3(proj.xy + vec2(half_, half_), depth));
			visibility = mix(1.0, lit * 0.25, u_shadowParams.x);
		}
	}
	return visibility;
}

// Ground that does not look tiled (u_ground, materials.xml detile="a b"): smooth value noise over the world's x-z
float ground_hash(vec2 p)
{
	p = fract(p * vec2(0.3183099, 0.3678794) + vec2(0.71, 0.113));
	p *= 17.0;
	return fract(p.x * p.y * (p.x + p.y));
}

float ground_noise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(ground_hash(i), ground_hash(i + vec2(1.0, 0.0)), u.x),
		mix(ground_hash(i + vec2(0.0, 1.0)), ground_hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

// The second place to read the texture at: turned, scaled and moved, so its repeats do not line up with the first's
vec2 ground_second_uv(vec2 uv)
{
	return vec2(0.8 * uv.x - 0.6 * uv.y, 0.6 * uv.x + 0.8 * uv.y) * 0.73 + vec2(0.31, 0.57);
}

// How much of the second read shows here (patches several meters wide), and the color's slow rise and fall over the
// ground (lighter and darker regions): applied to the two reads mixed
vec4 ground_detile(vec4 first, vec4 second, vec2 worldXZ, vec4 ground)
{
	float n = ground_noise(worldXZ * 0.08) * 0.7 + ground_noise(worldXZ * 0.23) * 0.3;
	vec4 texel = mix(first, second, smoothstep(0.3, 0.7, n) * ground.x);
	float m = ground_noise(worldXZ * 0.021 + vec2(5.2, 1.3)) * 0.65 + ground_noise(worldXZ * 0.09 + vec2(2.7, 8.1)) * 0.35;
	texel.rgb *= 1.0 + ground.y * (m - 0.5) * 2.0;
	return texel;
}

#endif
