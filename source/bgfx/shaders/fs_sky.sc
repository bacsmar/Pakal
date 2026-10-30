$input v_dir

#include <bgfx_shader.sh>

uniform vec4 u_skyTop;
uniform vec4 u_skyHorizon;
uniform vec4 u_skySun;      // xyz: toward the sun (world, normalized); w: seconds (the clouds drift)
uniform vec4 u_skySunColor; // rgb: the sun's light; w: how much cloud (0: a clear sky)

// Value noise from a hash: cheap, no texture
float hash2(vec2 p)
{
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise2(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash2(i), hash2(i + vec2(1.0, 0.0)), u.x), mix(hash2(i + vec2(0.0, 1.0)), hash2(i + vec2(1.0, 1.0)), u.x), u.y);
}

float fbm(vec2 p)
{
	float sum = 0.0;
	float amp = 0.5;
	for (int i = 0; i < 4; ++i)
	{
		sum += amp * noise2(p);
		p = p * 2.03 + vec2(1.7, 9.2);
		amp *= 0.5;
	}
	return sum;
}

void main()
{
	vec3 dir = normalize(v_dir);
	float t = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
	vec3 color = mix(u_skyHorizon.rgb, u_skyTop.rgb, t);

	// the sun: a disk and its glow round it (in the light's color), only above the horizon's line
	vec3 sun = u_skySun.xyz;
	float facing = max(dot(dir, sun), 0.0);
	float above = smoothstep(-0.05, 0.02, dir.y);
	vec3 sunLight = u_skySunColor.rgb;
	color += sunLight * (pow(facing, 12.0) * 0.18 + pow(facing, 120.0) * 0.35) * above;
	float disk = smoothstep(0.9990, 0.9996, facing) * above;

	// the clouds: noise on a plane high above, seen through the dome; they drift, thin out toward the horizon, and the side
	// toward the sun is lit with its color
	float cover = u_skySunColor.w;
	if (dir.y > 0.0 && cover > 0.0)
	{
		vec2 plane = dir.xz / (dir.y + 0.12) * 1.6 + vec2(u_skySun.w * 0.012, u_skySun.w * 0.005);
		float n = fbm(plane);
		float cloud = smoothstep(1.0 - cover, 1.0 - cover + 0.28, n) * smoothstep(0.0, 0.25, dir.y);
		vec3 lit = mix(vec3(1.05, 1.05, 1.08), sunLight * 1.05 + vec3(0.3, 0.3, 0.32), 0.5 + 0.5 * pow(facing, 3.0));
		vec3 shade = mix(u_skyHorizon.rgb, vec3(0.7, 0.72, 0.78), 0.5);
		vec3 cloudColor = mix(shade, lit, smoothstep(0.35, 0.9, n));
		color = mix(color, cloudColor * 1.12, cloud * 0.95);
		disk *= 1.0 - cloud;
	}
	color = mix(color, sunLight * 1.4 + vec3(0.3, 0.28, 0.2), disk);
	gl_FragColor = vec4(color, 1.0);
}
