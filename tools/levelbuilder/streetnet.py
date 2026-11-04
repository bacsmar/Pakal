"""Red de calles de un pueblo costero, hecha de curvas (no de una cuadrícula).

La costa manda: el malecón la sigue, y las avenidas son la costa desplazada tierra adentro a profundidades de manzana (con
algo de ruido). Las calles transversales salen de la costa hacia el cerro, con la normal de la costa girada por el ángulo
de su distrito: donde la costa se curva se abren en abanico y las manzanas quedan como cuñas y trapecios. Un río baja del
cerro al mar y parte el pueblo en dos distritos (cada uno con su ángulo y su largo de manzana); solo algunas avenidas lo
cruzan por un puente. Tierra adentro, una calle del cerro sigue el pie de la loma; algunas transversales llegan a ella como
callejones peatonales (escalinatas) y otras terminan antes.

El mar queda al oeste. Entra un brief (el bloque "coastal" del brief del generador, ver DEFAULTS) y una semilla; sale un
Network: los ejes (polilíneas con ancho y tipo) y los polígonos (mar, río, malecón, calles, banquetas, manzanas, plazas,
cerro), todo en metros con los ejes del nivel. Determinista: la misma semilla da la misma red.

    python tools/levelbuilder/streetnet.py [--seed N] [--out preview.png]     una vista rápida (PIL), sin Blender
"""
import argparse
import json
import math
import random
from pathlib import Path

import numpy as np
from shapely.geometry import LineString, Point, Polygon, box
from shapely.ops import substring, unary_union

from geom import Curve, Noise1D, catmull_rom, lines, polygons, ray, rotate

DEFAULTS = {
    'size': [320.0, 280.0],        # ancho (x) y alto (y) de lo que se juega, centrado en 0,0
    'sea': 46.0,                   # cuánto mar se ve al oeste, a media altura
    'bend': 0.5,                   # cuánto se curva la costa hacia el mar en medio (fracción de 'sea')
    'tilt': 0.12,                  # la costa inclinada: x crece con y por este factor
    'wiggle': 4.0,                 # ruido de la costa (m)
    'promenade': 13.0,             # ancho del malecón
    'sidewalk': 2.5,               # ancho de las banquetas
    'avenues': [11.0, 9.0, 9.0],   # anchos de las calles a lo largo de la costa (la primera va junto al malecón)
    'blocks': [40.0, 56.0],        # fondo de las manzanas entre avenidas (mín, máx)
    'hill': {'band': [30.0, 42.0], 'street': 7.0, 'wiggle': 9.0},
    'cross': {'spacing': [40.0, 70.0], 'width': 8.0, 'dead_end': 0.15, 'stairs': 0.45, 'alley': 4.0, 'jitter': 3.0},
    'river': {'at': 0.42, 'width': 15.0, 'bank': 5.0, 'bridges': [0, 2], 'footbridge': True, 'meander': 5.0},
    'districts': [{'angle': 4.0, 'spacing': 1.0}, {'angle': -12.0, 'spacing': 0.8}],   # al sur del río, al norte
    'plazas': 1,                   # plazas por distrito, en la primera franja de manzanas (junto al malecón)
    # una playa de arena entre el malecón y el mar en un distrito (0 = al sur del río), que se angosta hacia el río
    'beach': {'width': 24.0, 'district': 0, 'fade': 50.0},
    # un muelle que sale del malecón al mar, en la playa: largo, ancho y el radio de su plataforma
    'pier': {'length': 60.0, 'width': 4.0, 'platform': 7.0, 'at': 0.55},
    'fillet': 0.0,                 # streets meeting at an angle: their corners filled round with this radius (m)
    # a seawall promenade in the other district (off unless the brief asks): a band of concrete along the sea ('walk'
    # meters), the rest of the promenade stays cobblestone, and an esplanade jutting into the sea at 'at' of the district
    # (half 'length' along the shore, 'depth' out): the place of a landmark, its seaward edge stepped down to the water
    'malecon': None,
}

MARGIN = 70.0   # lo que la red se extiende fuera de lo jugable (para que nada se corte a la vista)
STEP = 0.2      # the rise of each of the esplanade's steps down to the sea


