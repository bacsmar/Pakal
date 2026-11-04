// Shared by the lit mesh shaders: the sun's shadow
#ifndef PAKAL_LIGHTING_SH
#define PAKAL_LIGHTING_SH

#include "pakal_precision.sh"

PAKAL_SAMPLER2DSHADOW_HIGH(s_shadowMap, 4);   // compared by the sampler (BGFX_SAMPLER_COMPARE_LEQUAL), filtered

uniform mat4 u_shadowMtx;     // world to the sun's view, with the texture's offset and scale (and depth) already applied
uniform vec4 u_shadowParams;  // x: strength (0: no shadows), y: depth bias, z: 1 / map size, w: offset along the normal
                              // (meters)
uniform vec4 u_shadowFilter;  // x: the filter's taps on each side (2 to 4), y: their spacing (texels)

PAKAL_SAMPLER3D_HIGH(s_giVolume, 5);

uniform vec4 u_giOrigin;      // xyz: corner of the baked grid (world space), w: how much of the ambient comes from it (0: none)
uniform vec4 u_giSize;        // xyz: size of the grid in world space (the sign flips an axis)
uniform vec4 u_lightmap;      // xyz: this instance's place in the atlas (the vertex shader uses it), w: 0 none, 1 + baked shadow
                              // strength (the atlas is the ambient light), or minus that (the atlas is relative to the open floor)

// The ambient light at a point: the flat sky/ground blend, or the baked global illumination (light bounced around the scene,
// dark in alleys and under walls). The baked grid holds half of the light, so the sample is doubled.
vec3 ambient_light(vec3 worldPos, vec3 n, vec3 sky, vec3 ground)
{
	vec3 hemisphere = mix(ground, sky, n.y * 0.5 + 0.5);
	// (a surface whose lightmap is its ambient light does not read the grid; a relative one scales the grid's)
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
// Replaces the ambient light, or (a relative atlas: 1 on the open floor) scales the probes' with its occlusion and bounce,
// so each time of day keeps its own color; and darkens the sun where the baked shadow is darker than the shadow map
// (beyond its reach, or with shadows off).
void apply_lightmap(vec2 uv, inout vec3 ambient, inout float visibility)
{
	if (u_lightmap.w != 0.0)
	{
		vec4 baked = texture2D(s_lightmap, uv);
		vec3 light = baked.rgb * baked.rgb * 2.0;
		// (relative: its brightness only, the bake's light is one time of day's; the probes give the color)
		ambient = u_lightmap.w > 0.0 ? light : ambient * min(dot(light, vec3_splat(1.0 / 3.0)), 1.25);
		visibility = min(visibility, mix(1.0, baked.a, abs(u_lightmap.w) - 1.0));
	}
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

uniform vec4 u_clouds;        // x: how much of the sun a cloud takes (0: none), y: 1 / a cloud's size (meters), zw: the wind's
                              // carry (meters, x and z)

// The clouds' shadows drifting over the town: soft patches of shade, two scales of noise moved by the wind
float cloud_shadow(vec3 worldPos)
{
	if (u_clouds.x <= 0.0)
	{
		return 1.0;
	}
	vec2 p = (worldPos.xz + u_clouds.zw) * u_clouds.y;
	float n = ground_noise(p) * 0.65 + ground_noise(p * 2.03 + vec2(3.1, 7.7)) * 0.35;
	return 1.0 - u_clouds.x * smoothstep(0.42, 0.68, n);
}

// 1 in the sun, down to (1 - strength) in shadow: the depth the sun saw, compared around the pixel. The sampler compares
// and blends 2x2 texels itself. u_shadowFilter sets the taps: 2x2 half a texel off the pixel is a tent over 3x3 texels (four
// reads: an older phone's GPU spent half its frame on nine plain taps); 4x4 a texel apart a smooth edge over 5x5 (sixteen
// reads: a screen covered in leaves pays them on every layer); 3x3, or 2x2 farther apart, in between
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
			float spacing = u_shadowParams.z * u_shadowFilter.y;
			float taps = u_shadowFilter.x;
			float lit = 0.0;
			if (taps > 2.5)
			{
				float center = (taps - 1.0) * 0.5;
				for (int ix = 0; ix < 4; ++ix)
				{
					for (int iy = 0; iy < 4; ++iy)
					{
						if (float(ix) < taps && float(iy) < taps)
						{
							vec2 offset = (vec2(float(ix), float(iy)) - vec2_splat(center)) * spacing;
							lit += shadow2D(s_shadowMap, vec3(proj.xy + offset, depth));
						}
					}
				}
				lit *= 1.0 / (taps * taps);
			}
			else
			{
				float half_ = 0.5 * spacing;
				lit = (shadow2D(s_shadowMap, vec3(proj.xy + vec2(-half_, -half_), depth))
					+ shadow2D(s_shadowMap, vec3(proj.xy + vec2(half_, -half_), depth))
					+ shadow2D(s_shadowMap, vec3(proj.xy + vec2(-half_, half_), depth))
					+ shadow2D(s_shadowMap, vec3(proj.xy + vec2(half_, half_), depth))) * 0.25;
			}
			visibility = mix(1.0, lit, u_shadowParams.x);
		}
	}
	return visibility * cloud_shadow(worldPos);
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

// Other surfaces mixed into the ground in patches (materials.xml mix="..." and mix2="..."): grass with darker grass and
// bare earth, a plaza with worn stone. Each layer is read flat over the world; where it shows follows noise a patch wide,
// broken at its edge by finer noise.
SAMPLER2D(s_groundMix0, 8);
SAMPLER2D(s_groundMix1, 9);
uniform vec4 u_groundMix0;    // x: the layer's repeats a meter (0: no layer), y: the noise above which it shows (the
                              // engine makes it from the share it covers), z: 1 / its patches' size (meters), w: how soft
                              // their edge is
uniform vec4 u_groundMix1;    // the second layer, the same

float ground_patches(vec2 p, vec4 layer, vec2 seed)
{
	vec2 q = p * layer.z + seed;
	float n = ground_noise(q) * 0.55 + ground_noise(q * 2.3 + vec2(1.7, 9.2)) * 0.27 + ground_noise(p * 1.9 + seed * 3.1) * 0.18;
	return smoothstep(layer.y - layer.w, layer.y + layer.w, n);
}

vec4 ground_mix(vec4 texel, vec2 worldXZ)
{
	if (u_groundMix0.x > 0.0)
	{
		vec4 other = texture2D(s_groundMix0, worldXZ * u_groundMix0.x);
		texel = mix(texel, other, ground_patches(worldXZ, u_groundMix0, vec2(3.7, 1.9)));
	}
	if (u_groundMix1.x > 0.0)
	{
		vec4 other = texture2D(s_groundMix1, worldXZ * u_groundMix1.x + vec2(0.37, 0.61));
		texel = mix(texel, other, ground_patches(worldXZ, u_groundMix1, vec2(8.3, 4.1)));
	}
	return texel;
}

#endif
