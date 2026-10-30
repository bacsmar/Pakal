"""Lo que rodea las calles: el mar con el muro del malecón, el río con sus orillas, el piso de las manzanas y el cerro.

El juego es plano (todo se camina a la altura 0); el agua está abajo y el cerro arriba, y los dos quedan fuera de lo que se
camina (la colisión los cierra, ver coastal.py). El cerro es una malla de alturas con ruido que sube desde su pie.
"""
import math
from pathlib import Path

import numpy as np
from shapely.geometry import LineString, Point, Polygon, box
from shapely.ops import unary_union

from geom import Noise1D, lines, polygons
from roadmesh import WATER_RIVER, WATER_SEA


def block_grounds(net, reach=0.6):
    """Each block's ground as a piece of one partition of the land: the block grown by 'reach' meters, less every other
    floor (the streets, the sidewalks, the promenade, the alleys, the plazas, the banks, the verges, the beach, the
    water). Its edge is then the very edge of the floor beside it: no strip left with no floor (the sky showed through
    there), no two floors laid over each other (they fight in the depth buffer). Sets b['ground_poly']."""
    a = net.areas
    others = [a.get(k) for k in ('road', 'sidewalk', 'promenade', 'alley', 'bank', 'verges', 'beach', 'sea', 'river', 'hill')]
    others = unary_union([g for g in others if g is not None and not g.is_empty]
                         + [k['poly'] for k in net.blocks if k['kind'] == 'plaza'])
    # a plaza's paving is the promenade's and the alleys' cobblestone: over them is the same floor; it stops at the rest
    streets = unary_union([g for g in (a.get(k) for k in ('road', 'sidewalk', 'bank', 'verges', 'beach', 'sea', 'river', 'hill'))
                           if g is not None and not g.is_empty])
    for b in net.blocks:
        if b['kind'] != 'plaza':
            b['ground_poly'] = b['poly'].buffer(reach, join_style=2).difference(others)
        else:
            b['ground_poly'] = b['poly'].buffer(reach, join_style=2).difference(streets)


