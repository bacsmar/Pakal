$input v_texcoord0

// FidelityFX Super Resolution 1.0, EASU (edge adaptive spatial upsampling): the picture drawn at a lower resolution, made the
// size of the screen. Each output pixel reads the 12 input pixels around it, finds the direction and strength of the edge
// there, and filters along it with a windowed lobe; the result is kept within the 4 nearest pixels (no ringing).
// Port of AMD's ffx_fsr1.h (Copyright (c) 2021 Advanced Micro Devices, Inc., MIT license). Like its 16-bit path, the colors,
// lumas, the edge and the weights are half precision on OpenGL ES (a phone's GPU does twice as much of it; 8-bit colors lose
// nothing); the positions where it reads stay full precision (a 2400-pixel screen needs more than half's 11 bits).

#include <bgfx_shader.sh>

#if BGFX_SHADER_LANGUAGE_GLSL
	#define HALF mediump float
	#define HALF2 mediump vec2
	#define HALF3 mediump vec3
#else
	#define HALF float
	#define HALF2 vec2
	#define HALF3 vec3
#endif

SAMPLER2D(s_fsrInput, 0);

uniform vec4 u_fsrCon0; // xy: input size / output size, zw: half of that minus half (pixel centers)
uniform vec4 u_fsrCon1; // xy: 1 / input size, zw: (1, -1) / input size
uniform vec4 u_fsrCon2; // (-1, 2, 1, 2) / input size
uniform vec4 u_fsrCon3; // xy: (0, 4) / input size, w: 1 for a plain bilinear stretch instead (to compare)

HALF3 easu_load(vec2 p)
{
	return texture2DLod(s_fsrInput, p, 0.0).rgb;
}

// Luma times 2, in 2 operations
HALF easu_luma(HALF3 c)
{
	return c.g + 0.5 * (c.r + c.b);
}

// The edge's direction and length, accumulated for one of the 4 nearest pixels with its bilinear weight
void easu_set(inout HALF2 dir, inout HALF len, HALF w, HALF lA, HALF lB, HALF lC, HALF lD, HALF lE)
{
	HALF lenX = max(abs(lD - lC), abs(lC - lB));
	HALF dirX = lD - lB;
	dir.x += dirX * w;
	lenX = clamp(abs(dirX) / max(lenX, 1.0 / 16384.0), 0.0, 1.0);
	lenX *= lenX;
	len += lenX * w;
	HALF lenY = max(abs(lE - lC), abs(lC - lA));
	HALF dirY = lE - lA;
	dir.y += dirY * w;
	lenY = clamp(abs(dirY) / max(lenY, 1.0 / 16384.0), 0.0, 1.0);
	lenY *= lenY;
	len += lenY * w;
}

// One tap of the filter: the lobe along the rotated, stretched offset
void easu_tap(inout HALF3 aC, inout HALF aW, HALF2 off, HALF2 dir, HALF2 len, HALF lob, HALF clp, HALF3 c)
{
	HALF2 v = vec2(dot(off, dir), dot(off, vec2(-dir.y, dir.x)));
	v *= len;
	HALF d2 = min(dot(v, v), clp);
	HALF wB = 0.4 * d2 - 1.0;
	HALF wA = lob * d2 - 1.0;
	wB *= wB;
	wA *= wA;
	wB = 1.5625 * wB - 0.5625;
	HALF w = wB * wA;
	aC += c * w;
	aW += w;
}

