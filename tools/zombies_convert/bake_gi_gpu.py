"""bake_gi.bake on the GPU (CUDA through CuPy): the same probes, rays and light, one thread per probe and ray.

bake_gi.bake uses it when CuPy and a CUDA GPU are there (PAKAL_GI_GPU=0 keeps the CPU); the CPU version stays as the
fallback and the reference: tests/levelbuilder compares both. The math is bake_gi's, in single precision:
- a ray that leaves the scene sees the sky (horizon to zenith by its height);
- one that lands on the ground or a box sees that surface lit by the sun (unless a box is in the way) plus a little
  sky, times its albedo;
- a probe inside a box is in the wall: dark.
"""
import math

import cupy as cp
import numpy as np

KERNEL = r'''
#define INFINITY __int_as_float(0x7f800000)
extern "C" __global__ void probe_rays(const float* boxes, const int boxCount, const float* directions, const int rays,
    const float x0, const float y0, const float cell, const float* heights, const int nx, const int ny, const int nz,
    const float* toSun, const float* sun, const float* skyTop, const float* skyHorizon, const float* skyAvg,
    const float groundAlbedo, const float wallAlbedo, float* radiance, int* inside)
{
    const long long index = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    const long long total = (long long)nx * ny * nz * rays;
    if (index >= total) return;
    const int r = (int)(index % rays);
    const long long probe = index / rays;
    const int ix = (int)(probe % nx);
    const int iy = (int)((probe / nx) % ny);
    const int iz = (int)(probe / ((long long)nx * ny));
    const float o[3] = { x0 + (ix + 0.5f) * cell, y0 + (iy + 0.5f) * cell, heights[iz] };
    const float d[3] = { directions[r * 3], directions[r * 3 + 1], directions[r * 3 + 2] };
    float inv[3];
    for (int a = 0; a < 3; ++a) inv[a] = 1.0f / (fabsf(d[a]) < 1e-9f ? 1e-9f : d[a]);
    // the nearest box in front, and the axis it was entered through
    float best = INFINITY;
    int bestAxis = -1;
    for (int b = 0; b < boxCount; ++b)
    {
        const float* box = boxes + b * 6;
        float entry = -INFINITY, exit_ = INFINITY;
        int axis = 0;
        for (int a = 0; a < 3; ++a)
        {
            const float t1 = (box[a] - o[a]) * inv[a], t2 = (box[a + 3] - o[a]) * inv[a];
            const float nearT = fminf(t1, t2), farT = fmaxf(t1, t2);
            if (nearT > entry) { entry = nearT; axis = a; }
            exit_ = fminf(exit_, farT);
        }
        if (exit_ >= fmaxf(entry, 0.0f))
        {
            if (entry < 0.0f && exit_ > 0.0f) inside[probe] = 1;   // the probe is in this box
            if (entry > 0.0f && entry < best) { best = entry; bestAxis = axis; }
        }
    }
    float n[3] = { 0.0f, 0.0f, 0.0f };
    if (bestAxis >= 0) n[bestAxis] = d[bestAxis] > 0.0f ? -1.0f : 1.0f;
    if (d[2] < -1e-6f)
    {
        const float ground = -o[2] / d[2];
        if (ground < best) { best = ground; n[0] = 0.0f; n[1] = 0.0f; n[2] = 1.0f; }
    }
    float* out = radiance + index * 3;
    if (!isfinite(best))
    {
        const float up = fminf(fmaxf(d[2], 0.0f), 1.0f);
        for (int c = 0; c < 3; ++c) out[c] = skyHorizon[c] * (1.0f - up) + skyTop[c] * up;
        return;
    }
    // the surface it landed on: in the sun or in a box's shadow
    const float p[3] = { o[0] + d[0] * best + n[0] * 0.05f, o[1] + d[1] * best + n[1] * 0.05f, o[2] + d[2] * best + n[2] * 0.05f };
    float sinv[3];
    for (int a = 0; a < 3; ++a) sinv[a] = 1.0f / (fabsf(toSun[a]) < 1e-9f ? 1e-9f : toSun[a]);
    bool shadowed = false;
    for (int b = 0; b < boxCount && !shadowed; ++b)
    {
        const float* box = boxes + b * 6;
        float entry = -INFINITY, exit_ = INFINITY;
        for (int a = 0; a < 3; ++a)
        {
            const float t1 = (box[a] - p[a]) * sinv[a], t2 = (box[a + 3] - p[a]) * sinv[a];
            entry = fmaxf(entry, fminf(t1, t2));
            exit_ = fminf(exit_, fmaxf(t1, t2));
        }
        shadowed = exit_ >= fmaxf(entry, 0.0f) && exit_ > 0.0f;
    }
    const float ndl = fmaxf(n[0] * toSun[0] + n[1] * toSun[1] + n[2] * toSun[2], 0.0f);
    const float albedo = n[2] > 0.5f ? groundAlbedo : wallAlbedo;
    for (int c = 0; c < 3; ++c) out[c] = albedo * (sun[c] * ndl * (shadowed ? 0.0f : 1.0f) + skyAvg[c] * 0.55f);
}
'''