def merged(defaults, brief):
    out = json.loads(json.dumps(defaults))
    for key, value in (brief or {}).items():
        if isinstance(value, dict) and isinstance(out.get(key), dict):
            out[key].update(value)
        else:
            out[key] = value
    return out


class Network:
    """El resultado: ejes y polígonos. 'edges': [{id, kind, width, line, bridge}] con kind avenue / street / hill_street /
    alley (peatonal); 'blocks': [{id, poly, district, band, kind}] con kind block o plaza."""

    def __init__(self):
        self.edges = []
        self.blocks = []
        self.entrances = []      # [(x, y, kind)] por donde llegan los zombis: calles que salen del mapa, callejones al cerro
        self.areas = {}          # nombre -> geometría: sea, land, river, bank, promenade, footbridge, road, sidewalk, paving, hill

    def edge(self, kind, line, width, **extra):
        e = {'id': f'{kind}_{sum(1 for x in self.edges if x["kind"] == kind) + 1:02d}', 'kind': kind, 'line': line,
             'width': width}
        e.update(extra)
        self.edges.append(e)
        return e

    def to_json(self):
        def coords(g):
            return [[[round(x, 2), round(y, 2)] for x, y in p.exterior.coords] for p in polygons(g)]
        return {
            'edges': [{'id': e['id'], 'kind': e['kind'], 'width': e['width'], 'bridge': e.get('bridge', False),
                       'points': [[round(x, 2), round(y, 2)] for x, y in e['line'].coords]} for e in self.edges],
            'blocks': [{'id': b['id'], 'kind': b['kind'], 'district': b['district'], 'band': b['band'],
                        'points': [[round(x, 2), round(y, 2)] for x, y in b['poly'].exterior.coords]} for b in self.blocks],
            'entrances': [[round(x, 2), round(y, 2), k] for x, y, k in self.entrances],
            'areas': {name: coords(g) for name, g in self.areas.items()},
        }


def first_hit(line, cutters):
    """Distancia a lo largo de 'line' de su primer cruce con alguna de las líneas 'cutters' (None si no cruza)."""
    best = None
    for cutter in cutters:
        hit = line.intersection(cutter)
        if hit.is_empty:
            continue
        points = [hit] if hit.geom_type == 'Point' else [g for g in getattr(hit, 'geoms', []) if g.geom_type == 'Point']
        for p in points:
            d = line.project(p)
            if d > 1.0 and (best is None or d < best):
                best = d
    return best


def cut(line, distance):
    """La línea desde su inicio hasta 'distance'."""
    if distance >= line.length:
        return line
    coords = [line.coords[0]]
    for c in list(line.coords)[1:]:
        if line.project(Point(c)) < distance:
            coords.append(c)
    coords.append(tuple(line.interpolate(distance).coords[0]))
    return LineString(coords)


