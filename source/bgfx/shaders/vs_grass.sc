$input a_position, a_texcoord0, a_texcoord1, i_data0
$output v_normal, v_color0, v_worldpos, v_texcoord0

#include <bgfx_shader.sh>
#include "pakal_precision.sh"

// Grass made on the GPU (BgfxGrass, docs/GRASS.md): one blade's corner. The tile mesh gives the root's place in its tile,
// the height along the blade (a_position.y: 0 root, 1 tip), the edge (a_texcoord0.x: -1, +1, 0 the tip), the blade's
// rank (a_texcoord0.y) and two random numbers; the instance gives the tile's corner. Everything else comes from here:
// whether the density keeps it, its height and color (its clump's), its facing (turned toward the camera when seen
// edge-on), its lean, the wind and whether the sun reaches its root.
uniform vec4 u_grassField;   // xy: the field's corner (x, z), zw: 1 / its size
uniform vec4 u_grassShape;   // x, y: the lowest and highest blade (m), z: a blade's width, w: the ground's height (no map)
uniform vec4 u_grassReach;   // x: the reach (m), y: the density's scale (quality), z: a tile's side, w: the fade at the reach
uniform vec4 u_grassGround;  // xy: the height map's size (texels), z: its lowest point, w: its range (0: flat at u_grassShape.w)
uniform vec4 u_grassFocus;   // xyz: where the reach is measured from, w: the clock (seconds)
uniform vec4 u_grassEye;     // xyz: the camera
uniform vec4 u_grassWind;    // x: bend, y: speed, w: 1 on
uniform vec4 u_grassRoot;    // rgb: the color at the ground
uniform vec4 u_grassTip;     // rgb: at the tip, in most clumps
uniform vec4 u_grassClumpColors[3];   // rgb: the tips of clumps of another color, w: the share of the clumps that take it
uniform vec4 u_grassDead;    // rgb: dead blades, w: their share
uniform vec4 u_grassClump;   // x: 1 / a clump's size (m), y: how far a clump departs from the field, z: blades a tuft (0: none),
                             // w: how far a tuft's blades lean out
uniform vec4 u_grassThin;    // x: where the thinning starts (share of the reach), y: the density left at the reach
uniform mat4 u_grassShadowMtx;   // world to the sun's shadow map (as u_shadowMtx)
uniform vec4 u_grassShadow;  // x: the shadow's strength (0: none), y: depth bias

SAMPLER2D(s_grassCover, 0);  // r: the share of the blades kept
SAMPLER2D(s_grassGround, 1); // rg: the ground's height, 16 bits over its range (point-sampled)
PAKAL_SAMPLER2DSHADOW_HIGH(s_grassShadow, 2);

float grass_hash(vec2 p)
{
	vec3 q = fract(vec3(p.x, p.y, p.x) * 0.1031);
	q += dot(q, q.yzx + 33.33);
	return fract((q.x + q.y) * q.z);
}

// Value noise (0..1): smooth bumps one unit apart
float grass_noise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	float a = grass_hash(i);
	float b = grass_hash(i + vec2(1.0, 0.0));
	float c = grass_hash(i + vec2(0.0, 1.0));
	float d = grass_hash(i + vec2(1.0, 1.0));
	return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float ground_texel(vec2 texel)
{
	vec4 s = texture2DLod(s_grassGround, (texel + 0.5) / u_grassGround.xy, 0.0);
	return (floor(s.r * 255.0 + 0.5) * 256.0 + floor(s.g * 255.0 + 0.5)) / 65535.0;
}

// The ground's height under a point of the field (uv 0..1): the four texels around it, blended
float ground_height(vec2 uv)
{
	if (u_grassGround.w <= 0.0)
	{
		return u_grassShape.w;
	}
	vec2 f = clamp(uv, 0.0, 1.0) * u_grassGround.xy - 0.5;
	vec2 i = floor(f);
	vec2 w = f - i;
	vec2 last = u_grassGround.xy - 1.0;
	vec2 i0 = clamp(i, vec2_splat(0.0), last);
	vec2 i1 = clamp(i + 1.0, vec2_splat(0.0), last);
	float h = mix(mix(ground_texel(i0), ground_texel(vec2(i1.x, i0.y)), w.x),
		mix(ground_texel(vec2(i0.x, i1.y)), ground_texel(i1), w.x), w.y);
	return u_grassGround.z + u_grassGround.w * h;
}

