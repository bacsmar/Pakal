// The cutout of an alpha-tested texture (leaves, grass, fences), kept as wide far away as near: a far read of the texture
// comes from a small mip, where the leaves' edges are blurred under the cutoff and a blade breaks into pixels that blink as
// it moves (the grass's shimmer). Its alpha is lifted by its mip level, a quarter a level (Ben Golus's mip-scaled alpha
// test); the level from how many texels the pixel spans, the textures taken as 1024 wide (commercial packs' atlases and free
// packs' leaves are 512 to 2048).
float cutout_mip_scale(vec2 uv)
{
	vec2 texels = uv * 1024.0;
	vec2 dx = dFdx(texels), dy = dFdy(texels);
	float lod = 0.5 * log2(max(1.0, max(dot(dx, dx), dot(dy, dy))));
	return 1.0 + lod * 0.25;
}

// The alpha lifted by its mip level: the coverage and the discard alike (the engine's shaders)
float cutout_alpha(float alpha, vec2 uv)
{
	return alpha * cutout_mip_scale(uv);
}