def terrain_surfaces(net, mesh, clip, seed=1, hill_height=14.0):
    a = net.areas
    world = clip.buffer(40.0)
    # el mar: una lámina de agua, y el muro de piedra del malecón (de la orilla al agua)
    # el mar, por profundidad: una grilla cuyos vértices llevan qué tan bajo es el agua (1 en la orilla, 0 a 'deep' metros):
    # el shader lo pinta turquesa en lo bajo y azul hondo mar adentro (sin ese dato, el mar lejano es hondo)
    deep = 35.0
    shore = a['land'].boundary
    mesh.heightfield(a['sea'].intersection(world), lambda x, y: WATER_SEA, 'water', step=6.0,
                     uv1_of=lambda x, y: (max(0.0, 1.0 - shore.distance(Point(x, y)) / deep) ** 1.5, 0.0))
    land = a['land'].intersection(world)
    # la playa: arena casi a nivel, y una franja que baja al agua
    beach = a.get('beach', Polygon())
    if not beach.is_empty:
        mesh.flat(beach.intersection(world), -0.04, 'sand')
        sand_fringe(beach.intersection(world), land.difference(beach), mesh, seed)
        shore_waves(beach.intersection(world), a['sea'], mesh)
        shore_wet(beach.intersection(world), a['sea'], mesh)
        net.palapas = palapas(beach.intersection(clip), land.difference(beach), mesh, seed=seed)
        shore = beach.buffer(6.0).intersection(a['sea']).intersection(world)
        mesh.heightfield(shore, lambda x, y: max(-0.04 - 0.22 * beach.distance(Point(x, y)), WATER_SEA - 0.4), 'sand', step=2.0)
    pier_surfaces(net, mesh)
    malecon = getattr(net, 'malecon', None)
    lowest = min((h for _, h in malecon['steps']), default=0.0) if malecon else 0.0
    for poly in polygons(land):
        coords = list(poly.exterior.coords)
        for p, q in zip(coords, coords[1:]):
            mid = Point((p[0] + q[0]) / 2, (p[1] + q[1]) / 2)
            if a['sea'].distance(mid) < 0.5 and not beach.buffer(0.3).contains(mid):
                # the seawall: up to the lowest step where the esplanade steps down to the water
                top = lowest if malecon and malecon['arc'].distance(mid) < 0.5 else 0.0
                mesh.wall(p, q, WATER_SEA - 0.4, top, 'stone', facing=_toward(a['sea'], p, q))
    if malecon:
        malecon_surfaces(net, mesh, world)
    # el río: agua abajo, paredes de piedra en sus orillas (no bajo los puentes: ahí el agua sigue y el puente la cubre)
    river = a['river'].intersection(world)
    mesh.flat(river, WATER_RIVER, 'river')
    covered = a.get('bridges')
    for poly in polygons(river):
        for ring in [poly.exterior] + list(poly.interiors):
            coords = list(ring.coords)
            for p, q in zip(coords, coords[1:]):
                mid = Point((p[0] + q[0]) / 2, (p[1] + q[1]) / 2)
                if a['sea'].distance(mid) < 0.2:
                    continue
                mesh.wall(p, q, WATER_RIVER - 0.4, 0.0, 'stone', facing=_toward(river, p, q))
    # la orilla: pasto donde no hay calle, banqueta ni empedrado (a la misma altura: encimados pelean en el z-buffer)
    paved = unary_union([a['road'], a['sidewalk'], a['promenade'], a['alley']] + [k['poly'] for k in net.blocks if k['kind'] == 'plaza'])
    bank = a['bank'].intersection(world).difference(paved)
    # (what a bridge's road and sidewalks cover is already out of it with the paved; the rest of the bridge's strip on the
    # bank is grass too, or a hole showed the sky there)
    mesh.flat(bank, 0.0, 'grass')
    if a.get('verges') is not None and not a['verges'].is_empty:
        mesh.flat(a['verges'].intersection(world), 0.0, 'grass')   # the scraps between streets too small for a block
    # las manzanas: tierra (los edificios la tapan casi toda; lo que queda son patios y baldíos)
    for b in net.blocks:
        if b['kind'] != 'plaza':
            # what is paved over it (a fancy block's tiles, a forecourt, a parking lot) is left out: two floors a hair
            # apart fight in the depth buffer
            ground = b.get('ground_poly', b['poly'])
            earth = ground.difference(b['paved']) if b.get('paved') is not None else ground
            if not earth.is_empty:
                mesh.flat(earth.intersection(world), 0.0, b.get('ground', 'dirt'))
    # el cerro: sube desde su pie, con ruido; más alto tierra adentro
    hill = a['hill'].intersection(world)
    foot = net.hill_edge
    river_side = a['bank'].union(a['river'])
    noise = Noise1D(seed * 13 + 5, 25.0, octaves=3)

    def height(x, y):
        point = Point(x, y)
        d = foot.distance(point)
        # junto al río la loma baja a la orilla
        d = min(d, river_side.distance(point) * 1.5)
        rise = hill_height * (1.0 - math.exp(-d / 22.0)) + 0.06 * d
        return rise * (0.85 + 0.3 * noise(x * 0.7 + y * 0.3)) + 0.4 * noise(x * 3.1 - y * 1.7) * min(d / 4.0, 1.0)

    mesh.heightfield(hill, height, 'grass', step=4.0)
    # the rest of the land as far as the level's ground goes (the town's edge, past the closures, the avenues running out):
    # a floor too, at 0 where the far ground (backdrop) takes over, so the land is one piece to the horizon: nothing
    # floats over the sky's color. What every other floor covers is left out (exactly: no two floors over each other).
    taken = [a.get(k) for k in ('road', 'sidewalk', 'promenade', 'alley', 'bank', 'verges', 'beach', 'hill', 'river', 'sea',
                                'malecon', 'pier', 'bridges')]
    taken += [b.get('ground_poly', b['poly']) for b in net.blocks]
    taken += [ring for ring, _ in (malecon or {}).get('steps', [])]
    rest = land.difference(unary_union([g for g in taken if g is not None and not g.is_empty]))
    rest = unary_union([p for p in polygons(rest) if p.area > 0.01])
    if not rest.is_empty:
        mesh.flat(rest, 0.0, 'grass')
    return height