def generate(brief=None, seed=1):
    b = merged(DEFAULTS, brief)
    rng = random.Random(seed)
    W, H = b['size']
    x0, y0, x1, y1 = -W / 2, -H / 2, W / 2, H / 2
    play = box(x0, y0, x1, y1)
    world = box(x0 - MARGIN, y0 - MARGIN, x1 + MARGIN, y1 + MARGIN)
    net = Network()
    net.pier = None
    sw = b['sidewalk']

    # --- la costa: de sur a norte, el mar a la izquierda (oeste), tierra adentro es la normal derecha (este) ---
    wiggle = Noise1D(seed * 7 + 1, 45.0)
    mid = x0 + b['sea']
    control = []
    for y in np.linspace(y0 - MARGIN - 40, y1 + MARGIN + 40, 9):
        u = y / (H / 2)
        x = mid - b['bend'] * b['sea'] * (1 - min(u * u, 1.6)) + b['tilt'] * y + b['wiggle'] * wiggle(y)
        control.append((x, y))
    coast = Curve(catmull_rom(control, 2.0))
    s_lo, s_hi = coast.project((mid, y0)), coast.project((mid, y1))

    land = Polygon(list(coast.points) + [(x1 + MARGIN + 200, coast.points[-1][1]), (x1 + MARGIN + 200, coast.points[0][1])])
    land = land.buffer(0).intersection(world)
    net.areas['land'] = land
    net.areas['sea'] = world.difference(land)

    # --- las avenidas: la costa desplazada; cada una con su ruido, sin cruzarse ---
    widths = b['avenues']
    offsets = []
    d = b['promenade'] + sw + widths[0] / 2
    for k, w in enumerate(widths):
        if k > 0:
            depth = rng.uniform(*b['blocks'])
            d = d + widths[k - 1] / 2 + sw + depth + sw + w / 2
        noise = Noise1D(seed * 31 + k, 80.0)
        amp = 0.0 if k == 0 else 0.12 * (b['blocks'][0])
        offsets.append((lambda base, n, a: (lambda s: base + a * n(s)))(d, noise, amp))
    hb = b['hill']
    hill_noise = Noise1D(seed * 53, 60.0)
    hill_base = d + widths[-1] / 2 + sw + rng.uniform(*hb['band']) + sw + hb['street'] / 2
    hill_offset = lambda s: hill_base + hb['wiggle'] * hill_noise(s)
    avenue_lines = [LineString(coast.offset(f)) for f in offsets]
    hill_line = LineString(coast.offset(hill_offset))
    hill_edge = LineString(coast.offset(lambda s: hill_offset(s) + hb['street'] / 2 + sw))
    hill_area = Polygon(list(hill_edge.coords) + [(x1 + MARGIN + 200, hill_edge.coords[-1][1]),
                                                  (x1 + MARGIN + 200, hill_edge.coords[0][1])]).buffer(0).intersection(world)
    net.hill_edge = hill_edge

    # --- el río: del cerro al mar, serpenteando ---
    rv = b['river']
    s_r = s_lo + rv['at'] * (s_hi - s_lo)
    meander = Noise1D(seed * 97, 40.0)
    origin, n_r, t_r = coast.at(s_r), coast.normal(s_r), coast.tangent(s_r)
    river_pts = [tuple(origin + n_r * dd + t_r * rv['meander'] * meander(dd)) for dd in np.arange(-40.0, hill_base + 120.0, 4.0)]
    river_line = LineString(river_pts)
    river = river_line.buffer(rv['width'] / 2, cap_style=2).intersection(world)
    bank = river_line.buffer(rv['width'] / 2 + rv['bank'], cap_style=2).intersection(world)
    net.areas['river'] = river.intersection(land)
    net.areas['bank'] = bank.difference(river).intersection(land)
    net.areas['river_mouth'] = river
    hill_area = hill_area.difference(bank)
    net.areas['hill'] = hill_area

    # --- los distritos y las transversales ---
    clear = rv['width'] / 2 + rv['bank'] + b['cross']['width']
    districts = [(s_lo - MARGIN, s_r - clear), (s_r + clear, s_hi + MARGIN)]
    cr = b['cross']
    cross_lines = []
    for di, (sa, sb) in enumerate(districts):
        spec = b['districts'][min(di, len(b['districts']) - 1)]
        s = sa + rng.uniform(0.3, 0.8) * cr['spacing'][0] * spec.get('spacing', 1.0)
        previous = []
        while s < sb:
            direction = rotate(coast.normal(s), spec.get('angle', 0.0) + rng.uniform(-cr['jitter'], cr['jitter']))
            start = coast.at(s) + coast.normal(s) * (offsets[0](s))
            line = ray(start, direction, hill_base * 2.5)
            # hasta la calle del cerro, o antes si topa con otra transversal (una T)
            stop = first_hit(line, [hill_line] + previous)
            if stop is None:
                s += rng.uniform(*cr['spacing']) * spec.get('spacing', 1.0)
                continue
            line = cut(line, stop)
            # no cruza el río: se queda con el tramo que sale de la avenida del malecón
            if line.intersects(bank):
                pieces = [p for p in lines(line.difference(bank)) if p.distance(Point(start)) < 0.5]
                if not pieces:
                    s += rng.uniform(*cr['spacing']) * spec.get('spacing', 1.0)
                    continue
                line = pieces[0]
            # algunas terminan antes del cerro (en una avenida), otras suben al cerro como callejón peatonal
            if rng.random() < cr['dead_end'] and len(avenue_lines) > 2:
                k = rng.randint(1, len(avenue_lines) - 1)
                hit = first_hit(line, [avenue_lines[k]])
                if hit:
                    line = cut(line, hit)
            last = first_hit(line, [avenue_lines[-1]])
            reaches_hill = line.length > (last or 1e9) + 5.0 and line.distance(hill_line) < 0.5
            if last and reaches_hill and rng.random() < cr['stairs']:
                street, alley = cut(line, last), LineString([line.interpolate(last).coords[0], line.coords[-1]])
                net.edge('street', street, cr['width'])
                net.edge('alley', alley, cr['alley'])
                net.entrances.append((*alley.coords[-1], 'hill'))
            else:
                net.edge('street', line, cr['width'])
            previous.append(line)
            cross_lines.append(line)
            s += rng.uniform(*cr['spacing']) * spec.get('spacing', 1.0)

    # --- avenidas y calle del cerro, cortadas por el río salvo las que tienen puente ---
    for k, line in enumerate(avenue_lines + [hill_line]):
        kind = 'avenue' if k < len(avenue_lines) else 'hill_street'
        width = widths[k] if k < len(avenue_lines) else hb['street']
        line = line.intersection(world)
        if kind == 'avenue' and k in rv['bridges']:
            for piece in lines(line):
                outside = lines(piece.difference(bank))
                for part in outside:
                    net.edge(kind, part, width)
                for part in lines(piece.intersection(bank)):
                    net.edge(kind, part, width, bridge=True)
        else:
            for part in lines(line.difference(bank)):
                if part.length > 8.0:
                    net.edge(kind, part, width)

    # --- la playa: la costa se recorre mar adentro en un distrito; la arena queda entre la costa y la nueva orilla ---
    bc = b.get('beach')
    beach = Polygon()
    if bc and bc.get('width', 0) > 0:
        south = bc.get('district', 0) == 0
        edge = s_r - clear if south else s_r + clear

        def beach_width(s):
            inside = (edge - s) if south else (s - edge)
            return bc['width'] * min(max(inside / bc['fade'], 0.0), 1.0)

        outer = coast.offset(lambda s: -beach_width(s))
        beach = Polygon(list(coast.points) + list(reversed(outer))).buffer(0).intersection(world).difference(land)
        beach = beach.difference(bank)
        # (where it has narrowed to nothing the offset leaves slivers of no area along the shore: left out, or the sand's
        # slope into the sea and the beach's spawners took them for beach)
        beach = unary_union([p for p in polygons(beach) if p.area > 1.0])
        land = land.union(beach)
        net.areas['land'] = land
        net.areas['sea'] = world.difference(land)
    net.areas['beach'] = beach

    # --- el muelle: del malecón mar adentro, recto, con una plataforma redonda al final ---
    pc = b.get('pier')
    if pc and not beach.is_empty:
        sa, sb = (s_lo, s_r - clear) if bc.get('district', 0) == 0 else (s_r + clear, s_hi)
        s_p = sa + pc.get('at', 0.5) * (sb - sa)
        base = coast.at(s_p) + coast.normal(s_p) * (b['promenade'] * 0.5)
        tip = coast.at(s_p) - coast.normal(s_p) * pc['length']
        deck = LineString([tuple(base), tuple(tip)]).buffer(pc['width'] / 2, cap_style=2)
        platform = Point(tuple(tip)).buffer(pc['platform'], 24)
        net.areas['pier'] = deck.union(platform).difference(land.difference(beach))
        net.pier = {'base': tuple(base), 'tip': tuple(tip), 'width': pc['width'], 'platform': pc['platform']}
        net.areas['sea'] = net.areas['sea'].difference(net.areas['pier'])

    # --- the seawall promenade: the esplanade goes into the land before the promenade is cut from it ---
    mc = b.get('malecon')
    net.malecon = None
    if mc:
        on_beach = (bc or {}).get('district', 0) if bc and bc.get('width', 0) > 0 else None
        district = mc.get('district', 1 if on_beach == 0 else 0)
        sa, sb = (s_lo - MARGIN, s_r - clear) if district == 0 else (s_r + clear, s_hi + MARGIN)
        inner = (max(sa, s_lo), min(sb, s_hi))
        s_m = inner[0] + mc.get('at', 0.5) * (inner[1] - inner[0])
        c, t, n = coast.at(s_m), coast.tangent(s_m), coast.normal(s_m)
        half, depth = mc.get('length', 30.0) / 2, mc.get('depth', 10.0)
        arc = [tuple(c + t * half * math.cos(u) - n * depth * math.sin(u)) for u in np.linspace(0.0, math.pi, 25)]
        esplanade = Polygon(arc + [tuple(c - t * half + n * 2.0), tuple(c + t * half + n * 2.0)]).buffer(0)
        land = land.union(esplanade)
        net.areas['land'] = land
        net.areas['sea'] = net.areas['sea'].difference(esplanade)
        ss = np.linspace(sa, sb, max(int((sb - sa) / 3.0), 2) + 1)
        band = Polygon([tuple(coast.at(s) - coast.normal(s) * 1.0) for s in ss] +
                       [tuple(coast.at(s) + coast.normal(s) * mc.get('walk', 8.5)) for s in reversed(ss)]).buffer(0)
        # its seaward edge steps down to the water ('steps' treads 'tread' meters deep, the outer lowest): areas of
        # their own, so the floor grid (floor.py) knows their heights
        count, tread = mc.get('steps', 3), mc.get('tread', 0.8)
        steps = []
        for k in range(count):
            ring = LineString(arc).buffer((k + 1) * tread, cap_style=2).difference(LineString(arc).buffer(k * tread, cap_style=2))
            steps.append((unary_union(polygons(ring.intersection(esplanade))), -STEP * (count - k)))
        # raised planters along the sea side of the walk, long and rounded, every 'beds' meters (not on the esplanade)
        beds = []
        own = random.Random(seed * 61 + 7)   # (its own: the town after it stays as it was without the malecon)
        s = sa + own.uniform(0.3, 1.0) * mc.get('beds', 14.0)
        while s < sb:
            if Point(tuple(coast.at(s))).distance(esplanade) > 5.0:
                spine = LineString([tuple(coast.at(s + k) + coast.normal(s + k) * mc.get('bed_offset', 3.0)) for k in (-2.2, -1.1, 0.0, 1.1, 2.2)])
                beds.append(spine.buffer(1.0, quad_segs=6))
            s += mc.get('beds', 14.0)
        net.malecon = {'district': district, 's': (sa, sb), 'center': tuple(c), 'tangent': tuple(t), 'normal': tuple(n),
                       'esplanade': esplanade, 'esplanade_floor': mc.get('esplanade_floor', 'malecon'), 'band': band.union(esplanade), 'arc': LineString(arc), 'steps': steps, 'tread': tread,
                       'beds': beds}
        for k, (ring, _) in enumerate(steps):
            net.areas[f'steps_{k}'] = ring

    # --- polígonos: calles, banquetas, malecón, peatonal ---
    asphalt = [e for e in net.edges if e['kind'] != 'alley']
    fillet = b.get('fillet', 0.0)
    # with round corners the streets end square (half their width on): two that end together (at the river) overlap into
    # one outer corner instead of leaving a notch between their cut ends
    road = unary_union([e['line'].buffer(e['width'] / 2, cap_style=3 if fillet > 0 else 2, join_style=1) for e in asphalt])
    if fillet > 0:
        # two streets that end together turn into each other: the hull of their last meters, one outer corner
        ends = []
        for e in asphalt:
            line = e['line']
            for at in (0.0, line.length):
                stub = substring(line, max(0.0, at - 4.0), min(line.length, at + 4.0))
                ends.append((Point(line.interpolate(at)), stub.buffer(e['width'] / 2, cap_style=3), e))
        for i, (p, cap, e) in enumerate(ends):
            for q, other, f in ends[i + 1:]:
                if f is not e and p.distance(q) < (e['width'] + f['width']) / 2 + 1.0:
                    road = road.union(cap.union(other).convex_hull)
    # where streets meet at an angle their corners are round ('fillet' meters): the inner corners filled (closing), the
    # outer ones turned (opening, at most half a lane so no street narrows); the sidewalks and curbs follow from it
    if fillet > 0:
        road = road.buffer(fillet, join_style=1).buffer(-fillet, join_style=1)
        turn = min(fillet, 2.0)
        road = road.buffer(-turn, join_style=1).buffer(turn, join_style=1)
    road = road.intersection(land)
    promenade = Polygon(list(coast.offset(0.0)) + list(reversed(coast.offset(b['promenade'])))).buffer(0)
    promenade = promenade.intersection(world)
    footbridge = promenade.intersection(bank) if rv.get('footbridge') else None
    promenade = promenade.difference(bank).intersection(land)
    if footbridge is not None:
        net.areas['footbridge'] = footbridge
        promenade = promenade.union(footbridge)
    alleys = unary_union([e['line'].buffer(e['width'] / 2, cap_style=2) for e in net.edges if e['kind'] == 'alley']) \
        if any(e['kind'] == 'alley' for e in net.edges) else Polygon()
    bridge_decks = unary_union([e['line'].buffer(e['width'] / 2 + sw + 0.6, cap_style=2) for e in net.edges if e.get('bridge')]) \
        if any(e.get('bridge') for e in net.edges) else Polygon()
    walk = road.buffer(sw, join_style=1).difference(road).difference(promenade)
    walk = walk.intersection(land).difference(bank.difference(bridge_decks)).difference(hill_area.difference(road.buffer(sw)))
    net.areas['road'] = road
    net.areas['sidewalk'] = walk
    if net.malecon:
        promenade = promenade.union(net.malecon['esplanade'].intersection(world))
        net.areas['malecon'] = net.malecon['band'].intersection(promenade)
    net.areas['promenade'] = promenade
    net.areas['alley'] = alleys.difference(road).difference(walk)
    net.areas['bridges'] = bridge_decks.intersection(bank)

    # --- manzanas: lo que queda de tierra entre todo eso ---
    taken = unary_union([road.buffer(sw, join_style=1), promenade, bank, hill_area, alleys, beach])
    district_mid = [(max(sa, s_lo) + min(sb, s_hi)) / 2 for sa, sb in districts]
    plazas_left = {0: b['plazas'], 1: b['plazas']}
    found = []
    verges = []
    for poly in polygons(land.intersection(world).difference(taken)):
        if poly.area < 80.0:
            verges.append(poly)   # too small for a block: a verge (grass), or a hole would show the sky
            continue
        c = poly.representative_point()
        s = coast.project((c.x, c.y))
        district = 0 if s < s_r else 1
        dist = coast.line.distance(c)
        band = sum(1 for k in range(1, len(offsets)) if dist > offsets[k](s))
        found.append({'poly': poly.simplify(0.05), 'district': district, 'band': band, 's': s})
    # plazas: en la primera franja, las manzanas más cerca del centro de cada distrito (y de buen tamaño, dentro de lo jugable)
    for district in (0, 1):
        candidates = [f for f in found if f['district'] == district and f['band'] == 0 and 500.0 < f['poly'].area < 3500.0
                      and play.contains(f['poly'].representative_point())]
        candidates.sort(key=lambda f: abs(f['s'] - district_mid[district]))
        for f in candidates[:plazas_left[district]]:
            f['kind'] = 'plaza'
    found.sort(key=lambda f: (f['district'], f['band'], f['s']))
    for i, f in enumerate(found):
        kind = f.get('kind', 'block')
        net.blocks.append({'id': f'{kind}_{i + 1:02d}', 'kind': kind, 'poly': f['poly'], 'district': f['district'],
                           'band': f['band']})

    net.areas['verges'] = unary_union(verges) if verges else Polygon()

    # --- entradas: donde las avenidas y la calle del cerro salen de lo jugable ---
    edge_of_play = play.exterior
    for e in net.edges:
        if e['kind'] == 'alley':
            continue
        hit = e['line'].intersection(edge_of_play)
        for p in ([hit] if hit.geom_type == 'Point' else [g for g in getattr(hit, 'geoms', []) if g.geom_type == 'Point']):
            # un poco adentro, sobre la calle
            inward = Point(p.x * 0.97, p.y * 0.97)
            q = e['line'].interpolate(e['line'].project(inward))
            net.entrances.append((q.x, q.y, 'street'))
    net.entrances = [(x, y, k) for x, y, k in net.entrances if play.buffer(1.0).contains(Point(x, y))]
    net.play = play
    net.coast = coast
    net.brief = b
    return net


