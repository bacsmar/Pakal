"""Spawners como datos (la idea de Gaia: lugares candidatos, una aptitud de 0 a 1 por máscaras, y variación por instancia).

Un spawner (JSON, ver presets/*.json):

    {"name": "palmeras_malecon", "role": "palm",            # o "decal": "pothole" (calcomanía en vez de modelo)
     "where": "promenade",                                  # la región donde se buscan lugares (coastal.py las nombra)
     "within": "promenade",                                 # (opcional) la planta del modelo debe quedar adentro de esta
     "places": "poisson", "spacing": 8.0,                   # poisson | row | cluster | line | perimeter
     "perimeter": {"of": "buildings", "offset": 0.6, "spacing": 3.0, "count": [1, 3], "radius": 0.7},
     "line": {"along": "coastline", "offset": 3.0, "spacing": 10.0},
     "row": {"edges": ["avenue"], "offset": 1.4, "spacing": 18.0, "sides": [-1, 1]},
     "cluster": {"spacing": 30.0, "count": [3, 6], "radius": 4.0},
     "density": 1.2,                                        # (opcional) como mucho tantos por 100 m2 de la región
     "masks": [{"type": "distance", "to": "sea", "range": [2, 6], "invert": true},
               {"type": "noise", "scale": 25, "range": [0.4, 0.7]},
               {"type": "inside", "region": "plaza", "invert": true}],
     "threshold": 0.2,
     "yaw": "random",                                       # random | street | along | snap90 | face:<región> | away:<región> | fixed:<grados>
     "scale": [0.9, 1.2], "clearance": 0.4, "occupy": "footprint" | "trunk",
     "size": [1.5, 3.0]}                                    # (decals) lado en metros

Las máscaras se evalúan en cada candidato (exactas, con shapely). Se multiplican; 'op' (min, max, add, sub) cambia cómo
entra cada una. Un candidato pasa si su aptitud supera 'threshold' y un número al azar queda bajo ella. Cada cosa puesta
ocupa su planta en la grilla de ocupación: lo grande primero (el orden de la lista), lo chico después. Determinista: cada
spawner tiene su propio generador a partir de la semilla y su nombre.
"""
import math
import random
import zlib

import numpy as np
import shapely
from shapely.geometry import LineString, Point, Polygon

from geom import lines, polygons
import parcels


class Occupancy:
    """Lo que ya ocupa lugar en el piso: polígonos en cubetas de 4 m."""

    def __init__(self, cell=4.0):
        self.cell = cell
        self.buckets = {}

    def _keys(self, bounds):
        x0, y0, x1, y1 = bounds
        for i in range(math.floor(x0 / self.cell), math.floor(x1 / self.cell) + 1):
            for j in range(math.floor(y0 / self.cell), math.floor(y1 / self.cell) + 1):
                yield i, j

    def add(self, poly):
        for key in self._keys(poly.bounds):
            self.buckets.setdefault(key, []).append(poly)

    def hits(self, poly):
        seen = set()
        for key in self._keys(poly.bounds):
            for other in self.buckets.get(key, []):
                if id(other) in seen:
                    continue
                seen.add(id(other))
                if other.intersects(poly):
                    return True
        return False


class ValueNoise2D:
    def __init__(self, seed, scale):
        rng = np.random.default_rng(seed)
        self.table = rng.random((257, 257)).astype(np.float32)
        self.scale = scale

    def __call__(self, xy):
        p = xy / self.scale + 64.0
        i = np.floor(p).astype(int)
        f = p - i
        f = f * f * (3 - 2 * f)
        i0, j0 = i[:, 0] % 256, i[:, 1] % 256
        t = self.table
        a, b = t[i0, j0], t[i0 + 1, j0]
        c, d = t[i0, j0 + 1], t[i0 + 1, j0 + 1]
        top = a + (b - a) * f[:, 0]
        bottom = c + (d - c) * f[:, 0]
        return top + (bottom - top) * f[:, 1]


def ramp(values, lo, hi):
    if hi == lo:
        return (values >= lo).astype(np.float32)
    return np.clip((values - lo) / (hi - lo), 0.0, 1.0)


def band(values, lo, hi, fade):
    """1 dentro de [lo, hi], bajando a 0 en 'fade' fuera de cada lado."""
    fade = max(fade, 1e-6)
    return np.clip(np.minimum((values - lo) / fade + 1.0, (hi - values) / fade + 1.0), 0.0, 1.0).astype(np.float32)


