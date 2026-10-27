$input v_texcoord0

// FidelityFX Super Resolution 1.0, RCAS (robust contrast adaptive sharpening): after EASU, at the screen's size. Sharpens each
// pixel against its 4 neighbours as much as it can without leaving their range (no halos), less on noise.
// Port of AMD's ffx_fsr1.h (Copyright (c) 2021 Advanced Micro Devices, Inc., MIT license). The colors and lumas are half
// precision on OpenGL ES, as in its 16-bit path (fs_fsr_easu.sc says why); the positions stay full precision.

#include <bgfx_shader.sh>

#if BGFX_SHADER_LANGUAGE_GLSL
	#define HALF mediump float
	#define HALF3 mediump vec3
#else
	#define HALF float
	#define HALF3 vec3
#endif

SAMPLER2D(s_fsrInput, 0);

uniform vec4 u_fsrRcas; // xy: 1 / size, z: sharpness (exp2(-stops): 1 is the most, 0.5 a stop less)

#define FSR_RCAS_LIMIT (0.25 - (1.0 / 16.0))

HALF3 rcas_load(vec2 p)
{
	return texture2DLod(s_fsrInput, (p + 0.5) * u_fsrRcas.xy, 0.0).rgb;
}

void main()
{
	vec2 sp = floor(gl_FragCoord.xy);
	//    b
	//  d e f
	//    h
	HALF3 b = rcas_load(sp + vec2( 0.0, -1.0));
	HALF3 d = rcas_load(sp + vec2(-1.0,  0.0));
	HALF3 e = rcas_load(sp);
	HALF3 f = rcas_load(sp + vec2( 1.0,  0.0));
	HALF3 h = rcas_load(sp + vec2( 0.0,  1.0));
	HALF bL = b.g + 0.5 * (b.b + b.r);
	HALF dL = d.g + 0.5 * (d.b + d.r);
	HALF eL = e.g + 0.5 * (e.b + e.r);
	HALF fL = f.g + 0.5 * (f.b + f.r);
	HALF hL = h.g + 0.5 * (h.b + h.r);
	// Noise detection: less sharpening where the center stands out of a flat neighbourhood
	HALF nz = 0.25 * (bL + dL + fL + hL) - eL;
	HALF range = max(max(max(bL, dL), max(fL, hL)), eL) - min(min(min(bL, dL), min(fL, hL)), eL);
	nz = clamp(abs(nz) / max(range, 1.0 / 16384.0), 0.0, 1.0);
	nz = 1.0 - 0.5 * nz;
	// The most negative lobe that keeps the result within the neighbours' range
	HALF3 mn4 = min(min(b, d), min(f, h));
	HALF3 mx4 = max(max(b, d), max(f, h));
	HALF3 hitMin = mn4 / (4.0 * mx4 + 1.0 / 16384.0);
	HALF3 hitMax = (1.0 - mx4) / (4.0 * mn4 - 4.0 - 1.0 / 16384.0);
	HALF3 lobeRGB = max(-hitMin, hitMax);
	HALF lobe = max(-FSR_RCAS_LIMIT, min(max(lobeRGB.r, max(lobeRGB.g, lobeRGB.b)), 0.0)) * u_fsrRcas.z;
	lobe *= nz;
	gl_FragColor = vec4((lobe * (b + d + h + f) + e) / (4.0 * lobe + 1.0), 1.0);
}
