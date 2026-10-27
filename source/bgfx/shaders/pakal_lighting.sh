// Shared by the lit mesh shaders: the sun's shadow
#ifndef PAKAL_LIGHTING_SH
#define PAKAL_LIGHTING_SH

#include "pakal_precision.sh"

PAKAL_SAMPLER2D_HIGH(s_shadowMap, 4);

uniform mat4 u_shadowMtx;     // world to the sun's view, with the texture's offset and scale (and depth) already applied
uniform vec4 u_shadowParams;  // x: strength (0: no shadows), y: depth bias, z: 1 / map size, w: offset along the normal (meters)

PAKAL_SAMPLER3D_HIGH(s_giVolume, 5);

uniform vec4 u_giOrigin;      // xyz: corner of the baked grid (world space), w: how much of the ambient comes from it (0: none)
uniform vec4 u_giSize;        // xyz: size of the grid in world space (the sign flips an axis)

// The ambient light at a point: the flat sky/ground blend, or the baked global illumination (light bounced around the scene,
// dark in alleys and under walls). The baked grid holds half of the light, so the sample is doubled.
vec3 ambient_light(vec3 worldPos, vec3 n, vec3 sky, vec3 ground)
{
	vec3 hemisphere = mix(ground, sky, n.y * 0.5 + 0.5);
	if (u_giOrigin.w > 0.0)
	{
		vec3 uvw = vec3(worldPos.x - u_giOrigin.x, worldPos.z - u_giOrigin.z, worldPos.y - u_giOrigin.y) / vec3(u_giSize.x, u_giSize.z, u_giSize.y);
		vec3 baked = texture3D(s_giVolume, clamp(uvw, vec3_splat(0.001), vec3_splat(0.999))).rgb * 2.0 * (0.8 + 0.2 * n.y);
		return mix(hemisphere, baked, u_giOrigin.w);
	}
	return hemisphere;
}

SAMPLER2D(s_lightmap, 6);

uniform vec4 u_style;         // x: the plastic highlight's strength (0: none), y: its shininess (the toy look); z: plasticine
                              // (0: none, 1: all of it), w: its dents per unit of the model's own space
uniform vec4 u_lightmap;      // xyz: this instance's place in the atlas (the vertex shader uses it), w: 0 none, 1 + baked shadow strength

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

// 1 in the sun, down to (1 - strength) in shadow: the depth the sun saw, compared around the pixel (a 3x3 filter softens the edge)
float sun_visibility(vec3 worldPos, vec3 n)
{
	float visibility = 1.0;
	if (u_shadowParams.x > 0.0)
	{
		vec4 sp = mul(u_shadowMtx, vec4(worldPos + n * u_shadowParams.w, 1.0));
		vec3 proj = sp.xyz / sp.w;
		if (proj.x > 0.0 && proj.x < 1.0 && proj.y > 0.0 && proj.y < 1.0 && proj.z < 1.0)
		{
			float lit = 0.0;
			for (int ix = -1; ix <= 1; ++ix)
			{
				for (int iy = -1; iy <= 1; ++iy)
				{
					float d = texture2D(s_shadowMap, proj.xy + vec2(float(ix), float(iy)) * u_shadowParams.z).x;
					lit += (proj.z - u_shadowParams.y <= d) ? 1.0 : 0.0;
				}
			}
			visibility = mix(1.0, lit / 9.0, u_shadowParams.x);
		}
	}
	return visibility;
}

#endif
