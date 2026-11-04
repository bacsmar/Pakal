"""Escenas armadas: el apocalipsis contado en lugares (una barricada en un cruce, un choque, un retén, una tienda
saqueada), no la basura regada pareja. Lo que se ve en las demos de los paquetes ( barricadas, cinta, coches
quemados, cuerpos, juntos en un punto).

El preset las trae en "setpieces":

    {"setpieces": [
        {"name": "barricade", "at": "crossing", "count": 2, "apart": 60.0,
         "items": [{"role": "barrier", "n": [3, 5], "x": [-5, 5], "y": [-1, 1], "yaw": "across"},
                   {"role": "car", "n": [1, 2], "x": [-6, 6], "y": [2, 5], "yaw": "random"},
                   {"role": "cone", "n": [2, 4], "x": [-6, 6], "y": [-3, 3], "yaw": "random"}]},
        {"name": "looted_shop", "at": "shop", "count": 4, "apart": 30.0,
         "items": [{"role": "trash", "n": [3, 6], "x": [-3, 3], "y": [0.5, 2.5]}]}]}

'at': crossing (un cruce de calles; x a lo largo de una de ellas, y de través), street (un punto de una calle; x a lo
largo, y de través desde su eje), shop (frente a un edificio con rol shop; x a lo largo de la fachada, y hacia la calle), river_end (where a street dies
at the river: x along the street toward its end, the end at x 0; a road works site there).
'apart': la distancia mínima entre dos escenas del mismo nombre. Cada cosa ocupa su lugar y respeta lo ocupado; lo que
no cabe se omite. 'within' (opcional, por cosa): la región donde debe quedar (road, walk...). 'line' (opcional): las cosas en fila, una tras otra a
lo ancho (y) en una sola x, como un muro de barreras.
"""
import math
import random

from shapely.geometry import Point

import dress
import parcels
import roadmesh


def anchors(kind, net, buildings, catalog, rng):
    """Lugares para una escena: (x, y, ángulo de su eje x en radianes)."""
    out = []
    if kind == 'crossing':
        for c, a, _ in roadmesh.junctions(net):
            d = a['line'].project(c)
            p, q = a['line'].interpolate(max(d - 0.5, 0)), a['line'].interpolate(min(d + 0.5, a['line'].length))
            out.append((c.x, c.y, math.atan2(q.y - p.y, q.x - p.x)))
    elif kind == 'street':
        for e in net.edges:
            if e['kind'] in ('alley',) or e.get('bridge'):
                continue
            d = rng.uniform(5, 25)
            while d < e['line'].length - 5:
                p, q = e['line'].interpolate(d), e['line'].interpolate(d + 0.5)
                out.append((p.x, p.y, math.atan2(q.y - p.y, q.x - p.x)))
                d += rng.uniform(25, 45)
    elif kind == 'river_end':
        river = net.areas.get('river')
        for e in net.edges:
            if e['kind'] == 'alley' or river is None or river.is_empty:
                continue
            line = e['line']
            for at, back in ((0.0, 3.0), (line.length, line.length - 3.0)):
                p = line.interpolate(at)
                if river.distance(p) > e['width'] / 2 + 6.0 or not net.play.contains(p):
                    continue
                q = line.interpolate(back)
                out.append((p.x, p.y, math.atan2(p.y - q.y, p.x - q.x)))
    elif kind == 'shop':
        for b in buildings:
            if b['role'] != 'shop':
                continue
            f = dress.frame(catalog['models'][b['asset']], b)
            mx, my = (f['a'][0] + f['b'][0]) / 2, (f['a'][1] + f['b'][1]) / 2
            # x a lo largo de la fachada, y (x girado 90 grados) hacia la calle
            out.append((mx, my, math.atan2(-f['t'][1], -f['t'][0])))
    rng.shuffle(out)
    return out


def place_all(specs, net, buildings, catalog, models_for, occupancy, regions, seed):
    out = []
    for spec in specs:
        rng = random.Random(seed * 53 + sum(map(ord, spec['name'])))
        done = []
        for x, y, angle in anchors(spec['at'], net, buildings, catalog, rng):
            if len(done) >= spec.get('count', 1):
                break
            if any(math.hypot(x - px, y - py) < spec.get('apart', 40.0) for px, py in done):
                continue
            got = place(spec, x, y, angle, catalog, models_for, occupancy, regions, rng)
            if got:
                done.append((x, y))
                for g in got:
                    g['spawner'] = f'setpiece:{spec["name"]}'
                out += got
    return out


def place(spec, x, y, angle, catalog, models_for, occupancy, regions, rng):
    cx, cy = math.cos(angle), math.sin(angle)
    got = []
    for item in spec['items']:
        models = models_for(item['role'])
        if not models:
            continue
        within = regions.get(item['within']) if item.get('within') else None
        count = rng.randint(*item.get('n', [1, 1]))
        # 'line': the items end to end across the street (y), at one x: a wall, not a scatter
        line_x = rng.uniform(*item.get('x', [-2, 2])) if item.get('line') else None
        step = max(catalog['models'][models[0]]['size'][:2]) + 0.05 if item.get('line') else 0.0
        for n in range(count):
            for _try in range(6 if line_x is None else 1):
                if line_x is None:
                    lx, ly = rng.uniform(*item.get('x', [-2, 2])), rng.uniform(*item.get('y', [-2, 2]))
                else:
                    lx, ly = line_x, (n - (count - 1) / 2.0) * step
                px, py = x + cx * lx - cy * ly, y + cy * lx + cx * ly
                mode = item.get('yaw', 'random')
                if mode == 'along':
                    yaw = math.degrees(angle) + 90 + rng.uniform(-12, 12)
                elif mode == 'across':
                    yaw = math.degrees(angle) + (rng.uniform(-3, 3) if line_x is not None else rng.uniform(-15, 15))
                else:
                    yaw = rng.uniform(0, 360)
                asset = rng.choice(models)
                m = catalog['models'][asset]
                foot = parcels.footprint_poly(m, px, py, yaw)
                if within is not None and not within.contains(foot.centroid):
                    continue
                if occupancy.hits(foot.buffer(0.1)):
                    continue
                occupancy.add(foot)
                got.append({'asset': asset, 'x': round(px, 3), 'y': round(py, 3), 'yaw': round(yaw % 360, 2),
                            'scale': 1.0, 'role': item['role']})
                break
    return got
