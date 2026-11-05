$input v_normal, v_texcoord0, v_worldpos, v_tangent, v_texcoord1

#include <bgfx_shader.sh>
#include "pakal_precision.sh"
#include "pakal_lighting.sh"

SAMPLER2D(s_texColor, 0);
PAKAL_SAMPLER2D_HIGH(s_texNormal, 1);
PAKAL_SAMPLER2D_HIGH(s_texOrm, 2);

uniform vec4 u_lightDir;      // xyz: direction the light travels (from the light into the scene)
uniform vec4 u_lightColor;    // rgb: light color
uniform vec4 u_ambient;       // rgb: ambient color from above (the sky)
uniform vec4 u_ambientGround; // rgb: ambient color from below (the light bounced by the ground)
uniform vec4 u_tint;          // material color multiplier
uniform vec4 u_material;      // x: alpha test reference (0 = off), y: keep the texture alpha, z: unlit, w: dithered opacity
uniform vec4 u_cut;       // a model cut away (MeshComponent3D_Bgfx::set_fade_cut): x: the height (world y) above which
                          // nothing is drawn, y: the rim under it (m), drawn flat in u_cutColor; w: 1 = on
uniform vec4 u_cutColor;
uniform vec4 u_ground;        // x: how much a second, turned read of the texture breaks its tiling, y: the macro variation (0: off)
uniform vec4 u_uvScroll;      // z: water (how much it reflects the sky; 0 for the rest), w: its clock in seconds