_kernel = None


def bake(boxes, sun_travel, sun_color, sky_top, sky_horizon, bounds, cell=1.5, heights=(0.6, 2.4, 4.2, 6.0), rays=72,
         ground_albedo=0.32, wall_albedo=0.42, reference=None):
    """bake_gi.bake's arguments and result (a numpy (nz, ny, nx, 3) array, dims, origin, cell), computed on the GPU."""
    global _kernel
    if _kernel is None:
        _kernel = cp.RawKernel(KERNEL, 'probe_rays')
    import bake_gi
    box_array = np.array([[b[0], b[1], 0.0, b[2], b[3], b[4]] for b in boxes], dtype=np.float32).reshape(-1, 6)
    x0, y0, x1, y1 = bounds
    nx = int(math.ceil((x1 - x0) / cell))
    ny = int(math.ceil((y1 - y0) / cell))
    nz = len(heights)
    directions = bake_gi.sphere_directions(rays).astype(np.float32)
    to_sun = -np.array(sun_travel, dtype=np.float64)
    to_sun /= np.linalg.norm(to_sun)
    sky_avg = (np.array(sky_top) + np.array(sky_horizon)) * 0.5
    f32 = lambda v: cp.asarray(np.asarray(v, dtype=np.float32))   # noqa: E731
    g_boxes = f32(box_array) if len(box_array) else cp.zeros(6, dtype=cp.float32)
    g_dirs, g_heights = f32(directions), f32(heights)
    g_sun, g_to_sun = f32(sun_color), f32(to_sun)
    g_top, g_horizon, g_avg = f32(sky_top), f32(sky_horizon), f32(sky_avg)
    probes = nx * ny * nz
    radiance = cp.zeros((probes * rays, 3), dtype=cp.float32)
    inside = cp.zeros(probes, dtype=cp.int32)
    threads = 128
    count = probes * rays
    _kernel(((count + threads - 1) // threads,), (threads,),
            (g_boxes, np.int32(len(box_array)), g_dirs, np.int32(rays), np.float32(x0), np.float32(y0), np.float32(cell),
             g_heights, np.int32(nx), np.int32(ny), np.int32(nz), g_to_sun, g_sun, g_top, g_horizon, g_avg,
             np.float32(ground_albedo), np.float32(wall_albedo), radiance, inside))
    mean = radiance.reshape(probes, rays, 3).mean(axis=1)
    mean = cp.where((inside > 0)[:, None], cp.float32(0.03), mean)
    result = cp.asnumpy(mean).astype(np.float64).reshape(nz, ny, nx, 3)
    return result, (nx, ny, nz), (x0, y0), cell


def main():
    """Run from another Python (bake_gi without CuPy): the bake's arguments in a JSON file, its result into a .npy."""
    import json
    import sys
    source, target = sys.argv[1], sys.argv[2]
    with open(source, encoding='utf-8') as f:
        job = json.load(f)
    result, _, _, _ = bake(**job)
    np.save(target, result)


if __name__ == '__main__':
    main()
