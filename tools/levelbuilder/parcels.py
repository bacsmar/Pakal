"""Manzanas poligonales -> lotes con edificio: cada edificio con su frente a la calle, uno junto a otro a lo largo de cada
lado de la manzana (fachadas corridas, como en un centro), sin salirse de la manzana ni encimarse.

Los lados se recorren del más largo al más corto (las esquinas las toma el primero que llega). Para cada tramo se elige un
modelo que quepa en lo que queda del lado y en el fondo de la manzana; si no cabe ninguno se avanza un poco. Lo que no se
llena queda como patio (la manzana menos los edificios), para la vegetación y la basura de los spawners.
"""
import math

from shapely.geometry import LineString, Polygon
from shapely.ops import unary_union

import citykit_path  # noqa: F401  (pone tools/zombies_convert en sys.path)
import citykit  # noqa: E402


def yaw_facing(nx, ny):
    """El giro (grados) con el que el frente de un modelo (que mira al sur con giro 0) mira hacia (nx, ny)."""
    return math.degrees(math.atan2(nx, -ny)) % 360.0


def footprint_poly(model, x, y, yaw, scale=1.0, shrink=0.0):
    return Polygon(citykit.footprint(model['low'], model['high'], x, y, yaw, scale, shrink))


def centered(model, cx, cy, yaw):
    """Where a model's origin goes for its footprint to be centered on (cx, cy) (some packs' buildings have theirs at a
    corner), and that footprint."""
    foot = footprint_poly(model, cx, cy, yaw)
    x, y = cx + (cx - foot.centroid.x), cy + (cy - foot.centroid.y)
    return x, y, footprint_poly(model, x, y, yaw)


