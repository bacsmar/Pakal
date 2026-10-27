"""Bakes the ambient light of a level into a 3D grid of probes (global illumination, one bounce of the sun).

Each probe looks around with rays. A ray that leaves the scene sees the sky; one that lands on the ground or on a
building sees that surface lit by the sun (or in its shadow) plus a little sky light, times the surface's albedo. The
average is the light that reaches the probe from every direction: it is dark in narrow alleys and under walls, bright
in open streets, and warm near sunlit walls. The game reads it in the shaders instead of a flat ambient color.

The scene is boxes (buildings, containers, cars) on a flat ground, taken from the level builder. Units are meters,
level axes (x east, y north, z up).

Output: <name>.json (the grid's placement) and <name>.bin (RGBA8, x fastest, then y, then height).
"""
import json
import math
import sys

import numpy as np


def sphere_directions(count):
    """Roughly even directions over the whole sphere (a Fibonacci spiral)."""
    i = np.arange(count) + 0.5
    z = 1.0 - 2.0 * i / count
    r = np.sqrt(np.maximum(0.0, 1.0 - z * z))
    a = math.pi * (3.0 - math.sqrt(5.0)) * i
    return np.stack([r * np.cos(a), r * np.sin(a), z], axis=1)


def nearest_hit(origin, directions, boxes):
    """For each direction: the distance to the first box or the ground, and the surface normal there. t = inf for none."""
    count = len(directions)
    safe = np.where(np.abs(directions) < 1e-9, 1e-9, directions)
    inverse = 1.0 / safe
    t = np.full(count, np.inf)
    normal = np.zeros((count, 3))
    inside = False
    if len(boxes):
        t1 = (boxes[:, None, 0:3] - origin) * inverse[None]
        t2 = (boxes[:, None, 3:6] - origin) * inverse[None]
        near = np.minimum(t1, t2)
        far = np.maximum(t1, t2)
        entry = near.max(axis=2)
        exit_ = far.min(axis=2)
        hit = exit_ >= np.maximum(entry, 0.0)
        # a probe inside a box is in the wall: dark
        if np.any(hit & (entry < 0.0) & (exit_ > 0.0)):
            inside = True
        entry = np.where(hit & (entry > 0.0), entry, np.inf)
        best = entry.argmin(axis=0)
        t_box = entry[best, np.arange(count)]
        axis = near[best, np.arange(count)].argmax(axis=1)
        use = np.isfinite(t_box)
        t = np.where(use, t_box, t)
        n = np.zeros((count, 3))
        n[np.arange(count), axis] = -np.sign(directions[np.arange(count), axis])
        normal = np.where(use[:, None], n, normal)
    down = directions[:, 2] < -1e-6
    t_ground = np.where(down, -origin[2] / np.where(down, directions[:, 2], -1.0), np.inf)
    ground = t_ground < t
    t = np.where(ground, t_ground, t)
    normal = np.where(ground[:, None], np.array([0.0, 0.0, 1.0]), normal)
    return t, normal, inside


def in_shadow(points, to_sun, boxes):
    """Whether a point has a box between it and the sun."""
    if not len(boxes) or not len(points):
        return np.zeros(len(points), dtype=bool)
    d = np.where(np.abs(to_sun) < 1e-9, 1e-9, to_sun)
    t1 = (boxes[:, None, 0:3] - points[None]) / d
    t2 = (boxes[:, None, 3:6] - points[None]) / d
    near = np.minimum(t1, t2).max(axis=2)
    far = np.maximum(t1, t2).min(axis=2)
    return np.any((far >= np.maximum(near, 0.0)) & (far > 0.0), axis=0)