def evaluate(masks, points, regions, seed, height=None):
    """La aptitud (0..1) de cada punto (N x 2) según las máscaras. 'height(x, y)': la altura del suelo (para las máscaras
    'height' y 'slope'; sin ella valen 1)."""
    fitness = np.ones(len(points), dtype=np.float32)
    if not len(points):
        return fitness
    for k, mask in enumerate(masks):
        kind = mask['type']
        if kind == 'inside':
            geometry = regions.get(mask['region'])
            value = shapely.contains_xy(geometry, points[:, 0], points[:, 1]).astype(np.float32) if geometry is not None \
                else np.zeros(len(points), np.float32)
        elif kind == 'distance':
            geometry = regions.get(mask['to'])
            if geometry is None or geometry.is_empty:
                value = np.ones(len(points), np.float32)
            else:
                d = shapely.distance(shapely.points(points), geometry)
                lo, hi = mask.get('range', [0.0, 10.0])
                value = 1.0 - ramp(d, lo, hi)   # 1 cerca, 0 lejos
        elif kind == 'noise':
            noise = ValueNoise2D(seed * 101 + k, mask.get('scale', 20.0))
            lo, hi = mask.get('range', [0.3, 0.7])
            value = ramp(noise(points), lo, hi)
        elif kind in ('height', 'slope'):
            # como las reglas de Gaia: la altura del suelo (metros) o su pendiente (subida por metro), dentro de 'range'
            # (y 'fade' de margen afuera: 10 % del rango si no se da)
            if height is None:
                value = np.ones(len(points), np.float32)
            else:
                if kind == 'height':
                    measured = np.array([height(x, y) for x, y in points], np.float32)
                else:
                    e = mask.get('step', 1.0)
                    measured = np.array([math.hypot(height(x + e, y) - height(x - e, y), height(x, y + e) - height(x, y - e)) / (2 * e)
                                         for x, y in points], np.float32)
                lo, hi = mask.get('range', [0.0, 1.0])
                value = band(measured, lo, hi, mask.get('fade', 0.1 * (hi - lo)))
        else:
            raise ValueError(f'máscara desconocida: {kind}')
        if mask.get('invert'):
            value = 1.0 - value
        value = value * mask.get('weight', 1.0) + (1.0 - mask.get('weight', 1.0))
        op = mask.get('op', 'mul')
        if op == 'mul':
            fitness *= value
        elif op == 'min':
            fitness = np.minimum(fitness, value)
        elif op == 'max':
            fitness = np.maximum(fitness, value)
        elif op == 'add':
            fitness = np.clip(fitness + value, 0, 1)
        elif op == 'sub':
            fitness = np.clip(fitness - value, 0, 1)
    return fitness


def poisson(region, spacing, rng, limit=20000):
    """Puntos con al menos 'spacing' entre sí dentro de la región (Bridson)."""
    if region is None or region.is_empty:
        return []
    x0, y0, x1, y1 = region.bounds
    cell = spacing / math.sqrt(2)
    grid = {}
    points, active = [], []

    def ok(p):
        gx, gy = int((p[0] - x0) / cell), int((p[1] - y0) / cell)
        for i in range(gx - 2, gx + 3):
            for j in range(gy - 2, gy + 3):
                q = grid.get((i, j))
                if q is not None and (q[0] - p[0]) ** 2 + (q[1] - p[1]) ** 2 < spacing * spacing:
                    return False
        return True

    def add(p):
        grid[(int((p[0] - x0) / cell), int((p[1] - y0) / cell))] = p
        points.append(p)
        active.append(p)

    # semillas en cada pedazo de la región (si no, una región en varias partes se llena solo en una)
    for piece in polygons(region):
        c = piece.representative_point()
        if ok((c.x, c.y)):
            add((c.x, c.y))
    while active and len(points) < limit:
        i = rng.randrange(len(active))
        base = active[i]
        for _ in range(20):
            a = rng.uniform(0, 2 * math.pi)
            r = rng.uniform(spacing, 2 * spacing)
            p = (base[0] + math.cos(a) * r, base[1] + math.sin(a) * r)
            if x0 <= p[0] <= x1 and y0 <= p[1] <= y1 and ok(p):
                add(p)
                break
        else:
            active.pop(i)
    inside = shapely.contains_xy(region, np.array([p[0] for p in points]), np.array([p[1] for p in points]))
    return [p for p, keep in zip(points, inside) if keep]