void main()
{
	if (u_fsrCon3.w > 0.5)
	{
		gl_FragColor = vec4(easu_load((floor(gl_FragCoord.xy) + 0.5) * u_fsrCon0.xy * u_fsrCon1.xy), 1.0);
		return;
	}
	vec2 pp = floor(gl_FragCoord.xy) * u_fsrCon0.xy + u_fsrCon0.zw;
	vec2 fp = floor(pp);
	pp -= fp;
	vec2 p0 = fp * u_fsrCon1.xy + u_fsrCon1.zw;
	vec2 p1 = p0 + u_fsrCon2.xy;
	vec2 p2 = p0 + u_fsrCon2.zw;
	vec2 p3 = p0 + u_fsrCon3.xy;
	vec4 off = vec4(-0.5, 0.5, -0.5, 0.5) * u_fsrCon1.xxyy;
	//    b c
	//  e f g h
	//  i j k l
	//    n o
	HALF3 bC = easu_load(p0 + off.xw); HALF bL = easu_luma(bC);
	HALF3 cC = easu_load(p0 + off.yw); HALF cL = easu_luma(cC);
	HALF3 iC = easu_load(p1 + off.xw); HALF iL = easu_luma(iC);
	HALF3 jC = easu_load(p1 + off.yw); HALF jL = easu_luma(jC);
	HALF3 fC = easu_load(p1 + off.yz); HALF fL = easu_luma(fC);
	HALF3 eC = easu_load(p1 + off.xz); HALF eL = easu_luma(eC);
	HALF3 kC = easu_load(p2 + off.xw); HALF kL = easu_luma(kC);
	HALF3 lC = easu_load(p2 + off.yw); HALF lL = easu_luma(lC);
	HALF3 hC = easu_load(p2 + off.yz); HALF hL = easu_luma(hC);
	HALF3 gC = easu_load(p2 + off.xz); HALF gL = easu_luma(gC);
	HALF3 oC = easu_load(p3 + off.yz); HALF oL = easu_luma(oC);
	HALF3 nC = easu_load(p3 + off.xz); HALF nL = easu_luma(nC);

	HALF2 dir = vec2(0.0, 0.0);
	HALF len = 0.0;
	HALF2 f = vec2(pp);   // where the pixel lies between the 4 nearest (0..1)
	easu_set(dir, len, (1.0 - f.x) * (1.0 - f.y), bL, eL, fL, gL, jL);
	easu_set(dir, len, f.x * (1.0 - f.y), cL, fL, gL, hL, kL);
	easu_set(dir, len, (1.0 - f.x) * f.y, fL, iL, jL, kL, nL);
	easu_set(dir, len, f.x * f.y, gL, jL, kL, lL, oL);

	// Normalize the direction (none on flat areas), shape the length
	HALF2 dir2 = dir * dir;
	HALF dirR = dir2.x + dir2.y;
	bool zro = dirR < (1.0 / 16384.0);
	dirR = zro ? 1.0 : inversesqrt(dirR);
	dir.x = zro ? 1.0 : dir.x;
	dir *= dirR;
	len = len * 0.5;
	len *= len;
	// Stretch the kernel: 1 along the axes to sqrt(2) on diagonals; the window from sqrt(2) to past 2 on edges
	HALF stretch = dot(dir, dir) / max(abs(dir.x), abs(dir.y));
	HALF2 len2 = vec2(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);
	HALF lob = 0.5 - 0.29 * len;
	HALF clp = 1.0 / lob;

	HALF3 min4 = min(min(fC, gC), min(jC, kC));
	HALF3 max4 = max(max(fC, gC), max(jC, kC));
	HALF3 aC = vec3(0.0, 0.0, 0.0);
	HALF aW = 0.0;
	easu_tap(aC, aW, vec2( 0.0, -1.0) - f, dir, len2, lob, clp, bC);
	easu_tap(aC, aW, vec2( 1.0, -1.0) - f, dir, len2, lob, clp, cC);
	easu_tap(aC, aW, vec2(-1.0,  1.0) - f, dir, len2, lob, clp, iC);
	easu_tap(aC, aW, vec2( 0.0,  1.0) - f, dir, len2, lob, clp, jC);
	easu_tap(aC, aW, vec2( 0.0,  0.0) - f, dir, len2, lob, clp, fC);
	easu_tap(aC, aW, vec2(-1.0,  0.0) - f, dir, len2, lob, clp, eC);
	easu_tap(aC, aW, vec2( 1.0,  1.0) - f, dir, len2, lob, clp, kC);
	easu_tap(aC, aW, vec2( 2.0,  1.0) - f, dir, len2, lob, clp, lC);
	easu_tap(aC, aW, vec2( 2.0,  0.0) - f, dir, len2, lob, clp, hC);
	easu_tap(aC, aW, vec2( 1.0,  0.0) - f, dir, len2, lob, clp, gC);
	easu_tap(aC, aW, vec2( 1.0,  2.0) - f, dir, len2, lob, clp, oC);
	easu_tap(aC, aW, vec2( 0.0,  2.0) - f, dir, len2, lob, clp, nC);
	gl_FragColor = vec4(min(max4, max(min4, aC / aW)), 1.0);
}
