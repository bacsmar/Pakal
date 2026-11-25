#ifndef PAKAL_AO_DEPTH_SH
#define PAKAL_AO_DEPTH_SH

// Two normalized channels preserve the depth of the surface the AO texel describes.
// The decode is linear, so hardware filtering interpolates depth and visibility together.
vec2 ao_encode_depth(float depth)
{
	float ranged = clamp(depth / 512.0, 0.0, 1.0) * 255.0;
	float high = floor(ranged);
	return vec2(high / 255.0, ranged - high);
}

float ao_decode_depth(vec2 encoded)
{
	return dot(encoded, vec2(512.0, 512.0 / 255.0));
}

#endif