def sand_fringe(beach, other, mesh, seed=1, width=1.8, under=0.4):
    """Donde la playa llega al adoquín (o a lo que sea de tierra, no al agua): una franja de 'width' metros sobre eso, y
    'under' sobre la arena, con la arena desvanecida hacia afuera: no hay una orilla recta entre los dos."""
    texture = make_fringe_texture()
    if texture is None:
        return
    edge = beach.boundary.intersection(other.buffer(0.3))
    lines = [g for g in getattr(edge, 'geoms', [edge]) if g.geom_type == 'LineString' and g.length > 1.0]
    if edge.geom_type == 'MultiLineString' or edge.geom_type == 'GeometryCollection':
        from shapely.ops import linemerge
        merged = linemerge([g for g in edge.geoms if g.geom_type == 'LineString'])
        lines = [g for g in getattr(merged, 'geoms', [merged]) if g.length > 1.0]
    for line in lines:
        n = max(2, int(line.length / 1.0) + 1)
        pts = [line.interpolate(k * line.length / (n - 1)) for k in range(n)]
        inner, outer = [], []
        for k, p in enumerate(pts):
            q, r = pts[max(0, k - 1)], pts[min(n - 1, k + 1)]
            tx, ty = r.x - q.x, r.y - q.y
            length = math.hypot(tx, ty) or 1.0
            nx, ny = -ty / length, tx / length
            if beach.contains(Point(p.x + nx * 0.5, p.y + ny * 0.5)):
                nx, ny = -nx, -ny   # hacia afuera de la arena
            inner.append((p.x - nx * under, p.y - ny * under))
            outer.append((p.x + nx * width, p.y + ny * width))
        mesh.strip(inner, outer, 0.015, 'sand_fringe')


def shore_waves(beach, sea, mesh, out=5.0, up=1.2):
    """The waves on the beach: a band of foam along the waterline ('out' meters over the water, 'up' up the sand), just
    over the water; its crests roll in toward the sand (the material's scroll). Where the sand is higher than the band, the
    sand hides it: the line where they meet is the shore."""
    from roadmesh import WATER_SEA
    edge = beach.boundary.intersection(sea.buffer(0.3))
    from shapely.ops import linemerge
    parts = [g for g in getattr(edge, 'geoms', [edge]) if g.geom_type == 'LineString']
    if not parts:
        return
    merged = linemerge(parts) if len(parts) > 1 else parts[0]
    for line in [g for g in getattr(merged, 'geoms', [merged]) if g.length > 2.0]:
        n = max(2, int(line.length / 1.5) + 1)
        pts = [line.interpolate(k * line.length / (n - 1)) for k in range(n)]
        seaward, landward = [], []
        for k, p in enumerate(pts):
            q, r = pts[max(0, k - 1)], pts[min(n - 1, k + 1)]
            tx, ty = r.x - q.x, r.y - q.y
            length = math.hypot(tx, ty) or 1.0
            nx, ny = -ty / length, tx / length
            if beach.contains(Point(p.x + nx * 0.6, p.y + ny * 0.6)):
                nx, ny = -nx, -ny   # toward the water
            seaward.append((p.x + nx * out, p.y + ny * out))
            landward.append((p.x - nx * up, p.y - ny * up))
        # v = 0 out at sea, 1 on the sand: the scroll brings the crests in
        mesh.strip(seaward, landward, WATER_SEA + 0.03, 'shore_foam')


def shore_wet(beach, sea, mesh, under=4.2, up=2.6, slope=0.22):
    """The wet sand along the waterline: a band over the sand from 'under' meters out (under the water: the sand slopes
    down 'slope' a meter past the beach's edge, and meets the water about 4 m out) to 'up' meters up the dry sand,
    following the slope just over it. Its texture (make_wet_sand) has the foam line where the water meets the sand."""
    from roadmesh import WATER_SEA
    from shapely.ops import linemerge
    edge = beach.boundary.intersection(sea.buffer(0.3))
    parts = [g for g in getattr(edge, 'geoms', [edge]) if g.geom_type == 'LineString']
    if not parts:
        return
    merged = linemerge(parts) if len(parts) > 1 else parts[0]
    offsets = [under, under - 0.3, 3.2, 2.4, 1.6, 0.8, 0.0, -0.8, -1.6, -up]   # meters toward the water
    heights = [max(-0.04 - slope * max(d, 0.0), WATER_SEA - 0.4) + 0.02 for d in offsets]
    # the shallows: from where the slope meets the water out to sea, just over the water (the sand shows through, fading)
    water_line = (-0.04 - WATER_SEA) / slope
    shallows = [water_line, water_line + 1.5, water_line + 3.0, water_line + 5.0]
    for line in [g for g in getattr(merged, 'geoms', [merged]) if g.length > 2.0]:
        # the waterline's small wiggles out first, and each point's way from its neighbors 4.5 m off: offset by meters,
        # the normals of a wiggly line cross and the band folds into teeth
        line = line.simplify(0.4)
        n = max(2, int(line.length / 1.5) + 1)
        pts = [line.interpolate(k * line.length / (n - 1)) for k in range(n)]
        rows = [[] for _ in offsets]
        out_rows = [[] for _ in shallows]
        for k, p in enumerate(pts):
            q, r = pts[max(0, k - 3)], pts[min(n - 1, k + 3)]
            tx, ty = r.x - q.x, r.y - q.y
            length = math.hypot(tx, ty) or 1.0
            nx, ny = -ty / length, tx / length
            if beach.contains(Point(p.x + nx * 0.6, p.y + ny * 0.6)):
                nx, ny = -nx, -ny   # toward the water
            for row, d in zip(rows + out_rows, offsets + shallows):
                row.append((p.x + nx * d, p.y + ny * d))
        mesh.ribbon(rows, heights, 'wet_sand')
        mesh.ribbon(out_rows, [WATER_SEA + 0.012] * len(shallows), 'shallows')