def row_points(net, spec, rng):
    """Lugares cada 'spacing' metros a lo largo de las calles, a 'offset' de la guarnición (sobre la banqueta), a uno o
    los dos lados, lejos de los cruces. Devuelve (x, y, dirección de la calle)."""
    out = []
    kinds = set(spec.get('edges', ['avenue', 'street', 'hill_street']))
    crossings = None
    others = [e for e in net.edges if e['kind'] != 'alley']
    for e in net.edges:
        if e['kind'] not in kinds:
            continue
        for side in spec.get('sides', [-1, 1]):
            shifted = e['line'].offset_curve(side * (e['width'] / 2 + spec.get('offset', 1.2)))
            for piece in lines(shifted):
                d = rng.uniform(0, spec['spacing'])
                while d < piece.length:
                    p = piece.interpolate(d)
                    q = piece.interpolate(min(d + 0.5, piece.length))
                    out.append((p.x, p.y, math.atan2(q.y - p.y, q.x - p.x), e))
                    d += spec['spacing'] * rng.uniform(0.85, 1.15)
    # fuera de los cruces: lejos de las demás calles
    result = []
    for x, y, angle, e in out:
        pt = Point(x, y)
        if any(o is not e and o['line'].distance(pt) < o['width'] / 2 + spec.get('clear_crossing', 4.0) for o in others):
            continue
        result.append((x, y, angle))
    return result


def facing_yaw(point, regions, mode, angle=None, rng=None):
    if mode == 'random':
        return rng.uniform(0, 360)
    if mode == 'snap90':
        return rng.choice((0, 90, 180, 270))
    if mode.startswith('fixed:'):
        # one yaw for all, a little varied (a statue facing the game's camera: "fixed:315")
        return float(mode[6:]) + rng.uniform(-4.0, 4.0)
    if mode == 'along' and angle is not None:
        # el largo del modelo (su eje z, de frente a fondo) a lo largo de la calle
        return (math.degrees(angle) + 90.0 + rng.choice((0, 180))) % 360
    if mode.startswith('face:') or mode.startswith('away:'):
        # face: el frente hacia la región; away: la espalda (una banca recargada en la jardinera mira al andador)
        target = regions.get(mode[5:])
        if target is not None and not target.is_empty:
            p = Point(point)
            q = target.boundary.interpolate(target.boundary.project(p)) if not target.contains(p) else target.centroid
            if math.hypot(q.x - p.x, q.y - p.y) > 1e-6:
                yaw = parcels.yaw_facing(q.x - p.x, q.y - p.y)
                return yaw if mode.startswith('face:') else (yaw + 180.0) % 360
        return rng.uniform(0, 360)
    road = regions.get('road')
    if mode in ('street', 'along') and road is not None and not road.is_empty:
        p = Point(point)
        q = road.boundary.interpolate(road.boundary.project(p))
        dx, dy = q.x - p.x, q.y - p.y
        if math.hypot(dx, dy) > 1e-6:
            if mode == 'along':
                return (parcels.yaw_facing(dx, dy) + 90.0) % 360
            return parcels.yaw_facing(dx, dy)
    return rng.uniform(0, 360)