# --- vista rápida ---

COLORS = {'beach': (230, 210, 150), 'pier': (200, 200, 200), 'sea': (70, 120, 160), 'land': (205, 190, 160), 'hill': (120, 140, 90), 'bank': (110, 150, 90),
          'river': (80, 130, 170), 'promenade': (215, 200, 150), 'road': (60, 60, 64), 'sidewalk': (170, 170, 168),
          'alley': (190, 160, 120), 'bridges': (120, 110, 100)}


def preview(net, path, scale=3.0, extra=None):
    """Una imagen cenital de la red (PIL): mar, tierra, cerro, río, calles, manzanas (las plazas en verde), entradas."""
    from PIL import Image, ImageDraw
    x0, y0, x1, y1 = net.play.bounds
    w, h = int((x1 - x0) * scale), int((y1 - y0) * scale)
    image = Image.new('RGB', (w, h), (0, 0, 0))
    draw = ImageDraw.Draw(image)

    def px(x, y):
        return ((x - x0) * scale, (y1 - y) * scale)

    def fill(geometry, color, outline=None):
        # con máscara: los agujeros (las manzanas dentro de las banquetas) dejan ver lo de abajo
        for p in polygons(geometry):
            mask = Image.new('L', (w, h), 0)
            m = ImageDraw.Draw(mask)
            m.polygon([px(*c) for c in p.exterior.coords], fill=255)
            for hole in p.interiors:
                m.polygon([px(*c) for c in hole.coords], fill=0)
            image.paste(color, (0, 0, w, h), mask)
            if outline:
                draw.line([px(*c) for c in p.exterior.coords], fill=outline, width=1)

    for name in ('sea', 'land', 'beach', 'hill', 'bank', 'river', 'promenade', 'pier'):
        fill(net.areas.get(name), COLORS[name])
    for blk in net.blocks:
        fill(blk['poly'], (120, 170, 100) if blk['kind'] == 'plaza' else (225, 205, 175), (150, 120, 90))
    for name in ('sidewalk', 'road', 'alley', 'bridges'):
        fill(net.areas.get(name), COLORS[name])
    for name, color in (extra or {}).items():
        fill(name, color)
    for e in net.edges:
        color = (250, 220, 60) if e['kind'] == 'avenue' else (240, 240, 240)
        if e['kind'] != 'alley':
            draw.line([px(*c) for c in e['line'].coords], fill=color, width=1)
    for x, y, k in net.entrances:
        cx, cy = px(x, y)
        draw.ellipse([cx - 5, cy - 5, cx + 5, cy + 5], fill=(230, 40, 40) if k == 'street' else (230, 120, 40))
    image.save(path)
    return path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--seed', type=int, default=1)
    parser.add_argument('--brief', type=Path, help='un JSON con el bloque coastal')
    parser.add_argument('--out', type=Path, default=Path('streetnet_preview.png'))
    args = parser.parse_args()
    brief = json.loads(args.brief.read_text(encoding='utf-8')).get('coastal') if args.brief else None
    net = generate(brief, args.seed)
    print(f'{len(net.edges)} ejes, {len(net.blocks)} manzanas ({sum(1 for b in net.blocks if b["kind"] == "plaza")} plazas), '
          f'{len(net.entrances)} entradas')
    print(preview(net, args.out))


if __name__ == '__main__':
    main()
