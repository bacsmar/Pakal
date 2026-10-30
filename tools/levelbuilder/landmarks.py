"""Landmarks: one-off pieces a town is known by, placed where the brief says.

    letters(spec, net, catalog, models_for, occupancy, seed) -> placed entities

"letters": a word standing on the floor in big letter models (role "letter": one model per letter, its name ending in the
letter), each its own color, on a seawall promenade's esplanade (streetnet's 'malecon'). It reads from the game's camera:
the letters face it and the word runs left to right on the screen, whatever the shore's direction.

    {"word": "TOWN NAME", "height": 1.8, "camera": [-1, -1], "colors": [[r, g, b], ...], "gap": 0.12,
     "layout": "row"}    # a straight row across the screen through the esplanade ("along": where its middle stands)
     "layout": "arc"     # an arc bowing inland around the esplanade's tip ("radius", "back"; "face": "center")
     "layout": "shore"   # along the shore, "offset" meters inland, "shift" meters along it from the esplanade
"""
import math
import random

import shapely
from shapely.geometry import Point

import parcels


def _path(spec, m, t, total):
    """The line the word is laid along, and the arc's center (for letters that face it; None for the others)."""
    c, T, N = m['center'], m['tangent'], m['normal']
    layout = spec.get('layout', 'row')
    if layout == 'arc':
        # an arc bowing inland from a center out over the esplanade (an amphitheater around its tip)
        radius = spec.get('radius', 9.0)
        o = (c[0] - N[0] * spec.get('back', 3.0), c[1] - N[1] * spec.get('back', 3.0))
        half = total / 2 / radius + 0.05
        return o, shapely.geometry.LineString([(o[0] + radius * (math.cos(u) * N[0] + math.sin(u) * T[0]),
                                             o[1] + radius * (math.cos(u) * N[1] + math.sin(u) * T[1]))
                                            for u in [-half + 2 * half * k / 40 for k in range(41)]])
    if layout == 'shore':
        # along the shore, 'offset' meters inland, its middle 'shift' meters along it from the esplanade's
        off, shift, half = spec.get('offset', 8.0), spec.get('shift', -6.0), total / 2 + 0.5
        return None, shapely.geometry.LineString([(c[0] + T[0] * (shift + d) + N[0] * off, c[1] + T[1] * (shift + d) + N[1] * off)
                                            for d in [-half + 2 * half * k / 20 for k in range(21)]])
    # a straight row across the screen through the esplanade, its middle at 'along' of where it crosses the walk
    center = Point(c)
    across = shapely.geometry.LineString([(center.x - t[0] * 30, center.y - t[1] * 30), (center.x + t[0] * 30, center.y + t[1] * 30)])
    middle = center
    walk = m['band'].buffer(-0.5)
    crossing = across.intersection(walk) if not walk.is_empty else None
    if crossing is not None and not crossing.is_empty:
        span = max((g for g in getattr(crossing, 'geoms', [crossing])), key=lambda g: g.length)
        middle = span.interpolate(spec.get('along', 0.5), normalized=True)
    return None, shapely.geometry.LineString([(middle.x - t[0] * total / 2, middle.y - t[1] * total / 2),
                                        (middle.x + t[0] * total / 2, middle.y + t[1] * total / 2)])


def letters(spec, net, catalog, models_for, occupancy, seed=1):
    m = getattr(net, 'malecon', None)
    if not m or not spec.get('word'):
        return []
    found = {}
    for key in models_for(spec.get('letters', 'letter')):
        found.setdefault(key.rsplit('_', 1)[-1].upper(), key)
    word = spec['word'].upper()
    if any(c != ' ' and c not in found for c in word):
        return []
    rng = random.Random(seed * 17 + 3)
    # toward the camera (the level's direction from what it looks at to it), and the screen's right on the floor
    cx, cy = spec.get('camera', [-1.0, -1.0])
    k = math.hypot(cx, cy) or 1.0
    n = (cx / k, cy / k)
    t = (-n[1], n[0])
    yaw = parcels.yaw_facing(*n)
    height = spec.get('height', 1.8)
    gap, space = spec.get('gap', 0.12), spec.get('space', 0.6)
    sized = []
    for c in word:
        if c == ' ':
            sized.append((None, space, 1.0))
            continue
        model = catalog['models'][found[c]]
        scale = height / max(model['size'][2], 1e-6)
        sized.append((found[c], model['size'][0] * scale, scale))
    total = sum(w for _, w, _ in sized) + gap * (len(sized) - 1)
    center, path = _path(spec, m, t, total)
    # the word left to right on the screen: the path runs toward the screen's right
    a, b = path.coords[0], path.coords[-1]
    if (b[0] - a[0]) * t[0] + (b[1] - a[1]) * t[1] < 0:
        path = shapely.geometry.LineString(list(path.coords)[::-1])
    s = (path.length - total) / 2
    palette = spec.get('colors') or [[0.9, 0.2, 0.25], [0.15, 0.55, 0.85], [0.98, 0.78, 0.15], [0.2, 0.7, 0.35],
                                     [0.95, 0.45, 0.1], [0.6, 0.3, 0.75]]
    out = []
    for i, (key, w, scale) in enumerate(sized):
        if key:
            model = catalog['models'][key]
            c = s + w / 2 - (model['low'][0] + model['high'][0]) / 2 * scale
            p = path.interpolate(min(max(c, 0.0), path.length))
            x, y = p.x, p.y
            if spec.get('face') == 'center' and center is not None:
                yaw = parcels.yaw_facing(center[0] - x, center[1] - y)
            color = palette[(i + rng.randrange(len(palette))) % len(palette)] if spec.get('shuffle') else palette[i % len(palette)]
            out.append({'asset': key, 'x': round(x, 3), 'y': round(y, 3), 'z': round(-model['low'][1] * scale, 3),
                        'yaw': round(yaw + rng.uniform(-1.5, 1.5), 2), 'scale': round(scale, 3), 'role': 'landmark_letter',
                        'spawner': 'landmark:letters', 'tint': color})
            foot = parcels.footprint_poly(model, x, y, yaw, scale)
            occupancy.add(foot.buffer(0.6))
        s += w + gap
    # the word's own room: nothing tall in a band round it (a palm's trunk 2 m off keeps its crown off the letters)
    occupancy.add(path.buffer(2.5, cap_style=2))
    return out