def bake(boxes, sun_travel, sun_color, sky_top, sky_horizon, bounds, cell=1.5, heights=(0.6, 2.4, 4.2, 6.0), rays=72,
         ground_albedo=0.32, wall_albedo=0.42, reference=None):
    """boxes: list of (x0, y0, x1, y1, height). Returns (rgba bytes, dims, origin, size)."""
    boxes = np.array([[b[0], b[1], 0.0, b[2], b[3], b[4]] for b in boxes], dtype=float).reshape(-1, 6)
    x0, y0, x1, y1 = bounds
    nx = int(math.ceil((x1 - x0) / cell))
    ny = int(math.ceil((y1 - y0) / cell))
    nz = len(heights)
    directions = sphere_directions(rays)
    to_sun = -np.array(sun_travel, dtype=float)
    to_sun /= np.linalg.norm(to_sun)
    sun = np.array(sun_color, dtype=float)
    sky_avg = (np.array(sky_top) + np.array(sky_horizon)) * 0.5
    sky_dir = lambda d: np.array(sky_horizon)[None] * (1 - np.clip(d[:, 2:3], 0, 1)) + np.array(sky_top)[None] * np.clip(d[:, 2:3], 0, 1)

    result = np.zeros((nz, ny, nx, 3))
    total = nx * ny * nz
    done = 0
    for hi, height in enumerate(heights):
        for iy in range(ny):
            for ix in range(nx):
                origin = np.array([x0 + (ix + 0.5) * cell, y0 + (iy + 0.5) * cell, height])
                t, normal, inside = nearest_hit(origin, directions, boxes)
                if inside:
                    result[hi, iy, ix] = 0.03
                    continue
                radiance = np.zeros((rays, 3))
                sky = ~np.isfinite(t)
                radiance[sky] = sky_dir(directions[sky])
                hit = ~sky
                if hit.any():
                    points = origin + directions[hit] * t[hit][:, None]
                    n = normal[hit]
                    shadowed = in_shadow(points + n * 0.05, to_sun, boxes)
                    ndl = np.maximum((n * to_sun).sum(axis=1), 0.0)
                    direct = sun[None] * (ndl * (~shadowed))[:, None]
                    albedo = np.where((n[:, 2] > 0.5)[:, None], ground_albedo, wall_albedo)
                    radiance[hit] = albedo * (direct + sky_avg[None] * 0.55)
                result[hi, iy, ix] = radiance.mean(axis=0)
                done += 1
        print('  probes: %d%%' % (100 * (hi + 1) // nz), file=sys.stderr)
    return result, (nx, ny, nz), (x0, y0), cell


def pack(result, ambient_reference):
    """Scales so that an open street matches the flat ambient the level had, and packs RGBA8 (half scale, the shader doubles)."""
    nz, ny, nx, _ = result.shape
    # the median of the open probes at the second height level (2.4 m) is the reference: a typical spot
    level = result[min(1, nz - 1)].reshape(-1, 3)
    open_air = level[level.mean(axis=1) > 0.1]            # leave out the probes inside buildings
    typical = np.median(open_air if len(open_air) else level, axis=0)
    norm = float(np.mean(ambient_reference) / max(float(np.mean(typical)), 1e-4))
    scaled = np.clip(result * norm * 0.5, 0.0, 1.0)
    rgba = np.zeros((nz, ny, nx, 4), dtype=np.uint8)
    rgba[..., :3] = (scaled * 255.0 + 0.5).astype(np.uint8)
    rgba[..., 3] = 255
    return rgba, norm


def write(path_prefix, rgba, dims, origin_xy, cell, heights, norm):
    nx, ny, nz = dims
    step = heights[1] - heights[0]
    with open(path_prefix + '.bin', 'wb') as f:
        f.write(rgba.tobytes())
    meta = {
        'dims': [nx, ny, nz],
        # level axes: x, y (north) and height; the game maps them to its own space
        'origin': [origin_xy[0], origin_xy[1], heights[0] - 0.5 * step],
        'size': [nx * cell, ny * cell, nz * step],
        'normalization': norm,
    }
    with open(path_prefix + '.json', 'w', encoding='utf-8') as f:
        json.dump(meta, f, indent=1)
    return meta