def run(spec, net, regions, catalog, models_for, occupancy, seed, mesh=None, decal_height=None, ground_height=None):
    """Pone lo de un spawner. Devuelve [{asset, x, y, yaw, scale, role}] (los decals van directo a 'mesh').
    'ground_height(x, y)': la altura del suelo, para las máscaras 'height' y 'slope'."""
    rng = random.Random(zlib.crc32(f'{seed}/{spec["name"]}'.encode()))
    where = regions.get(spec.get('where'))
    within = regions.get(spec.get('within')) if spec.get('within') else None
    places = spec.get('places', 'poisson')
    candidates = []
    if places == 'row':
        candidates = row_points(net, spec['row'], rng)
    elif places == 'line':
        # a lo largo de una línea con nombre (la costa), desplazada tierra adentro
        ln = spec['line']
        curve = regions.get(ln['along'])
        if curve is not None:
            for piece in [part for base in lines(curve) for part in lines(base.offset_curve(ln.get('offset', 2.0)))]:
                d = rng.uniform(0, ln['spacing'])
                while d < piece.length:
                    p = piece.interpolate(d)
                    q = piece.interpolate(min(d + 0.5, piece.length))
                    candidates.append((p.x, p.y, math.atan2(q.y - p.y, q.x - p.x)))
                    d += ln['spacing'] * rng.uniform(0.85, 1.15)
    elif places == 'perimeter':
        # alrededor de lo que nombra 'of' (los edificios), a 'offset' de su borde: manchas pegadas a las paredes
        pm = spec['perimeter']
        around = regions.get(pm['of'])
        if around is not None and not around.is_empty:
            for piece in polygons(around.buffer(pm.get('offset', 0.6), join_style=2)):
                for ring in [piece.exterior]:
                    d = rng.uniform(0, pm['spacing'])
                    while d < ring.length:
                        p = ring.interpolate(d)
                        for _ in range(rng.randint(*pm.get('count', [1, 1]))):
                            a, r = rng.uniform(0, 2 * math.pi), pm.get('radius', 0.0) * math.sqrt(rng.random())
                            candidates.append((p.x + math.cos(a) * r, p.y + math.sin(a) * r, None))
                        d += pm['spacing'] * rng.uniform(0.6, 1.4)
    elif places == 'cluster':
        cl = spec['cluster']
        for cx, cy in poisson(where, cl['spacing'], rng):
            for _ in range(rng.randint(*cl['count'])):
                a, r = rng.uniform(0, 2 * math.pi), cl['radius'] * math.sqrt(rng.random())
                candidates.append((cx + math.cos(a) * r, cy + math.sin(a) * r, None))
    else:
        candidates = [(x, y, None) for x, y in poisson(where, spec.get('spacing', 4.0), rng)]
    if where is not None and places not in ('row', 'line'):
        inside = shapely.contains_xy(where, np.array([c[0] for c in candidates] or [0.0]), np.array([c[1] for c in candidates] or [0.0]))
        candidates = [c for c, keep in zip(candidates, inside) if keep] if candidates else []
    if not candidates:
        return []
    points = np.array([(c[0], c[1]) for c in candidates], dtype=np.float64)
    fitness = evaluate(spec.get('masks', []), points, regions, zlib.crc32(spec['name'].encode()) + seed, ground_height)
    limit = None
    if spec.get('density') and where is not None:
        limit = int(spec['density'] * where.area / 100.0)
    order = list(range(len(candidates)))
    rng.shuffle(order)
    placed = []
    models = [] if spec.get('decal') else models_for(spec['role'])
    if not spec.get('decal') and not models:
        return []
    size_noise = ValueNoise2D(zlib.crc32(spec['name'].encode()) + seed * 7, spec['size_noise'].get('scale', 30.0)) \
        if spec.get('size_noise') else None
    for i in order:
        if limit is not None and len(placed) >= limit:
            break
        f = float(fitness[i])
        if f <= spec.get('threshold', 0.0) or rng.random() > f:
            continue
        x, y, angle = candidates[i]
        if spec.get('decal'):
            from decals import KINDS
            lo, hi = spec.get('size', [1.5, 3.0])
            if spec.get('size_noise'):
                # el tamaño por ruido: manchones de chicos y unos cuantos grandes, no todos del mismo
                n = size_noise(np.array([[x, y]]))[0]
                size = lo + (hi - lo) * float(np.clip(n * 1.4 - 0.2 + rng.uniform(-0.15, 0.15), 0.0, 1.0))
            else:
                size = rng.uniform(lo, hi)
            stretch = rng.uniform(*spec.get('stretch', [1.0, 1.0]))
            foot = Point(x, y).buffer(size * 0.35 * stretch)
            if within is not None and not within.contains(foot):
                continue
            mesh.decal((x, y), size, rng.uniform(0, 360), rng.choice(KINDS[spec['decal']]),
                       decal_height(x, y) if decal_height else 0.01, stretch=stretch, mirror=rng.random() < 0.5)
            placed.append({'decal': spec['decal'], 'x': x, 'y': y})
            continue
        asset = rng.choice(models)
        m = catalog['models'][asset]
        yaw = facing_yaw((x, y), regions, spec.get('yaw', 'random'), angle, rng)
        scale = round(rng.uniform(*spec.get('scale', [1.0, 1.0])), 3)
        if spec.get('occupy') == 'trunk':
            foot = Point(x, y).buffer(spec.get('trunk', 0.5) * scale)
        else:
            foot = parcels.footprint_poly(m, x, y, yaw, scale)
        if within is not None and not within.contains(foot):
            continue
        claim = foot.buffer(spec.get('clearance', 0.3))
        if occupancy.hits(claim):
            continue
        occupancy.add(foot.buffer(spec.get('clearance', 0.3) * 0.5))
        placed.append({'asset': asset, 'x': round(x, 3), 'y': round(y, 3), 'yaw': round(yaw % 360, 2), 'scale': scale,
                       'role': spec['role'], 'spawner': spec['name'],
                       # "tints": colors it is painted in, one at random each (a beach's umbrellas)
                       **({'tint': rng.choice(spec['tints'])} if spec.get('tints') else {}),
                       # "unbreakable": scenery even when its model breaks elsewhere (the base's parked cars: the missions
                       # bring the cars that blow up)
                       **({'unbreakable': True} if spec.get('unbreakable') else {})})
    return placed
