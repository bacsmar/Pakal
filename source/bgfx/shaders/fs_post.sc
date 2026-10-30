$input v_texcoord0

#include <bgfx_shader.sh>
#include "pakal_precision.sh"

SAMPLER2D(s_postColor, 0);
PAKAL_SAMPLER2D_HIGH(s_postDepth, 1);

uniform vec4 u_post0; // x: exposure, y: contrast, z: saturation, w: vignette
uniform vec4 u_post1; // rgb: color gain, w: tilt-shift blur (pixels at the far edge)
uniform vec4 u_post2; // x: tilt-shift focus line (0..1 from the top), y: sharp half band, z: ambient occlusion strength, w: radius (meters)
uniform vec4 u_post3; // x: 1/width, y: 1/height, z: near plane, w: far plane
uniform vec4 u_post6; // x: quality (the share of the samples taken), y: the sketch look's strength (0: off)
uniform vec4 u_post7; // xyz: the view's translation (the world's origin in the view: the sketch's strokes stay on the scene)
uniform vec4 u_post5; // rgb: fade color, w: how much of the picture the fade covers
uniform vec4 u_post4; // x: projection scale x, y: projection scale y, z: occlusion bias (meters), w: occlusion range (meters)
uniform vec4 u_post8; // rgb: fog color, w: fog density per meter (0: off)
uniform vec4 u_post9; // x: fog start (meters), y: the most fog, z: the most fog on the sky, w: the horizon (meters, 0: none)
uniform vec4 u_post10; // rgb: the sky's color at the horizon

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
	float d = texture2D(s_postDepth, target_uv(uv)).x;
	return u_post3.z * u_post3.w / (u_post3.w - d * (u_post3.w - u_post3.z));
}

// A point of the view (camera looks down -z; z here is the distance in front of it)
vec3 view_position(vec2 uv, float z)
{
	vec2 ndc = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
	return vec3(ndc.x * z / u_post4.x, ndc.y * z / u_post4.y, z);
}

float noise(vec2 pixel)
{
	return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
}

// The surface's orientation from the depth of the neighbors (view space, facing the camera)
vec3 view_normal(vec2 uv, vec3 p)
{
	vec2 texel = u_post3.xy;
	vec3 px = view_position(uv + vec2(texel.x, 0.0), linear_depth(uv + vec2(texel.x, 0.0)));
	vec3 nx = view_position(uv - vec2(texel.x, 0.0), linear_depth(uv - vec2(texel.x, 0.0)));
	vec3 py = view_position(uv + vec2(0.0, texel.y), linear_depth(uv + vec2(0.0, texel.y)));
	vec3 ny = view_position(uv - vec2(0.0, texel.y), linear_depth(uv - vec2(0.0, texel.y)));
	vec3 dx = abs(px.z - p.z) < abs(p.z - nx.z) ? px - p : p - nx;
	vec3 dy = abs(py.z - p.z) < abs(p.z - ny.z) ? py - p : p - ny;
	vec3 n = normalize(cross(dy, dx));
	if (n.z > 0.0) n = -n;
	return n;
}

float hash12(vec2 p)
{
	vec3 q = fract(vec3(p.xyx) * 0.1031);
	q += dot(q, q.yzx + 33.33);
	return fract((q.x + q.y) * q.z);
}

// One family of pen strokes on the plane coordinates 'q' (meters): parallel lines 'spacing' apart along 'dir', each one
// broken into dashes and wobbling a little, thicker where the picture is darker
float strokes(vec2 q, vec2 dir, float spacing, float darkness)
{
	vec2 across = vec2(-dir.y, dir.x);
	float s = dot(q, across) / spacing;
	float row = floor(s);
	float along = dot(q, dir) / (spacing * 6.0);
	float wobble = 0.25 * sin(along * 2.1 + row * 1.7);
	float gap = abs(fract(s + wobble) - 0.5);
	float width = 0.06 + 0.3 * darkness;
	float ink = 1.0 - smoothstep(width, width + 0.08, gap);
	float dash = step(0.3, hash12(vec2(row, floor(along + hash12(vec2(row, 7.0))))));
	return ink * dash;
}

// The sketch look (an optional style): the picture as an etching on warm paper. Pen strokes cover each surface along its
// own plane (so they stay on the scene as the camera moves), more and crossed where it is darker, with a speckled grain.
vec3 sketch(vec2 uv, vec3 color, vec2 pixel)
{
	float z = linear_depth(uv);
	if (z > u_post3.w * 0.98)
	{
		return color;
	}
	vec3 p = view_position(uv, z);
	vec3 n = view_normal(uv, p);
	vec3 a = p - u_post7.xyz;
	vec3 w = abs(n);
	vec2 q = (w.x > w.y && w.x > w.z) ? a.yz : ((w.y > w.z) ? a.xz : a.xy);
	float luma = dot(color, vec3(0.299, 0.587, 0.114));
	float darkness = clamp(1.0 - luma * 1.25, 0.0, 1.0);
	float ink = strokes(q, normalize(vec2(1.0, 0.35)), 0.06, darkness) * smoothstep(0.15, 0.35, darkness);
	ink = max(ink, strokes(q, normalize(vec2(-0.4, 1.0)), 0.05, darkness) * smoothstep(0.45, 0.65, darkness));
	ink = max(ink, step(1.0 - 0.45 * darkness, hash12(pixel)) * 0.8);
	vec3 paper = color * vec3(1.06, 0.98, 0.86) + vec3(0.04, 0.03, 0.0);
	return mix(color, paper * (1.0 - 0.7 * ink), u_post6.y);
}

