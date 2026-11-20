$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texLife, 1);

// A sprite's own looks (SpriteComponent2D::set_flash, set_rim, set_hue); all zero: the picture as it is, times its color
uniform vec4 u_spriteFlash;   // rgb: the color it turns to, a: how much (0..1): a hit's white flash
uniform vec4 u_spriteRim;     // rgb: the rim's light, a: its width in texels (0: none)
uniform vec4 u_spriteRimDir;  // xy: where the light comes from in the picture (0,0: all around), zw: 1/texture size
uniform vec4 u_spriteHue;     // x: the hue turned (turns, 0..1), y: a hue left alone (skin, 0..1), z: how wide (0: none kept)
uniform vec4 u_spriteLife;    // x: seconds, y: strength (0: none); s_texLife: the mask (r water, g sway, b lights, 1 - a falling water)

vec3 rgb_to_hsv(vec3 c)
{
	vec4 k = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	vec4 p = mix(vec4(c.bg, k.wz), vec4(c.gb, k.xy), step(c.b, c.g));
	vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
	float d = q.x - min(q.w, q.y);
	return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + 1.0e-10)), d / (q.x + 1.0e-10), q.x);
}

vec3 hsv_to_rgb(vec3 c)
{
	vec3 p = abs(fract(c.xxx + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
	return c.z * mix(vec3_splat(1.0), clamp(p - 1.0, 0.0, 1.0), c.y);
}

void main()
{
	vec4 texColor;
	vec4 life = vec4_splat(0.0);
	if (u_spriteLife.y > 0.0)
	{
		// life over a still picture (SpriteComponent2D::set_life): where the mask says, the picture is read a few texels aside
		// (its alpha is stored the other way, 1 where nothing falls: the mask stays opaque, visible in a picture viewer)
		life = texture2D(s_texLife, v_texcoord0);
		life.a = 1.0 - life.a;
		life *= u_spriteLife.y;
		float t = u_spriteLife.x;
		vec2 texel = u_spriteRimDir.zw;
		vec2 at = v_texcoord0 / max(texel, vec2_splat(1.0e-6));   // (in texels)
		vec2 uv = v_texcoord0;
		// water: it slides sideways in slow waves whose crests bend along it (straight ones read as stripes), two of them at
		// lengths that never line up, and a little up and down
		float bend = sin(at.x * 0.031 + t * 0.7) * 1.6 + sin(at.x * 0.0127 - t * 0.4) * 2.3;
		uv.x += life.r * (sin(at.y * 0.23 + t * 1.9 + bend) * 1.1 + sin(at.y * 0.57 - t * 1.3 + at.x * 0.043) * 0.45) * texel.x;
		uv.y += life.r * sin(at.x * 0.05 + t * 1.3 + at.y * 0.02) * 0.6 * texel.y;
		// what sways: sideways, a wave going through it
		// (a slow sway of the whole, and a quicker flutter running through it, sideways and a little up and down: a leaf or a
		// paper flag moved all of a piece by two texels, read smooth, was hardly seen)
		// (the masks mark it faintly, picked by colour, blurred and faded toward where it hangs from: all of it from 0.4 up)
		float sway = min(1.0, life.g * 2.5);
		uv.x += sway * (sin(t * 1.7 + at.x * 0.02 + at.y * 0.03) * 3.0 + sin(t * 4.3 + at.x * 0.11 + at.y * 0.07) * 1.2) * texel.x;
		uv.y += sway * sin(t * 3.1 + at.x * 0.09 + at.y * 0.05) * 0.8 * texel.y;
		// falling water (a fountain's jets, a cascade): it shivers sideways as it runs down
		uv.x += life.a * sin(at.y * 0.6 - t * 10.0 + at.x * 0.4) * 0.6 * texel.x;
		// where it moves, the picture is read between its texels (a pixel picture moved by a fraction of one, read at the nearest,
		// steps); elsewhere, as it is
		if (life.r + life.g + life.a > 0.004)
		{
			vec2 p = uv / texel - 0.5;
			vec2 f = fract(p);
			vec2 base = (floor(p) + 0.5) * texel;
			vec4 a = texture2D(s_texColor, base);
			vec4 b = texture2D(s_texColor, base + vec2(texel.x, 0.0));
			vec4 c = texture2D(s_texColor, base + vec2(0.0, texel.y));
			vec4 d = texture2D(s_texColor, base + texel);
			texColor = mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
		}
		else texColor = texture2D(s_texColor, uv);
	}
	else texColor = texture2D(s_texColor, v_texcoord0);
	vec3 rgb = texColor.rgb;
	if (u_spriteLife.y > 0.0)
	{
		float t = u_spriteLife.x;
		vec2 at = v_texcoord0 / max(u_spriteRimDir.zw, vec2_splat(1.0e-6));
		// the water glints: here and there a short bright dash lights up and fades, each at its own place and pace (cells of
		// 16x6 texels, staggered every other row, a hash each: no grid of them to see)
		vec2 cellAt = at / vec2(16.0, 6.0);
		cellAt.x += 0.5 * mod(floor(cellAt.y), 2.0);
		vec2 cell = floor(cellAt);
		vec3 h3 = fract(vec3(cell.xyx) * vec3(0.1031, 0.1030, 0.0973));
		h3 += dot(h3, h3.yzx + 33.33);
		h3 = fract((h3.xxy + h3.yzz) * h3.zyx);
		vec2 off = fract(cellAt) - vec2(0.2 + 0.6 * h3.x, 0.3 + 0.4 * h3.y);
		float dash = exp(-(off.x * off.x * 22.0 + off.y * off.y * 30.0));
		float twinkle = pow(max(0.0, sin(t * (0.9 + 1.6 * h3.z) + h3.x * 40.0)), 24.0);
		float lit = dot(texColor.rgb, vec3(0.3, 0.5, 0.2));
		rgb += life.r * dash * twinkle * (0.25 + 0.45 * lit);
		// the water running down: short bright dashes falling through it, each thin column at its own pace
		float column = 0.5 + 0.5 * sin(at.x * 0.9 + sin(at.x * 0.13) * 4.0);
		float drop = 0.5 + 0.5 * sin(at.y * 0.15 - t * 7.0 + at.x * 0.37);
		rgb += life.a * pow(column, 8.0) * pow(drop, 4.0) * 0.35;
		// lights flicker, each on its own: a phase that changes smoothly across the picture (no seams through a light's halo)
		float phase = 6.2832 * (0.5 + 0.5 * sin(at.x * 0.021 + 1.3) * cos(at.y * 0.027 + 0.4));
		rgb *= 1.0 + life.b * (0.18 * sin(t * 9.0 + phase) + 0.10 * sin(t * 23.0 + phase * 1.7));
	}
	// another palette: the hues turned, but not the one kept (the skin) nor the greys
	if (u_spriteHue.x != 0.0)
	{
		vec3 hsv = rgb_to_hsv(rgb);
		float away = abs(fract(hsv.x - u_spriteHue.y + 0.5) - 0.5);
		float keep = u_spriteHue.z > 0.0 ? 1.0 - smoothstep(u_spriteHue.z * 0.5, u_spriteHue.z, away) : 0.0;
		float turn = (1.0 - keep) * smoothstep(0.08, 0.2, hsv.y);
		rgb = mix(rgb, hsv_to_rgb(vec3(fract(hsv.x + u_spriteHue.x), hsv.y, hsv.z)), turn);
	}
	// a rim of light on the inside of the outline: where the picture ends a few texels away (toward the light, when it has a side)
	if (u_spriteRim.a > 0.0 && texColor.a > 0.05)
	{
		vec2 texel = u_spriteRimDir.zw * u_spriteRim.a;
		vec2 dir = u_spriteRimDir.xy;
		float edge = 0.0;
		if (dot(dir, dir) > 0.0)
		{
			edge = 1.0 - texture2D(s_texColor, v_texcoord0 + dir * texel).a;
			edge = max(edge, 0.6 * (1.0 - texture2D(s_texColor, v_texcoord0 + dir * texel * 0.5).a));
		}
		else
		{
			float a = texture2D(s_texColor, v_texcoord0 + vec2(texel.x, 0.0)).a;
			a = min(a, texture2D(s_texColor, v_texcoord0 - vec2(texel.x, 0.0)).a);
			a = min(a, texture2D(s_texColor, v_texcoord0 + vec2(0.0, texel.y)).a);
			a = min(a, texture2D(s_texColor, v_texcoord0 - vec2(0.0, texel.y)).a);
			edge = 1.0 - a;
		}
		rgb = mix(rgb, u_spriteRim.rgb, clamp(edge, 0.0, 1.0) * 0.85);
	}
	vec4 color = vec4(rgb, texColor.a) * v_color0;
	// the flash, over the tint (a hit is white even on a sprite tinted dark)
	color.rgb = mix(color.rgb, u_spriteFlash.rgb, clamp(u_spriteFlash.a, 0.0, 1.0));
	gl_FragColor = color;
}