def fill_block(block, catalog, choices, rng, setback=0.3, gap=(0.0, 0.15), taken=None, min_edge=6.0, first=None, prefer=None,
               corner=None, corner_chance=1.0, step=1.0, camera=None, away=-0.25, passage=None, passages=None,
               unique=False):
    """Llena la manzana 'block' (Polygon) con edificios de 'choices' ([(id, peso)]), uno pegado al otro. 'corner': [(id,
    peso)] para el primer lote de cada lado (la esquina: la tiendita). 'first': [(id, rol)] que van antes, centrados en el
    lado más largo, o en el más cercano al punto 'prefer' (una iglesia frente a la plaza). 'taken': polígonos ya ocupados.
    'step': lo que se avanza cuando no cabe nada. Devuelve [{asset, x, y, yaw, poly, front, width, corner}] y el patio que
    queda.

    'camera' (the direction from what a fixed camera looks at to it, on the floor): a building whose front would face
    away from it (its street's normal against the camera under 'away') is turned around on its own footprint, its front
    to the block's yard, which the camera sees over the near side ("inward": True). Without it every front faces its
    street (a camera that turns or walks).

    'passage' (meters): a way in from the street to the yard (a vacant lot in the middle is walked into), kept free of
    buildings: on the side that faces the camera most (any long side without one), away from its corners. Its polygon
    goes to 'passages' (a list) for the caller to keep bardas and clutter off it."""
    placed = []
    occupied = list(taken or [])
    inner = block.buffer(-0.05)
    ring = block.exterior
    coords = list(ring.coords)
    ccw = ring.is_ccw
    edges = []
    for a, b in zip(coords, coords[1:]):
        length = math.hypot(b[0] - a[0], b[1] - a[1])
        if length >= min_edge:
            edges.append((length, a, b))
    edges.sort(key=lambda e: -e[0])
    models = catalog['models']
    if passage and edges:
        cam = camera or (0.0, -1.0)

        def facing(e):
            _, a, b = e
            length = math.hypot(b[0] - a[0], b[1] - a[1])
            t = ((b[0] - a[0]) / length, (b[1] - a[1]) / length)
            n = (t[1], -t[0]) if ccw else (-t[1], t[0])
            return n[0] * cam[0] + n[1] * cam[1]
        long_enough = [e for e in edges if e[0] >= passage + 12.0]
        if long_enough:
            length, a, b = max(long_enough, key=facing)
            s = rng.uniform(6.0 + passage / 2, length - 6.0 - passage / 2)
            mid = (a[0] + (b[0] - a[0]) * s / length, a[1] + (b[1] - a[1]) * s / length)
            c = block.centroid
            way = LineString([mid, (c.x, c.y)]).buffer(passage / 2, cap_style=2).intersection(block)
            occupied.append(way)
            if passages is not None:
                passages.append(way)

    def place(asset, a, t, n, s, length, center=False):
        m = models[asset]
        yaw = (yaw_facing(*n) + m.get('turn', 0.0)) % 360.0
        corners = citykit.footprint(m['low'], m['high'], 0.0, 0.0, yaw, 1.0, 0.0)
        along = [cx * t[0] + cy * t[1] for cx, cy in corners]
        out = [cx * n[0] + cy * n[1] for cx, cy in corners]
        width = max(along) - min(along)
        if center:
            s = (length - width) / 2
        if s + width > length + 0.01:
            return None, width
        start = s - min(along)
        x = a[0] + t[0] * start - n[0] * (max(out) + setback)
        y = a[1] + t[1] * start - n[1] * (max(out) + setback)
        poly = footprint_poly(m, x, y, yaw)
        if not inner.contains(poly.buffer(-0.08)):
            return None, width
        if any(poly.buffer(-0.08).intersects(o) for o in occupied):
            return None, width
        return {'asset': asset, 'x': round(x, 3), 'y': round(y, 3), 'yaw': round(yaw, 2), 'poly': poly,
                'front': (round(n[0], 3), round(n[1], 3)), 'width': round(width, 3)}, width

    def frame(a, b):
        length = math.hypot(b[0] - a[0], b[1] - a[1])
        t = ((b[0] - a[0]) / length, (b[1] - a[1]) / length)
        # afuera de la manzana: a la derecha de a->b si el anillo es antihorario
        n = (t[1], -t[0]) if ccw else (-t[1], t[0])
        return length, t, n

    first_edges = sorted(edges, key=lambda e: math.hypot((e[1][0] + e[2][0]) / 2 - prefer[0], (e[1][1] + e[2][1]) / 2 - prefer[1]))         if prefer else edges
    for asset, role in first or []:
        if not first_edges:
            break
        length, a, b = first_edges[0]
        length, t, n = frame(a, b)
        got, _ = place(asset, a, t, n, 0.0, length, center=True)
        if got:
            got['role'] = role
            placed.append(got)
            occupied.append(got['poly'])

    ids = [c for c, _ in choices]
    weights = [w for _, w in choices]
    corner_ids = [c for c, _ in corner or []]
    corner_weights = [w for _, w in corner or []]
    for length, a, b in edges:
        length, t, n = frame(a, b)
        s = 0.0
        at_corner = bool(corner_ids) and rng.random() < corner_chance
        while s < length - 3.0:
            pool, pool_w = (corner_ids, corner_weights) if at_corner else (ids, weights)
            tries = rng.choices(pool, pool_w, k=min(8, len(pool) * 2)) if pool else []
            # los que caben en lo que queda, del más ancho al más angosto; si ninguno, cualquiera del resto que quepa
            done = False
            for asset in sorted(set(tries), key=lambda k: -models[k]['size'][0]) + \
                    sorted(set(ids) - set(tries), key=lambda k: models[k]['size'][0]):
                if unique and any(p['asset'] == asset for p in placed):
                    continue
                got, width = place(asset, a, t, n, s, length)
                if got:
                    got['corner'] = at_corner and asset in corner_ids
                    placed.append(got)
                    occupied.append(got['poly'])
                    s += width + rng.uniform(*gap)
                    done = True
                    break
            if not done:
                s += step
            at_corner = False
    if camera is not None:
        k = math.hypot(*camera) or 1.0
        cam = (camera[0] / k, camera[1] / k)
        for p in placed:
            n = p['front']
            if n[0] * cam[0] + n[1] * cam[1] >= away:
                continue
            c = p['poly'].centroid
            yaw = (p['yaw'] + 180.0) % 360.0
            x, y, poly = centered(models[p['asset']], c.x, c.y, yaw)
            p.update(x=round(x, 3), y=round(y, 3), yaw=round(yaw, 2), poly=poly, front=(-n[0], -n[1]), inward=True)
    yard = block.difference(unary_union([p['poly'] for p in placed])) if placed else block
    return placed, yard