def make_fringe_texture():
    """La textura de la franja (la arena de la playa con alfa): opaca en v = 0, desvanecida hacia v = 1 con un borde
    que ondula a lo largo y granos sueltos más allá. Se hace una vez, al lado de la de la arena."""
    from roadmesh import MATERIALS
    out = Path(MATERIALS['sand_fringe']['color'])
    if out.exists():
        return out
    source = Path(MATERIALS['sand']['color'])
    if not source.exists():
        import decals
        source.parent.mkdir(parents=True, exist_ok=True)
        decals.sand_texture().save(source)
    from PIL import Image
    rgb = np.asarray(Image.open(source).convert('RGB').resize((512, 512)), dtype=np.float32)
    h, w = rgb.shape[:2]
    v = np.linspace(0.0, 1.0, h)[:, None]
    u = np.linspace(0.0, 1.0, w, endpoint=False)[None, :]
    rng = np.random.default_rng(7)
    # el borde: senos que repiten a lo ancho (la textura se repite a lo largo de la franja sin costura)
    wave = 0.5 + 0.12 * np.sin(2 * np.pi * (2 * u + 0.13)) + 0.07 * np.sin(2 * np.pi * (5 * u + 0.41)) + 0.04 * np.sin(2 * np.pi * (11 * u + 0.7))
    fade = np.clip((wave - v) / 0.28 + 0.5, 0.0, 1.0)
    fade = fade * fade * (3.0 - 2.0 * fade)
    grains = (rng.random((h, w)) < 0.18 * np.clip(1.2 - v, 0.0, 1.0)).astype(np.float32) * np.clip(1.0 - fade, 0.0, 1.0) * 0.8
    alpha = np.clip(fade + grains, 0.0, 1.0)
    alpha[:6, :] = 1.0
    image = np.dstack([rgb, alpha[:, :] * 255.0]).astype(np.uint8)
    Image.fromarray(image, 'RGBA').save(out)
    return out


def _toward(region, p, q):
    """Un punto junto a la arista p-q del lado de 'region' (para que la pared mire hacia ella)."""
    mx, my = (p[0] + q[0]) / 2, (p[1] + q[1]) / 2
    length = math.hypot(q[0] - p[0], q[1] - p[1]) or 1.0
    nx, ny = (q[1] - p[1]) / length, -(q[0] - p[0]) / length
    if region.contains(Point(mx + nx * 0.3, my + ny * 0.3)):
        return (mx + nx, my + ny)
    return (mx - nx, my - ny)