// Ambient occlusion from the depth: samples around the pixel, each one that sits in front of the surface (above it,
// within range) darkens it a little. The surface orientation comes from the depth of the neighbors.
float ambient_occlusion(vec2 uv, vec2 pixel)
{
	float z = linear_depth(uv);
	if (z > u_post3.w * 0.98)
	{
		return 1.0; // sky
	}
	vec3 p = view_position(uv, z);
	vec2 texel = u_post3.xy;
	vec3 px = view_position(uv + vec2(texel.x, 0.0), linear_depth(uv + vec2(texel.x, 0.0)));
	vec3 nx = view_position(uv - vec2(texel.x, 0.0), linear_depth(uv - vec2(texel.x, 0.0)));
	vec3 py = view_position(uv + vec2(0.0, texel.y), linear_depth(uv + vec2(0.0, texel.y)));
	vec3 ny = view_position(uv - vec2(0.0, texel.y), linear_depth(uv - vec2(0.0, texel.y)));
	vec3 dx = abs(px.z - p.z) < abs(p.z - nx.z) ? px - p : p - nx;
	vec3 dy = abs(py.z - p.z) < abs(p.z - ny.z) ? py - p : p - ny;
	vec3 n = normalize(cross(dy, dx));
	if (n.z > 0.0) n = -n; // facing the camera

	// radius in pixels for a radius in meters at this depth
	float radiusPx = u_post2.w * u_post4.y * 0.5 / (u_post3.y * z);
	radiusPx = min(radiusPx, 64.0);
	float angle = noise(pixel) * 6.2831853;
	float occlusion = 0.0;
	const int maxTaps = 14;
	int taps = int(max(4.0, float(maxTaps) * u_post6.x));
	for (int i = 0; i < maxTaps; ++i)
	{
		if (i >= taps) break;
		float t = (float(i) + 0.5) / float(taps);
		float a = angle + float(i) * 2.399963;
		vec2 offset = vec2(cos(a), sin(a)) * (radiusPx * sqrt(t)) * texel;
		vec2 suv = uv + offset;
		float sz = linear_depth(suv);
		vec3 s = view_position(suv, sz);
		vec3 v = s - p;
		float dist = length(v);
		float facing = max(dot(n, v / max(dist, 0.0001)) - u_post4.z, 0.0);
		float range = 1.0 - smoothstep(u_post4.w * 0.5, u_post4.w, dist);
		occlusion += facing * range;
	}
	occlusion /= float(taps);
	return 1.0 - clamp(occlusion * 2.2 * u_post2.z, 0.0, 0.85);
}

void main()
{
	vec2 uv = v_texcoord0;
	vec2 texel = u_post3.xy;

	// Tilt-shift: blur grows with the distance from the focus line (a ring of taps, rotating each pixel)
	float away = max(abs(uv.y - u_post2.x) - u_post2.y, 0.0) / max(0.5 - u_post2.y, 0.01);
	float blur = u_post1.w * clamp(away, 0.0, 1.0);
	vec3 color = texture2D(s_postColor, target_uv(uv)).rgb;
	if (blur > 0.5)
	{
		float angle = noise(gl_FragCoord.xy) * 6.2831853;
		vec3 sum = color;
		float weight = 1.0;
		int blurTaps = int(max(4.0, 12.0 * u_post6.x));
		for (int i = 0; i < 12; ++i)
		{
			if (i >= blurTaps) break;
			float t = (float(i) + 0.5) / float(blurTaps);
			float a = angle + float(i) * 2.399963;
			vec2 offset = vec2(cos(a), sin(a)) * (blur * sqrt(t)) * texel;
			sum += texture2D(s_postColor, target_uv(uv + offset)).rgb;
			weight += 1.0;
		}
		color = sum / weight;
	}

	if (u_post2.z > 0.0)
	{
		color *= ambient_occlusion(uv, gl_FragCoord.xy);
	}
	if (u_post6.y > 0.0)
	{
		color = sketch(uv, color, gl_FragCoord.xy);
	}
	// Fog by the distance from the camera: depth to the scene, and the horizon's haze over the sky
	if (u_post8.w > 0.0)
	{
		float z = linear_depth(uv);
		float fog = 1.0 - exp(-u_post8.w * max(z - u_post9.x, 0.0));
		fog = min(fog, z >= u_post3.w * 0.98 ? u_post9.z : u_post9.y);
		color = mix(color, u_post8.rgb, fog);
		// what stands toward the horizon fades wholly into what the sky shows there (its horizon color with the sky's share
		// of fog), so the far end of the world leaves no edge against it
		if (u_post9.w > 0.0 && z < u_post3.w * 0.98)
		{
			float horizon = min(u_post9.w, u_post3.w * 0.97);
			vec3 sky = mix(u_post10.rgb, u_post8.rgb, u_post9.z);
			color = mix(color, sky, smoothstep(horizon * 0.55, horizon, z));
		}
	}

	// Color: exposure, contrast around mid grey, saturation, gain, vignette
	color *= u_post0.x;
	color = (color - 0.5) * u_post0.y + 0.5;
	float luma = dot(color, vec3(0.299, 0.587, 0.114));
	color = mix(vec3_splat(luma), color, u_post0.z);
	color *= u_post1.rgb;
	vec2 offset = (uv - vec2_splat(0.5)) * vec2(1.0, 0.9);
	float edge = smoothstep(0.25, 0.85, length(offset));
	color = color * (1.0 - u_post0.w * edge);
	color = mix(color, u_post5.rgb, u_post5.a);
	gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