void main()
{
	vec2 root = i_data0.xy + a_position.xz;
	float t = a_position.y;
	float side = a_texcoord0.x;
	float rank = a_texcoord0.y;
	float r1 = a_texcoord1.x;
	float r2 = a_texcoord1.y;

	// Kept or not: the field's density map, thinned with distance from the focus (as BgfxGrass's density_at), and faded
	// out over the last meters of the reach. A blade left out gets no height: its triangles fold into a line.
	vec2 uv = (root - u_grassField.xy) * u_grassField.zw;
	float inside = step(0.0, uv.x) * step(uv.x, 1.0) * step(0.0, uv.y) * step(uv.y, 1.0);
	float cover = texture2DLod(s_grassCover, clamp(uv, 0.0, 1.0), 0.0).r * inside;
	float reach = u_grassReach.x;
	float dist = length(root - u_grassFocus.xz);
	float x = clamp((dist - u_grassThin.x * reach) / max(0.001, (1.0 - u_grassThin.x) * reach), 0.0, 1.0);
	float thinned = clamp(u_grassReach.y * (1.0 - (1.0 - u_grassThin.y) * x * x * (3.0 - 2.0 * x)), 0.0, 1.0);
	float keep = clamp((thinned * cover - rank) * 24.0, 0.0, 1.0);
	float fade = 1.0 - smoothstep(reach - u_grassReach.w, reach, dist);

	// Clumps: one noise sets their height and lean together, another their facing, a third their color. 'strength' 0 is
	// an even lawn; 1, clumps of very tall next to very short blades.
	vec2 cp = root * u_grassClump.x;
	float clump = grass_noise(cp);
	float clumpTurn = grass_noise(cp * 2.0 + vec2(17.0, 5.0));
	float strength = u_grassClump.y;
	float tall = mix(0.5, clump, strength) + (r2 - 0.5) * mix(0.5, 0.3, strength);
	// thinner grass is shorter too: the edge of a patch tapers into the bare ground instead of ending in a wall
	// (only the thin edge: a field painted at mid density keeps its full height)
	float edge = mix(0.55, 1.0, smoothstep(0.0, 0.3, cover));
	float h = mix(u_grassShape.x, u_grassShape.y, clamp(tall, 0.0, 1.0)) * keep * fade * edge;

	// Facing: within its clump's own direction (a tuft's blades every way); seen edge-on, turned toward the camera so it
	// never thins to a line, and its face toward the camera (both sides are drawn: the normal is the side seen)
	float tuft = step(1.5, u_grassClump.z);
	float angle = mix((clumpTurn + (r1 - 0.5) * 0.6), r1, tuft) * 6.2831853;
	vec2 outward = vec2(sin(angle), cos(angle));   // where it leans (a tuft's blades fan out)
	vec2 facing = outward;
	vec2 toEye = u_grassEye.xz - root;
	float eyeDist = length(toEye);
	toEye = eyeDist > 0.001 ? toEye / eyeDist : vec2(0.0, 1.0);
	if (dot(facing, toEye) < 0.0)
	{
		facing = -facing;
	}
	float edgeOn = 1.0 - dot(facing, toEye);
	facing = normalize(mix(facing, toEye, edgeOn * 0.6));
	vec2 across = vec2(facing.y, -facing.x);

	// Lean (a tuft's blades out from their root, a lone blade a little) and the wind, both growing toward the tip
	float lean = mix(0.25 + 0.35 * r2, 0.2 + 0.6 * u_grassClump.w * (0.5 + 0.5 * r2), tuft) * h;
	float phase = dot(root, vec2(0.37, 0.53)) * 0.7 + u_grassFocus.w * max(u_grassWind.y, 0.0) + r1 * 0.8;
	float gust = (sin(phase) * 0.7 + sin(phase * 2.3 + root.x * 0.5) * 0.3) * u_grassWind.w;
	vec2 sway = vec2(gust, gust * 0.6 + cos(phase * 1.7) * 0.3 * u_grassWind.w) * u_grassWind.x * h * 1.5;
	vec2 offset = (outward * lean + sway) * t * t;
	// the blade keeps its length as it bends: the tip comes down as it goes out
	float up = sqrt(max(h * h * t * t - dot(offset, offset), 0.0));

	// where the blades are thinned (far away, a low quality) the ones left are wider, so the grass covers as much: a far
	// blade a pixel wide would shimmer
	float width = u_grassShape.z * inversesqrt(max(thinned, 0.15)) * (1.0 - 0.85 * t) * side * 0.5;
	float groundY = ground_height(uv);
	vec3 world = vec3(root.x + across.x * width + offset.x, groundY + up, root.y + across.y * width + offset.y);
	gl_Position = mul(u_viewProj, vec4(world, 1.0));

	// The normal: the face, tilted up and rounded across the width (a blade is a little folded along its middle)
	v_normal = normalize(vec3(facing.x, 0.0, facing.y) + vec3(across.x, 0.0, across.y) * side * 0.35 + vec3(0.0, 0.8, 0.0));

	// The color: dark at the root; at the tip its clump's (most take the field's tip color; each other color has a noise of
	// its own, and the clumps where it is highest take it: patches, about its share of the field), a little uneven blade to
	// blade, and a share of dead blades anywhere
	vec3 tip = u_grassTip.rgb;
	for (int k = 0; k < 3; ++k)
	{
		float share = u_grassClumpColors[k].w;
		float n = grass_noise(cp * 0.6 + vec2(float(k) * 37.0 - 9.0, 31.0 - float(k) * 13.0));
		// value noise piles up around one half: a share s is roughly the part above 1 - 0.5 s - 0.25
		if (share > 0.0 && n > 0.75 - 0.5 * share)
		{
			tip = mix(tip, u_grassClumpColors[k].rgb, smoothstep(0.75 - 0.5 * share, 0.8 - 0.5 * share, n));
		}
	}
	tip *= 1.0 + (r2 - 0.5) * mix(0.2, 0.45, strength);
	if (fract(r1 * 7.31 + r2 * 3.17) < u_grassDead.w)
	{
		tip = u_grassDead.rgb;
	}
	vec3 color = mix(u_grassRoot.rgb, tip, smoothstep(0.0, 1.0, t));

	// The sun at its root: one read of the shadow map for the whole blade (a blade in a tree's shade is in it all along)
	float sunlit = 1.0;
	if (u_grassShadow.x > 0.0)
	{
		vec4 sp = mul(u_grassShadowMtx, vec4(root.x, groundY + 0.05, root.y, 1.0));
		vec3 proj = sp.xyz / sp.w;
		if (proj.x > 0.0 && proj.x < 1.0 && proj.y > 0.0 && proj.y < 1.0 && proj.z < 1.0)
		{
			sunlit = mix(1.0, shadow2D(s_grassShadow, vec3(proj.xy, proj.z - u_grassShadow.y)), u_grassShadow.x);
		}
	}
	v_color0 = vec4(color, sunlit);
	v_worldpos = world;
	v_texcoord0 = vec2(t, 0.0);
}