def pier_surfaces(net, mesh):
    """El muelle: tablero, barandales, patas en V hasta el agua, la plataforma redonda y su escultura de vela (un mástil,
    cables y la vela)."""
    if not getattr(net, 'pier', None):
        return
    pier = net.pier
    deck = net.areas['pier']
    mesh.flat(deck, 0.05, 'concrete')
    mesh.ring_walls(deck, WATER_SEA, 0.05, 'white')
    base, tip, half = np.array(pier['base']), np.array(pier['tip']), pier['width'] / 2
    d = tip - base
    length = float(np.linalg.norm(d))
    d /= length
    n = np.array([-d[1], d[0]])
    radius = pier['platform']
    # sobre la arena el tablero es un andador abierto (se sube desde la playa); los barandales y las patas empiezan donde
    # el tablero sale al agua (ahí el mar es la pared: la colisión y el barandal coinciden)
    from shapely.geometry import Point as _Point
    sand = net.areas.get('beach')
    over_water = 0.0
    if sand is not None and not sand.is_empty:
        # the deck starts on the promenade and crosses the sand: the water begins past the last point over the sand
        near_sand = sand.buffer(0.5)
        k = 0.0
        while k < length:
            if near_sand.contains(_Point(*(base + d * k))):
                over_water = k + 0.5
            k += 0.5
    # barandales a los lados del tablero (hasta la plataforma) y alrededor de la plataforma
    reach = length - radius + 0.6
    for side in (-1, 1):
        a0 = base + d * over_water + n * side * (half - 0.05)
        a1 = base + d * reach + n * side * (half - 0.05)
        for h in (0.55, 1.05):
            mesh.beam((*a0, h), (*a1, h), 0.06, 'white')
        k = over_water
        while k <= reach:
            q = base + d * k + n * side * (half - 0.05)
            mesh.beam((*q, 0.05), (*q, 1.08), 0.07, 'white')
            k += 2.0
    ring = [tip + radius * 0.97 * np.array([math.cos(t), math.sin(t)]) for t in np.linspace(0, 2 * math.pi, 33)]
    entry = math.atan2(-d[1], -d[0])
    for p0, p1 in zip(ring, ring[1:]):
        mid = (p0 + p1) / 2 - tip
        if abs((math.atan2(mid[1], mid[0]) - entry + math.pi) % (2 * math.pi) - math.pi) < math.asin(min(half / radius, 1)) + 0.12:
            continue   # la entrada del tablero
        for h in (0.55, 1.05):
            mesh.beam((*p0, h), (*p1, h), 0.06, 'white')
        mesh.beam((*p0, 0.05), (*p0, 1.08), 0.07, 'white')
    # patas en V bajo el tablero, cada 6 m
    k = max(4.0, over_water + 2.0)
    while k < length:
        c = base + d * k
        for side in (-1, 1):
            mesh.beam((*(c + n * side * half), 0.0), (*c, WATER_SEA - 1.5), 0.35, 'white')
        k += 6.0
    # la escultura: un mástil, cables desde la punta a la orilla de la plataforma y una vela
    top = 19.0
    mesh.beam((*tip, 0.05), (*tip, top), 0.45, 'white')
    for t in np.linspace(0, 2 * math.pi, 7)[:-1] + 0.3:
        foot = tip + (radius - 0.8) * np.array([math.cos(t), math.sin(t)])
        mesh.beam((*tip, top - 0.5), (*foot, 0.1), 0.06, 'white')
    side = n * 1.0 + d * 0.3
    for k in (-1, 1):
        sail = tip + (side * k) * 3.2
        mesh.triangle((*tip, 2.5), (*tip, top - 2.0), (*sail, 5.0), 'sail')


def malecon_surfaces(net, mesh, clip):
    """A seawall promenade's own pieces (streetnet's 'malecon'): the esplanade's steps down to the sea (each tread and
    the riser over it), and a low parapet along the rest of the seawall."""
    m = net.malecon
    a = net.areas
    steps = m['steps']
    for k, (ring, height) in enumerate(steps):
        mesh.flat(ring.intersection(clip), height, 'concrete')
        # the riser up to the next tread (or to the esplanade), on the tread's landward edge
        above = steps[k + 1][1] if k + 1 < len(steps) else 0.0
        inner = ring.boundary.difference(m['arc'].buffer((k + 1) * m['tread'] - 0.05))
        for line in lines(inner.intersection(m['esplanade'].buffer(-0.05))):
            coords = list(line.coords)
            for p, q in zip(coords, coords[1:]):
                mesh.wall(p, q, height, above, 'concrete', facing=_toward(ring, q, p))
    # the parapet: along the seawall's top edge where it meets the sea, but where the steps go down
    shore = a['land'].boundary.intersection(a['sea'].buffer(0.3)).intersection(m['band'])
    shore = shore.difference(m['esplanade'].buffer(0.6))
    for line in lines(shore.intersection(clip)):
        if line.length < 1.5:
            continue
        inside = line.offset_curve(0.2) if a['land'].contains(line.offset_curve(0.2).interpolate(0.5, normalized=True)) \
            else line.offset_curve(-0.2)
        mesh.box_along(inside, 0.35, 0.0, 0.5, 'white')