// Metal-rough shading for the models that come with a normal map and an ORM texture (occlusion, roughness, metalness in
// R, G, B): GGX for the sun's highlight, the baked occlusion on the ambient light, a little sky reflection on shiny surfaces
void main()
{
	vec4 texel = texture2D(s_texColor, v_texcoord0);
	if (u_ground.x + u_ground.y > 0.0)
	{
		texel = ground_detile(texel, texture2D(s_texColor, ground_second_uv(v_texcoord0)), v_worldpos.xz, u_ground);
	}
	texel = ground_mix(texel, v_worldpos.xz);
	texel = paint_texel(texel, v_texcoord0);
	vec4 albedo = texel * u_tint;
	// the cutout follows the texture alone: a faded model (u_tint.a) keeps its leaves
	if (u_material.x > 0.0 && texel.a < u_material.x)
	{
		discard;
	}
	// cut away: nothing above the cut; the band under it flat in the rim's color, so the edge reads from above
	if (u_cut.w > 0.0)
	{
		if (v_worldpos.y > u_cut.x)
		{
			discard;
		}
		if (v_worldpos.y > u_cut.x - u_cut.y)
		{
			gl_FragColor = vec4(u_cutColor.rgb, 1.0);
			return;
		}
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

	// a faded (blended) model, mostly see-through: a flat color from the ambient and a share of the sun, without the shadows,
	// the lightmap, the probes or the reflections. A building that fills the screen blends layers of these, and each costs
	// the whole lighting otherwise.
	if (u_material.z > 1.5)
	{
		gl_FragColor = vec4(albedo.rgb * (u_ambient.rgb * 0.75 + u_lightColor.rgb * 0.12), mix(1.0, albedo.a, u_material.y));
		return;
	}

	if (u_uvScroll.z > 0.0)
	{
		// water: turquoise where it is shallow (v_texcoord1.x: 1 by the shore, 0 out at sea; a sea without it is deep),
		// its own color farther out
		// (three stops, as a stylized water graph blends its depth colors: deep, mid, shallow)
		float shore = clamp(v_texcoord1.x, 0.0, 1.0);
		vec3 water = mix(vec3(0.03, 0.20, 0.36), vec3(0.07, 0.46, 0.58), smoothstep(0.0, 0.4, shore));
		water = mix(water, vec3(0.24, 0.74, 0.68), smoothstep(0.4, 0.9, shore));
		albedo.rgb = mix(albedo.rgb, water, 0.9);
	}
	vec3 orm = texture2D(s_texOrm, v_texcoord0).rgb;
	float occlusion = orm.r;
	float roughness = clamp(orm.g, 0.06, 1.0);
	float metal = orm.b;

	// Normal map: tangent space, green up (the glTF convention)
	vec3 n = normalize(v_normal);
	vec3 t = normalize(v_tangent - n * dot(n, v_tangent));
	vec3 b = cross(n, t);
	vec3 tn = texture2D(s_texNormal, v_texcoord0).xyz * 2.0 - 1.0;
	if (u_uvScroll.z > 0.0)
	{
		// water: the ripples' map read twice, its places wobbled by sines of the world position at two wavelengths and
		// speeds (no sheet sliding one way: they interfere into ripples that come and go)
		float time = u_uvScroll.w;
		vec2 wobble0 = sin(v_worldpos.zx * 0.45 + time * vec2(0.9, 0.7)) * 0.035;
		vec2 wobble1 = sin(v_worldpos.zx * 0.27 - time * vec2(0.6, 0.8)) * 0.05;
		vec3 n0 = texture2D(s_texNormal, v_texcoord0 + wobble0).xyz;
		vec3 n1 = texture2D(s_texNormal, v_texcoord0 * 1.43 + vec2(0.37, 0.61) + wobble1).xyz;
		tn = n0 + n1 - 1.0;
		tn = normalize(vec3(tn.xy * 1.3, tn.z));
	}
	vec3 normal = normalize(t * tn.x + b * tn.y + n * tn.z);
	float waterNoise = clamp(tn.x * 1.6 + 0.5, 0.0, 1.0);

	vec3 l = -normalize(u_lightDir.xyz);
	vec3 eye = mul(u_invView, vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	vec3 v = normalize(eye - v_worldpos);
	vec3 h = normalize(l + v);
	float visibility = sun_visibility(v_worldpos, n);

	float ndl = clamp((dot(normal, l) + 0.25) / 1.25, 0.0, 1.0);
	float ndh = max(dot(normal, h), 0.0);
	float ndv = max(dot(normal, v), 0.001);
	float vdh = max(dot(v, h), 0.0);

	// GGX distribution, Schlick fresnel, a cheap visibility term
	float a = roughness * roughness;
	float a2 = a * a;
	float denom = ndh * ndh * (a2 - 1.0) + 1.0;
	float distribution = a2 / (3.14159265 * denom * denom);
	vec3 f0 = mix(vec3_splat(0.04), albedo.rgb, metal);
	vec3 fresnel = f0 + (vec3_splat(1.0) - f0) * pow(1.0 - vdh, 5.0);
	float vis = 0.5 / (ndl * (ndv * (1.0 - a) + a) + ndv * (ndl * (1.0 - a) + a) + 0.0001);
	vec3 specular = distribution * vis * fresnel * max(dot(normal, l), 0.0);

	vec3 diffuseColor = albedo.rgb * (1.0 - metal);
	vec3 ambient = ambient_light(v_worldpos, normal, u_ambient.rgb, u_ambientGround.rgb);
	apply_lightmap(v_texcoord1, ambient, visibility);
	vec3 direct = u_lightColor.rgb * visibility;
	vec3 color = diffuseColor * (ambient * occlusion + direct * ndl) + direct * specular;
	// sky reflection: strongest at grazing angles and on smooth surfaces
	float reflectance = pow(1.0 - ndv, 4.0) * (1.0 - roughness) + metal * (1.0 - roughness) * 0.5;
	color += u_ambient.rgb * f0 * reflectance * occlusion * 0.45;
	if (u_uvScroll.z > 0.0)
	{
		// water: the sky reflected about the rippled normal (its color from the ambient's: above bright, near the horizon
		// paler), more at grazing angles (fresnel), over the water's own color
		vec3 r = reflect(-v, normal);
		vec3 sky = mix(u_ambientGround.rgb * 1.2 + u_lightColor.rgb * 0.25, u_ambient.rgb * 1.6, clamp(r.y * 1.5, 0.0, 1.0));
		float facing = pow(1.0 - ndv, 3.0);
		color = mix(color, sky, clamp(u_uvScroll.z * (0.25 + 0.75 * facing), 0.0, 0.9));
		// foam: a band along the shore that breathes in and out (its edge slides with the clock and the wave's phase along
		// the coast), broken up by the ripples' noise, and a few flecks on the crests farther out
		float shoreNow = clamp(v_texcoord1.x, 0.0, 1.0);
		float edge = 0.88 + 0.05 * sin(u_uvScroll.w * 0.8 + v_worldpos.x * 0.21 + v_worldpos.z * 0.17);
		float band = smoothstep(edge - 0.04, edge + 0.03, shoreNow);
		float lace = smoothstep(0.42, 0.62, waterNoise + (shoreNow - edge) * 1.4);
		float crest = smoothstep(0.80, 0.95, waterNoise) * smoothstep(0.1, 0.5, shoreNow) * 0.5;
		float foam = clamp(band * lace + crest, 0.0, 1.0);
		// caustics: a bright web on the shallows, from the same ripples (it drifts as they do)
		float web = smoothstep(0.60, 0.74, waterNoise) * smoothstep(0.5, 0.9, shoreNow);
		color += vec3(0.10, 0.16, 0.12) * web * (0.4 + 0.6 * visibility);
		color = mix(color, vec3(0.92, 0.97, 0.97) * (u_ambient.rgb * 0.9 + u_lightColor.rgb * 0.6 * visibility), foam * 0.85);
	}
	color = mix(color, albedo.rgb, u_material.z);

	gl_FragColor = vec4(color, mix(1.0, albedo.a, u_material.y));
}
