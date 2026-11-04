$input v_texcoord0

// Ambient occlusion from the depth, at half the picture's size (BgfxGraphicsSystem::render_ao): samples on a spiral
// around the pixel; each one in front of the surface darkens it by how much it faces it, less the farther it is, nothing
// past the radius (a cable meters above the floor does not darken it). The spiral turns by interleaved gradient noise, which
// the depth-aware blur after it (fs_ao_blur) evens out. Writes the light left (1: open) in r.

#include <bgfx_shader.sh>
#include "pakal_precision.sh"

PAKAL_SAMPLER2D_HIGH(s_aoDepth, 0);   // the frame's depth, full size (point sampled)

uniform vec4 u_ao0;   // x: 1 / the depth's width, y: 1 / its height, z: near plane, w: far plane
uniform vec4 u_ao1;   // x: projection scale x, y: projection scale y, z: radius (meters), w: bias (how much it must face)
uniform vec4 u_ao2;   // x: strength, y: taps (4 to 8)

// The pass thinks of uv as running downward from the top of the picture. Render targets of OpenGL start at the bottom.
vec2 target_uv(vec2 uv)
{
#if BGFX_SHADER_LANGUAGE_GLSL
	return vec2(uv.x, 1.0 - uv.y);
#else
	return uv;
#endif
}

float linear_depth(vec2 uv)
{
	float d = texture2D(s_aoDepth, target_uv(uv)).x;
	return u_ao0.z * u_ao0.w / (u_ao0.w - d * (u_ao0.w - u_ao0.z));
}

// A point of the view (camera looks down -z; z here is the distance in front of it)
vec3 view_position(vec2 uv, float z)
{
	vec2 ndc = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
	return vec3(ndc.x * z / u_ao1.x, ndc.y * z / u_ao1.y, z);
}

void main()
{
	vec2 uv = v_texcoord0;
	float z = linear_depth(uv);
	if (z > u_ao0.w * 0.98)
	{
		gl_FragColor = vec4_splat(1.0); // sky
		return;
	}
	vec3 p = view_position(uv, z);
	// the surface's orientation from the nearer neighbor on each axis (not across an edge)
	vec2 texel = u_ao0.xy;
	vec3 px = view_position(uv + vec2(texel.x, 0.0), linear_depth(uv + vec2(texel.x, 0.0)));
	vec3 nx = view_position(uv - vec2(texel.x, 0.0), linear_depth(uv - vec2(texel.x, 0.0)));
	vec3 py = view_position(uv + vec2(0.0, texel.y), linear_depth(uv + vec2(0.0, texel.y)));
	vec3 ny = view_position(uv - vec2(0.0, texel.y), linear_depth(uv - vec2(0.0, texel.y)));
	vec3 dx = abs(px.z - p.z) < abs(p.z - nx.z) ? px - p : p - nx;
	vec3 dy = abs(py.z - p.z) < abs(p.z - ny.z) ? py - p : p - ny;
	vec3 n = normalize(cross(dy, dx));
	if (n.z > 0.0) n = -n; // facing the camera

	float radius = u_ao1.z;
	// the radius on screen (uv) at this depth, kept to a part of the picture up close
	vec2 reach = vec2(radius * u_ao1.x * 0.5 / z, radius * u_ao1.y * 0.5 / z);
	reach = min(reach, vec2_splat(0.12));
	vec2 pixel = gl_FragCoord.xy;
	float angle = fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715)))) * 6.2831853;
	float taps = u_ao2.y;
	float occlusion = 0.0;
	for (int i = 0; i < 8; ++i)
	{
		if (float(i) >= taps) break;
		float t = (float(i) + 0.5) / taps;
		float a = angle + float(i) * 2.399963;
		vec2 suv = uv + vec2(cos(a), sin(a)) * reach * sqrt(t);
		vec3 v = view_position(suv, linear_depth(suv)) - p;
		float vv = dot(v, v);
		float facing = max(dot(n, v) * inversesqrt(vv + 0.0001) - u_ao1.w, 0.0);
		float falloff = max(1.0 - vv / (radius * radius), 0.0);
		occlusion += facing * falloff;
	}
	occlusion /= taps;
	float light = 1.0 - clamp(occlusion * 2.2 * u_ao2.x, 0.0, 0.85);
	gl_FragColor = vec4(light, light, light, 1.0);
}