def palapas(beach, inland, mesh, spacing=24.0, seed=1):
    """Palapas along the back of the sand (by the promenade): four posts and a palm-thatch cone, our own geometry (no
    pack has one). Returns their centres (for the spawners to keep clear)."""
    import random
    rng = random.Random(seed * 89 + 7)
    out = []
    edge = beach.boundary.intersection(inland.buffer(0.5))
    for line in lines(edge):
        d = rng.uniform(4.0, spacing)
        while d < line.length - 3.0:
            p = line.interpolate(d)
            q = beach.boundary.interpolate(beach.boundary.project(p))
            # 4.5 m into the sand from its back edge
            c = Point(p.x, p.y)
            inward = beach.representative_point()
            best = None
            for k in range(16):
                a = k * math.pi / 8
                t = Point(p.x + math.cos(a) * 4.5, p.y + math.sin(a) * 4.5)
                if beach.buffer(-2.8).contains(t):
                    best = t
                    break
            if best is not None:
                r, h = 2.6, 2.4
                for k in range(4):
                    a = k * math.pi / 2 + math.pi / 4
                    px, py = best.x + math.cos(a) * r * 0.7, best.y + math.sin(a) * r * 0.7
                    mesh.beam((px, py, 0.0), (px, py, h), 0.14, 'wood_post')
                ring = [(best.x + math.cos(a) * r * 1.15, best.y + math.sin(a) * r * 1.15) for a in np.linspace(0, 2 * math.pi, 11)]
                for a0, a1 in zip(ring, ring[1:]):
                    mesh.triangle((best.x, best.y, h + 1.6), (*a0, h - 0.15), (*a1, h - 0.15), 'thatch')
                out.append((best.x, best.y))
            d += spacing * rng.uniform(0.8, 1.2)
    return out


def backdrop(net, mesh, hill_height_of, seed=1, reach=1500.0):
    """Lo que se ve a lo lejos: el mar hasta el horizonte y la tierra que sube en montañas detrás del cerro, fuera de lo
    que cubren las mallas del nivel ('world'). Va en su propio modelo (un cuadro enorme: mesh con celda grande)."""
    a = net.areas
    world = a['land'].union(a['sea']).envelope
    x0, y0, x1, y1 = world.bounds
    big = box(x0 - reach, y0 - reach, x1 + reach, y1 + reach)
    coast = list(net.coast.points)
    coast = [(coast[0][0], y0 - reach - 10)] + coast + [(coast[-1][0], y1 + reach + 10)]
    far_land = Polygon(coast + [(x1 + reach + 10, y1 + reach + 10), (x1 + reach + 10, y0 - reach - 10)]).buffer(0)
    shoreline = LineString(coast)
    # it starts where the level's own ground ends (terrain_surfaces covers the playable area and 40 m around it): no
    # gap between them for a camera near the edge to see the sky through
    inner = net.play.buffer(40.0)
    far_sea = big.difference(far_land).difference(inner)
    mesh.flat(far_sea, WATER_SEA, 'water')
    noise = Noise1D(seed * 71 + 9, 160.0, octaves=3)
    near_reach = 110.0   # textured ground, with the hill going on, this far from the town
    start = 160.0        # the mountains rise from this far from the town (not from the coast: the town reaches inland)

    def height(x, y):
        p = Point(x, y)
        d = net.play.distance(p)
        h = max(hill_height_of(x, y), 0.0) if d < near_reach else 0.0
        # a sierra, not a wall: a gentle rise far off, ridges and saddles along it (two noises), never a flat top
        rise = max(0.0, d - start)
        base = 110.0 * (1.0 - math.exp(-rise / 260.0))
        ridges = 45.0 * noise(x * 0.35 + y * 0.9) + 20.0 * noise(x * 1.7 - y * 0.6)
        mountains = base + ridges * min(1.0, rise / 120.0)
        return max(h, max(0.0, mountains))

    land = far_land.intersection(big).difference(inner)
    near = land.intersection(net.play.buffer(near_reach))
    mesh.heightfield(near, height, 'grass', step=8.0)
    mesh.heightfield(land.difference(net.play.buffer(near_reach)), height, 'haze_near', step=40.0)

