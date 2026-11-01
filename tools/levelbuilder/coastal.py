"""Un pueblo costero de punta a punta: la red de calles por curvas (streetnet.py), sus mallas (roadmesh.py, terrain.py),
los edificios en los lados de cada manzana según su tipo (parcels.py), lo que trae cada lote (dress.py), las escenas
armadas (setpieces.py), los cables y el papel picado (overhead.py), lo que ponen los spawners (spawners.py,
presets/*.json), el desgaste (decals.py), la colisión del agua y del cerro, las entradas de los zombis y la misión.

Lo llama generate.py cuando el brief dice "layout": "coastal":

    {"layout": "coastal", "seed": 7,
     "theme": "<tema.json>",                       los modelos por rol (theme.py); sin tema, los de Quaternius que haya
     "presets": ["tools/levelbuilder/presets/coastal.json"],
     "coastal": {...},                             la red (streetnet.DEFAULTS)
     "buildings": {"0": {"shop": 4, "apartment": 1}, ...},   pesos de rol por franja (0 = junto al malecón), sin tipos
     "block_types": {"commercial": {"corner": {"shop": 1}, "mix": {"shop": 4, "apartment": 2}},
                     "residential": {"corner": {"shop": 1}, "corner_chance": 0.45, "mix": {"house": 5}},
                     "park": {"park": true}},      tipos de manzana: con qué se llena y qué va en las esquinas
     "block_bands": {"0": {"commercial": 1}, "2": {"residential": 3, "park": 0.5}},   qué tipos van en cada franja
     "bardas": {"height": 2.2, "thickness": 0.25, "material": "plaster"},   muros donde el frente no tiene edificio
     "background": {"near": 6, "far": 60, "spacing": 16},   edificios de fondo fuera de lo jugable
     "facade_decals": 0.6,                         probabilidad de desgaste en cada fachada
     "mission": {"mode": "survival", "kills": 80}}

Las mallas van a Assets/levelgen/<escena>/ (cuadros de 32 m) y entran como entidades de piso; la escena guarda la red
(network), los polígonos de las zonas y la colisión extra (outlines: cadenas cerradas que el jugador no cruza).
"""
import json
import math
import random
import re
import zlib
from pathlib import Path

import numpy as np
import shapely
import shapely.geometry
from shapely.geometry import Point, box
from shapely.ops import unary_union

import citykit_path  # noqa: F401
import citykit  # noqa: E402
import decals  # noqa: E402
import dress  # noqa: E402
import gasstation  # noqa: E402
import landmarks as landmark_pieces  # noqa: E402
import overhead  # noqa: E402
import parcels  # noqa: E402
import plazas as plazas_module  # noqa: E402
import setpieces  # noqa: E402
import roadmesh  # noqa: E402
import spawners  # noqa: E402
import streetnet  # noqa: E402
import terrain  # noqa: E402
import theme as theme_module  # noqa: E402
from geom import lines, polygons  # noqa: E402

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
OUT_ROOT = citykit.ASSETS_ROOT / 'levelgen'

BAND_ROLES = {
    0: {'shop': 4, 'apartment': 1},
    1: {'shop': 2, 'apartment': 2, 'house': 1},
    2: {'house': 3, 'apartment': 1, 'shop': 1},
}


def register_breakables(catalog, theme):
    """Lo que el tema dice que se rompe (por expresión de nombre -> plantilla del juego): a mission_survival.TEMPLATE_OF."""
    import mission_survival
    for spec in theme.get('breakable', []):
        for key, m in catalog['models'].items():
            if 'theme' in m['tags'] and re_search(spec['models'], key.rsplit('/', 1)[-1]):
                mission_survival.TEMPLATE_OF[key] = spec['template']
                if spec.get('broken'):
                    # its broken model, from the theme's own models ("broken": a name pattern)
                    burnt = next((k for k, bm in catalog['models'].items() if 'theme' in bm['tags']
                                  and re_search(spec['broken'], k.rsplit('/', 1)[-1])), None)
                    if burnt:
                        mission_survival.BROKEN_OF[key] = burnt
                if spec.get('round'):
                    mission_survival.ROUND.add(key)


def re_search(pattern, text):
    import re
    return re.search(pattern, text) is not None


def extend_catalog(scene, catalog):
    """El catálogo con lo que una escena costera usa además de Quaternius: su tema y sus mallas generadas."""
    data = scene.data or {}
    if data.get('theme'):
        theme = theme_module.load(REPO / data['theme'])
        theme_module.add_to_catalog(catalog, theme)
        register_breakables(catalog, theme)
    for key, info in data.get('generated', {}).items():
        catalog['models'][key] = {'pack': 'generated-mesh', 'path': info['path'], 'size': info['size'], 'low': info['low'],
                                  'high': info['high'], 'tags': ['ground', 'generated_mesh'], 'roles': ['ground']}
    return catalog


def models_for(catalog, role):
    themed = theme_module.by_role(catalog, role)
    if themed:
        return themed
    return sorted(k for k, m in catalog['models'].items() if role in m['roles'] and m['pack'] != 'generated-mesh')


def front_edge(model, entity):
    """La base de la fachada de un edificio puesto (dos puntos, la normal a la derecha mira a la calle)."""
    corners = citykit.footprint(model['low'], model['high'], entity['x'], entity['y'], entity['yaw'], entity.get('scale', 1.0))
    return corners[3], corners[2]


def poly_json(geometry):
    return [[[round(x, 2), round(y, 2)] for x, y in p.exterior.coords] for p in polygons(geometry)]


def background_ring(net, spec, catalog, models_for, seed, taken=None):
    """Edificios de fondo (rol background_building) en la tierra de afuera del área de juego, entre 'near' y 'far' metros de
    su borde, sin agua ni cerro: el pueblo sigue más allá (como el anillo de fondo de los mapas de demostración). No chocan."""
    models = models_for('background_building')
    if not models:
        return []
    rng = random.Random(seed * 41 + 3)
    a = net.areas
    ring = net.play.buffer(spec.get('far', 60.0)).difference(net.play.buffer(spec.get('near', 6.0)))
    land = ring.intersection(a['land'])
    for name in ('sea', 'river', 'hill', 'beach', 'road', 'sidewalk', 'promenade'):
        if a.get(name) is not None:
            land = land.difference(a[name].buffer(3.0))
    if taken is not None:
        land = land.difference(taken.buffer(4.0))
    c = net.play.centroid
    out = []
    for x, y in spawners.poisson(land, spec.get('spacing', 16.0), rng):
        yaw = parcels.yaw_facing(c.x - x, c.y - y)
        yaw = round(yaw / 90.0) * 90.0 + rng.uniform(-8, 8)
        out.append({'asset': rng.choice(models), 'x': round(x, 2), 'y': round(y, 2), 'yaw': round(yaw % 360, 2),
                    'scale': round(rng.uniform(*spec.get('scale', [0.8, 1.1])), 3), 'role': 'background_building',
                    'spawner': 'background'})
    return out


def sea_rocks(net, spec, catalog, models_for, seed):
    """Rocks standing out of the sea ("sea_rocks": {"count", "off": [near, far] meters from the shore, "scale": [lo, hi],
    "apart": meters between two, "role"}): the bay's islands (Los Arcos), seen from the promenade and in the pans to the
    horizon. Each a model of the role, scaled up, turned at random, its foot sunk a little in the water."""
    if not spec or not spec.get('count'):
        return []
    models = models_for(spec.get('role', 'sea_rock'))
    sea = net.areas.get('sea')
    if not models or sea is None or sea.is_empty:
        return []
    rng = random.Random(seed * 113 + 7)
    shore = net.areas['land'].boundary
    near, far = spec.get('off', [60.0, 170.0])
    band = sea.intersection(net.areas['land'].buffer(far)).difference(net.areas['land'].buffer(near))
    if band.is_empty:
        return []
    out, tries = [], 0
    x0, y0, x1, y1 = band.bounds
    while len(out) < spec['count'] and tries < 2000:
        tries += 1
        p = Point(rng.uniform(x0, x1), rng.uniform(y0, y1))
        if not band.contains(p) or any(math.dist((p.x, p.y), (o['x'], o['y'])) < spec.get('apart', 35.0) for o in out):
            continue
        key = rng.choice(models)
        scale = rng.uniform(*spec.get('scale', [3.0, 5.0]))
        m = catalog['models'][key]
        # (its base under the water: from the model's own foot, 15% of its height down)
        z = (-m['low'][1] - 0.15 * m['size'][2]) * scale
        out.append({'asset': key, 'x': round(p.x, 2), 'y': round(p.y, 2), 'z': round(z, 2),
                    'yaw': round(rng.uniform(0, 360), 1), 'scale': round(scale, 2), 'role': 'sea_rock', 'spawner': 'sea_rocks'})
    return out


def pier_boats(net, spec, catalog, models_for, seed):
    """Boats moored off the pier ("pier_boats": {"count", "role", "off": [near, far] meters from the deck's side,
    "draft": meters under the water, "crew": [lo, hi], "ship": {"role", "off", "draft"}}): each along the pier, a model of
    the role, its hull 'draft' under the water. 'crew': spots on board, [x, y, z, yaw] facing the pier, for the people a
    game puts there (the level's "crowd"). 'ship': one big one farther out, off the pier's tip, without crew."""
    pier = getattr(net, 'pier', None)
    sea = net.areas.get('sea')
    if not spec or not pier or sea is None or sea.is_empty:
        return []
    rng = random.Random(seed * 131 + 11)
    base, tip = np.array(pier['base']), np.array(pier['tip'])
    d = tip - base
    length = float(np.linalg.norm(d))
    d /= length
    n = np.array([-d[1], d[0]])
    keep_off = net.areas['pier'].buffer(0.8)

    def lie_along(key, x, y, scale, along):
        # the yaw that lays the model's long side along 'along' (its footprint's longer edge parallel to it)
        m = catalog['models'][key]
        yaw = math.degrees(math.atan2(along[1], along[0])) + rng.choice((0.0, 180.0))
        for turn in (0.0, 90.0):
            c = citykit.footprint(m['low'], m['high'], x, y, yaw + turn, scale)
            edges = [np.subtract(c[(i + 1) % 4], c[i]) for i in range(4)]
            longest = max(edges, key=lambda e: float(np.hypot(*e)))
            if abs(float(np.dot(longest, along))) / max(float(np.hypot(*longest)), 1e-6) > 0.7:
                return round((yaw + turn) % 360.0, 1), c
        return round(yaw % 360.0, 1), citykit.footprint(m['low'], m['high'], x, y, yaw, scale)

    def afloat(key, x, y, yaw, scale, draft, role):
        m = catalog['models'][key]
        z = roadmesh.WATER_SEA - m['low'][1] * scale - draft
        return {'asset': key, 'x': round(x, 2), 'y': round(y, 2), 'z': round(z, 3), 'yaw': yaw, 'scale': scale,
                'role': role, 'spawner': 'pier_boats', 'unbreakable': True}

    out, taken = [], []
    models = models_for(spec.get('role', 'boat'))
    near, far = spec.get('off', [3.0, 8.0])
    draft = spec.get('draft', 0.3)
    tries = 0
    while models and len(out) < spec.get('count', 0) and tries < 400:
        tries += 1
        key = rng.choice(models)
        m = catalog['models'][key]
        side = rng.choice((-1, 1))
        t = rng.uniform(0.35, 0.95) * (length - pier['platform'])
        off = pier['width'] / 2 + rng.uniform(near, far) + m['size'][0] / 2
        x, y = base + d * t + n * side * off
        yaw, corners = lie_along(key, float(x), float(y), 1.0, d)
        hull = shapely.geometry.Polygon(corners)
        if not sea.contains(hull) or hull.intersects(keep_off) or any(hull.distance(o) < 1.5 for o in taken):
            continue
        taken.append(hull)
        boat = afloat(key, float(x), float(y), yaw, 1.0, draft, 'boat')
        # its crew along its middle, facing the pier: as many as fit a meter and a half apart
        long_side = max(m['size'][0], m['size'][1])
        lo, hi = spec.get('crew', [1, 3])
        count = min(rng.randint(lo, hi), max(1, int(long_side * 0.4 / 1.5) + 1))
        # (the scene's yaw: at 0 facing south, counterclockwise; facing (sin, -cos))
        face = math.degrees(math.atan2(-side * n[0], side * n[1]))
        crew = []
        for k in range(count):
            u = 0.0 if count == 1 else (k / (count - 1) - 0.5) * long_side * 0.4
            cx, cy = np.array([x, y]) + d * u
            crew.append([round(float(cx), 2), round(float(cy), 2), round(roadmesh.WATER_SEA - draft + 0.25, 2), round(face, 1)])
        boat['crew'] = crew
        out.append(boat)
    ship = spec.get('ship')
    ships = models_for(ship.get('role', 'ship')) if ship else []
    if ships:
        key = rng.choice(ships)
        x, y = tip + d * ship.get('off', 120.0)
        yaw, corners = lie_along(key, float(x), float(y), 1.0, n)
        # (out past the play area the sea is drawn but not kept as an area: it only must keep off the land)
        if not shapely.geometry.Polygon(corners).buffer(10.0).intersects(net.areas['land']):
            out.append(afloat(key, float(x), float(y), yaw, 1.0, ship.get('draft', 1.5), 'ship'))
    return out


def fancy_furniture(yard, stores, rng, models):
    """A well-off plaza's furniture: bollards along its edge every 2.5 m, planters with a palm every 10 m a little in,
    benches between them facing in, lamps at the corners and between, climbing plants up the stores' walls, and in front
    of each store tables under parasols. 'models': the theme's "fancy_furniture", {role: [model ids]} (tall_palm,
    bollard, planter, palm, bench, street_light, ivy, table_set, parasol); a role it does not give is left out. Entities
    by model id, checked by the caller."""
    def key(role):
        found = models.get(role) or []
        if not found:
            return None
        return found[0] if len(found) == 1 else rng.choice(found)
    out = []
    ring = yard.exterior
    c = yard.centroid

    def along(offset, spacing, start=0.0):
        line = yard.buffer(-offset, join_style=2).exterior if offset > 0 else ring
        d = start
        while d < line.length:
            p, q = line.interpolate(d), line.interpolate(min(d + 0.5, line.length))
            yield p, math.atan2(q.y - p.y, q.x - p.x)
            d += spacing
    # tall palms all round, just inside the bollards
    for p, a in along(1.3, 7.0, 2.0):
        asset = key('tall_palm')
        out.append({'asset': asset, 'x': p.x, 'y': p.y, 'yaw': rng.uniform(0, 360), 'role': 'tall_palm', 'scale': rng.uniform(0.95, 1.15)})
    for p, a in along(0.4, 2.5):
        out.append({'asset': key('bollard'), 'x': p.x, 'y': p.y, 'yaw': 0.0, 'role': 'bollard'})
    for p, a in along(2.2, 10.0, 4.0):
        yaw = math.degrees(a) + 90.0
        out.append({'asset': key('planter'), 'x': p.x, 'y': p.y, 'yaw': yaw, 'role': 'planter'})
        asset = key('palm')
        out.append({'asset': asset, 'x': p.x, 'y': p.y, 'z': 0.6, 'yaw': rng.uniform(0, 360), 'role': 'palm', 'scale': 0.8})
    for p, a in along(3.4, 10.0, 9.0):
        out.append({'asset': key('bench'), 'x': p.x, 'y': p.y, 'yaw': parcels.yaw_facing(c.x - p.x, c.y - p.y), 'role': 'bench'})
    for p, a in along(1.2, 14.0, 1.0):
        out.append({'asset': key('street_light'), 'x': p.x, 'y': p.y, 'yaw': parcels.yaw_facing(c.x - p.x, c.y - p.y),
                    'role': 'street_light'})
    # climbing plants up the stores' walls, every few meters round each
    for st in stores:
        ring = st['poly'].exterior
        sc = st['poly'].centroid
        d = rng.uniform(1.0, 3.0)
        while d < ring.length:
            p = ring.interpolate(d)
            out.append({'asset': key('ivy'), 'x': p.x, 'y': p.y, 'yaw': parcels.yaw_facing(p.x - sc.x, p.y - sc.y), 'role': 'ivy'})
            d += rng.uniform(3.5, 6.0)
    for st in stores:
        # where the store's front looks (fill_block's 'front': its street, or its yard when turned in)
        n = st.get('front') or (math.sin(math.radians(st['yaw'])), -math.cos(math.radians(st['yaw'])))
        t = (-n[1], n[0])
        sc = st['poly'].centroid
        reach = max(st['poly'].bounds[2] - st['poly'].bounds[0], st['poly'].bounds[3] - st['poly'].bounds[1]) / 2
        # on the store's side that is in the plaza (its front, or its back when the front is on the street)
        if not yard.contains(Point(sc.x + n[0] * (reach + 2.5), sc.y + n[1] * (reach + 2.5))):
            n = (-n[0], -n[1])
        for k in (-1, 0, 1):
            x = sc.x + n[0] * (reach + 2.5) + t[0] * k * 3.2
            y = sc.y + n[1] * (reach + 2.5) + t[1] * k * 3.2
            out.append({'asset': key('table_set'), 'x': x, 'y': y, 'yaw': rng.uniform(0, 90), 'role': 'table_set'})
            out.append({'asset': key('parasol'), 'x': x + 0.01, 'y': y, 'yaw': 0.0, 'role': 'beach_umbrella'})
    return [dict(e, x=round(e['x'], 3), y=round(e['y'], 3), yaw=round(e['yaw'] % 360.0, 2)) for e in out if e['asset']]


def generate(scene, catalog, brief):
    seed = int(brief.get('seed', 1))
    rng = random.Random(seed)
    data = scene.data
    theme = {}
    if brief.get('theme'):
        theme_path = Path(brief['theme'])
        theme = theme_module.load(REPO / theme_path if not theme_path.is_absolute() else theme_path)
        missing = theme_module.missing(theme)
        if missing:
            theme_module.extract(theme)
        theme_module.add_to_catalog(catalog, theme)
        register_breakables(catalog, theme)
        theme_module.apply_materials(theme, roadmesh.MATERIALS)
        theme_module.material_tints(theme)
        data['theme'] = Path(theme['path']).relative_to(REPO).as_posix()

    net = streetnet.generate(brief.get('coastal'), seed)
    x0, y0, x1, y1 = net.play.bounds
    # la playa entera y el muelle son jugables, hasta el agua y hasta la punta: el borde se recorre al mar lo que salgan
    # del rectángulo (el mar alrededor sigue siendo colisión: nadie se sale de la arena ni del muelle)
    for name in ('beach', 'pier'):
        shore = net.areas.get(name)
        if shore is not None and not shore.is_empty:
            part = shore.intersection(box(x0 - 300.0, y0, x1, y1))
            if not part.is_empty:
                x0 = min(x0, round(part.bounds[0] - 2.0, 2))
    data['bounds'] = [x0, y0, x1, y1]
    data['layout'] = 'coastal'
    data['network'] = net.to_json()
    zones = data['zones'] = {}
    a = net.areas

    # --- edificios ---
    occupancy = spawners.Occupancy()
    entities = []
    weights = {int(k): v for k, v in brief.get('buildings', {}).items()} or BAND_ROLES
    plazas = [b for b in net.blocks if b['kind'] == 'plaza']
    blocks = [b for b in net.blocks if b['kind'] != 'plaza']
    landmark_block = {}
    for i, plaza in enumerate(plazas):
        c = plaza['poly'].centroid
        near = min((b for b in blocks if b['id'] not in landmark_block), key=lambda b: b['poly'].distance(c), default=None)
        if near is not None:
            landmark_block[near['id']] = (c.x, c.y, i)
    yards = []
    parks = []
    landmarks = models_for(catalog, 'church') + models_for(catalog, 'city_hall')
    # los tipos de manzana (comercial, residencial, parque...) y cuáles van en cada franja; sin ellos, un tipo por franja
    # con los pesos de rol de "buildings"
    types = brief.get('block_types') or {f'band{k}': {'mix': v} for k, v in weights.items()}
    bands = {int(k): v for k, v in brief.get('block_bands', {}).items()} or {k: {f'band{k}': 1} for k in weights}
    trng = random.Random(seed * 13 + 5)

    def role_choices(mix):
        out = []
        for role, weight in mix.items():
            found = models_for(catalog, role)
            out += [(m, weight / max(len(found), 1)) for m in found]
        return out

    town_reach = net.play.buffer(32.0)
    # the well-off blocks (the brief's "wealthy": the band facing the sea, its luxury hotels; the hillside with its view):
    # their buildings kept up (no pack of damaged ones), their streets clean (see the spawners and the facades below)
    wealthy = brief.get('wealthy') or {}
    wealthy_ids = {blk['id'] for blk in blocks
                   if any(all(blk.get(k) == v for k, v in sel.items()) for sel in wealthy.get('blocks', []))}
    shabby = set(wealthy.get('exclude_packs', []))
    paint_state = {'rng': random.Random(seed * 29 + 11)}   # its own: the paint does not change the blocks' draws
    paint_spec = brief.get('paint') or {}
    last_paint = [None]   # (family, color) of the building before, along the block

    def saturate(color, k):
        mean = sum(color) / 3.0
        return [min(1.0, max(0.0, mean + (c - mean) * k)) for c in color]

    def paint_for(rich, role, corner):
        """Mexico is colorful (the brief's "paint", the designer's palette): walls of a family drawn by weight (whites
        and creams, warm pastels, strong colors; a corner shop strong), never the color or the family of the neighbor
        before; a roof of waterproofing red, white or concrete. The well-off: whites and creams under terracotta tile."""
        walls = paint_spec.get('walls')
        if not walls or role not in paint_spec.get('roles', []):
            return None
        families = sorted(walls)
        roof = None
        if rich and paint_spec.get('wealthy'):
            forced = paint_spec['wealthy'].get('walls')
            roof = paint_spec['wealthy'].get('roof')
        else:
            forced = paint_spec.get('corners') if corner else None
        for _ in range(12):
            family = forced or paint_state['rng'].choices(families, [walls[f].get('weight', 1.0) for f in families])[0]
            name = paint_state['rng'].choice(sorted(walls[family]['colors']))
            before = last_paint[0]
            if before is None or (name != before[1] and (forced or family != before[0] or family == 'base')):
                break
        last_paint[0] = (family, name)
        wall = walls[family]['colors'][name]
        if family == 'strong':
            wall = saturate(wall, paint_spec.get('strong_saturation', 1.0))
        if roof is None:
            roofs = paint_spec.get('roofs') or {'none': [1.0, [0.5, 0.5, 0.5]]}
            kinds = sorted(roofs)
            roof = roofs[paint_state['rng'].choices(kinds, [roofs[k][0] for k in kinds])[0]][1]
        return {'name': f'{family}/{name}', 'wall': wall, 'roof': roof, 'atlases': paint_spec.get('atlases'),
                'models': paint_spec.get('building_models')}

    def kept_up(choices):
        kept = [(m, w) for m, w in choices if not (m.count('/') >= 2 and m.split('/')[1] in shabby)]
        return kept or choices

    # gas stations (gasstation.py): the brief's blocks, their forecourt open toward the game's camera
    gas = brief.get('gas_stations') or {}
    gas_plans = {}
    strip_lots = {}
    fancy_yards = []
    fancy_used = set()

    def pick_models(name):
        """A model of the catalog by its name's end (its file's stem), or None."""
        if not name:
            return None
        return next((k for k in catalog['models'] if k.rsplit('/', 1)[-1] == name), None)
    # One block made again on its own (the brief's "block_overrides": {block id: {"seed", "type", "mix"}}): the block
    # first draws as it would have (its buildings and paint thrown away), so the town's streams go on the same and every
    # other block keeps its buildings; then it is filled with its own seed, type and mix
    overrides = brief.get('block_overrides') or {}

    def shadow_fill(blk, kind):
        """The draws an ordinary block makes (parcels.fill_block and the paint of what it placed), results unused."""
        if kind.get('park') and blk['id'] not in landmark_block:
            return
        choices = role_choices(kind.get('mix', {}))
        corner = role_choices(kind['corner']) if kind.get('corner') else None
        if blk['id'] in wealthy_ids:
            choices = kept_up(choices)
            corner = kept_up(corner) if corner else corner
        first, prefer = None, None
        if blk['id'] in landmark_block and landmarks:
            px, py, k = landmark_block[blk['id']]
            first, prefer = [(landmarks[k % len(landmarks)], 'landmark')], (px, py)
        placed, _ = parcels.fill_block(blk['poly'], catalog, choices, rng, first=first, prefer=prefer, corner=corner,
                                       corner_chance=kind.get('corner_chance', 1.0), camera=brief.get('camera'),
                                       passage=kind.get('passage', brief.get('passage')), passages=[])
        for p in [q for q in placed if town_reach.contains(q['poly'].centroid)]:
            m = catalog['models'][p['asset']]
            role = p.get('role') or next((r for r in m['roles'] if r in ('shop', 'apartment', 'house', 'church', 'landmark')), m['roles'][0])
            paint_for(blk['id'] in wealthy_ids, role, p.get('corner', False))
        last_paint[0] = None

    # a way in to each block's yard (its vacant lot), the brief's "passage" (meters), free of bardas and clutter
    passages = []
    town_rng, town_paint = rng, paint_state['rng']
    for blk in blocks:
        band = min(blk['band'], max(bands))
        pick = bands.get(band) or next(iter(bands.values()))
        kind_name = trng.choices(list(pick), list(pick.values()))[0]
        # the brief may set a block's type ("block_types_by_id": a park put where the designer wants it); the draw above
        # stays, so the other blocks keep theirs
        kind_name = (brief.get('block_types_by_id') or {}).get(blk['id'], kind_name)
        kind = types[kind_name]
        rng, paint_state['rng'] = town_rng, town_paint
        own = overrides.get(blk['id'])
        if own:
            special = blk['id'] in (brief.get('fancy_blocks') or {}).get('blocks', [])                 or blk['id'] in (brief.get('strip_malls') or {}).get('blocks', []) or blk['id'] in gas.get('blocks', [])
            if special:
                raise SystemExit(f"block_overrides: {blk['id']} is a gas station, strip mall or shopping block (not made by type)")
            shadow_fill(blk, kind)
            kind_name = own.get('type', kind_name)
            if kind_name not in types:
                raise SystemExit(f"block_overrides: {blk['id']}: no block type {kind_name!r} (the brief's block_types: {sorted(types)})")
            kind = dict(types[kind_name], **({'mix': own['mix']} if own.get('mix') else {}))
            block_seed = int(own.get('seed', 1))
            rng, paint_state['rng'] = random.Random(block_seed * 7919 + 17), random.Random(block_seed * 104729 + 3)
        zones[blk['id']] = {'poly': poly_json(blk['poly']), 'rect': [round(v, 2) for v in blk['poly'].bounds],
                            'kind': 'park' if kind.get('park') and blk['id'] not in landmark_block else 'block',
                            'type': kind_name}
        if blk['id'] in wealthy_ids:
            zones[blk['id']]['wealthy'] = True
            blk['ground'] = 'concrete'   # (a well-off block's yards are paved, not earth)
        if blk['id'] in (brief.get('fancy_blocks') or {}).get('blocks', []):
            # a well-off block of whole stores (a pack's demo block, composed by the theme's tools), facing the camera,
            # its yard paved in slabs
            # each store once in the town (the next block gets the ones not yet placed), the first ones the biggest
            stores = [c for c in role_choices({'fancy_store': 1}) if c[0] not in fancy_used] or role_choices({'fancy_store': 1})
            placed, yard = parcels.fill_block(blk['poly'], catalog, stores, rng,
                                              camera=brief.get('camera'), gap=(1.5, 3.0), unique=True)
            blk['paved'] = blk['poly']   # (tiled whole: no earth under the tiles)
            fancy_used.update(q['asset'] for q in placed)
            for q in placed:
                entities.append({'asset': q['asset'], 'x': q['x'], 'y': q['y'], 'yaw': q['yaw'], 'scale': 1.0,
                                 'role': 'fancy_store', 'zone': blk['id'], 'corner': False})
                occupancy.add(q['poly'])
            zones[blk['id']]['wealthy'] = True
            zones[blk['id']]['kind'] = 'gas_station'   # (open: no barda round it)
            wealthy_ids.add(blk['id'])
            fancy_yards.append((blk['id'], blk['poly'].buffer(-0.2, join_style=2), placed))
            continue
        if blk['id'] in (brief.get('strip_malls') or {}).get('blocks', []):
            # the anchor + L block: an L market, a diner on the corner, a parking lot
            sm = brief['strip_malls']
            gp = gasstation.plan(blk['poly'], sm.get('camera', brief.get('camera', [-1.0, -1.0])), sm.get('depth', 9.0))
            zones[blk['id']]['kind'] = 'gas_station'   # (no barda across its open front)
            zones[blk['id']]['type'] = 'strip_mall'
            placed, lot = gasstation.strip_mall(gp, catalog, role_choices({'shop': 1}), rng,
                                                mall=pick_models(sm.get('mall')), diner=pick_models(sm.get('diner')))
            for q in placed:
                entities.append({'asset': q['asset'], 'x': q['x'], 'y': q['y'], 'yaw': q['yaw'], 'scale': 1.0, 'role': 'shop',
                                 'zone': blk['id'], 'corner': False})
                paint = paint_for(False, 'shop', False)
                if paint:
                    entities[-1]['paint'] = paint
                occupancy.add(q['poly'])
            strip_lots[blk['id']] = lot
            blk['paved'] = lot
            continue
        if blk['id'] in gas.get('blocks', []):
            gp = gasstation.plan(blk['poly'], gas.get('camera', brief.get('camera', [-1.0, -1.0])), gas.get('depth', 8.0))
            gas_plans[blk['id']] = gp
            blk['paved'] = gp['forecourt']
            zones[blk['id']]['kind'] = 'gas_station'   # (no barda across its open front)
            zones[blk['id']]['type'] = 'plazuela_pemex'
            # the store (the e-Ozzo): one kept up (the theme's "store_pristine", when it has one), whatever the block
            pristine = models_for(catalog, gas.get('store_role', 'store_pristine'))
            placed = gasstation.shops(gp, catalog, role_choices({'shop': 1}), rng, store=pristine or None)
            for k, q in enumerate(placed):
                entities.append({'asset': q['asset'], 'x': q['x'], 'y': q['y'], 'yaw': q['yaw'], 'scale': 1.0, 'role': 'shop',
                                 'zone': blk['id'], 'corner': q.get('corner', False),
                                 **({'word': gas.get('store', 'E-OZZO'), 'pristine': bool(pristine)} if k == 0 else {})})
                paint = paint_for(True, 'shop', False)   # (a plaza's shops in one look: whites and creams)
                if paint:
                    entities[-1]['paint'] = paint
                occupancy.add(q['poly'])
            continue
        if kind.get('park') and blk['id'] not in landmark_block:
            parks.append(blk['poly'])
            blk['ground'] = 'grass'   # (a park's ground is lawn, not earth)
            continue
        choices = role_choices(kind.get('mix', {}))
        corner = role_choices(kind['corner']) if kind.get('corner') else None
        if blk['id'] in wealthy_ids:
            choices = kept_up(choices)
            corner = kept_up(corner) if corner else corner
        first, prefer = None, None
        if blk['id'] in landmark_block and landmarks:
            px, py, k = landmark_block[blk['id']]
            first, prefer = [(landmarks[k % len(landmarks)], 'landmark')], (px, py)
        # with a fixed camera (the brief's "camera"), the sides that face away turn their fronts to the block's yard
        placed, yard = parcels.fill_block(blk['poly'], catalog, choices, rng, first=first, prefer=prefer, corner=corner,
                                          corner_chance=kind.get('corner_chance', 1.0), camera=brief.get('camera'),
                                          passage=kind.get('passage', brief.get('passage')), passages=passages)
        # only as far as the level's ground reaches (terrain: 40 m around the playable area); beyond, the background ring
        placed = [q for q in placed if town_reach.contains(q['poly'].centroid)]
        yards.append(yard)
        for p in placed:
            m = catalog['models'][p['asset']]
            role = p.get('role') or next((r for r in m['roles'] if r in ('shop', 'apartment', 'house', 'church', 'landmark')), m['roles'][0])
            entities.append({'asset': p['asset'], 'x': p['x'], 'y': p['y'], 'yaw': p['yaw'], 'scale': 1.0, 'role': role,
                             'zone': blk['id'], 'corner': p.get('corner', False)})
            paint = paint_for(blk['id'] in wealthy_ids, role, p.get('corner', False))
            if paint:
                entities[-1]['paint'] = paint
            occupancy.add(p['poly'])
        last_paint[0] = None   # the next block starts its own run of neighbors
    rng, paint_state['rng'] = town_rng, town_paint
    for way in passages:
        occupancy.add(way)   # (the way in stays clear: no tree, planter or clutter in it)
    buildings = unary_union([parcels.footprint_poly(catalog['models'][e['asset']], e['x'], e['y'], e['yaw']) for e in entities]) \
        if entities else None

    # --- bardas: donde el frente de una manzana no tiene edificio, un muro (la calle sigue siendo pared, como en México) ---
    barda = brief.get('bardas', {})
    barda_lines = []
    if barda.get('height', 2.2) > 0 and buildings is not None:
        thick = barda.get('thickness', 0.25)
        for blk in blocks:
            if zones[blk['id']]['kind'] != 'block':
                continue
            frontage = blk['poly'].buffer(-(0.2 + thick / 2), join_style=2)
            open_ways = unary_union(passages).buffer(0.2) if passages else None
            for ring in polygons(frontage):
                edge = ring.exterior.difference(buildings.buffer(0.35, join_style=2))
                if open_ways is not None:
                    edge = edge.difference(open_ways)
                for piece in lines(edge):
                    if piece.length >= barda.get('min', 2.0):
                        barda_lines.append(piece)
                        occupancy.add(piece.buffer(thick / 2 + 0.1, cap_style=2))

    # --- plazas (plazas.py): andadores al centro, jardineras entre ellos, y al centro el kiosco o la fuente ---
    plaza_kinds = brief.get('plazas', ['civic', 'children'])
    plaza_plans = []
    prng = random.Random(seed * 17 + 1)
    for i, plaza in enumerate(plazas):
        zones[plaza['id']] = {'poly': poly_json(plaza['poly']), 'rect': [round(v, 2) for v in plaza['poly'].bounds], 'kind': 'plaza'}
        plan = plazas_module.layout(plaza['poly'], plaza_kinds[min(i, len(plaza_kinds) - 1)], prng, brief.get('plaza'))
        plaza_plans.append(plan)
        zones[plaza['id']]['type'] = plan['kind']
        # al centro el kiosco (cívica, de baile) o la fuente; el brief puede pedir una para todas ('plaza_centerpiece')
        role = brief.get('plaza_centerpiece') or ('kiosk' if plan['kind'] in ('civic', 'dance') else 'fountain')
        scale = 2.2 if role == 'kiosk' else 1.8
        found = models_for(catalog, role)
        if found:
            c = plaza['poly'].centroid
            m = catalog['models'][found[0]]
            foot = parcels.footprint_poly(m, c.x, c.y, 0.0, scale)
            entities.append({'asset': found[0], 'x': round(c.x, 3), 'y': round(c.y, 3), 'yaw': 0.0, 'scale': scale,
                             'role': role, 'zone': plaza['id']})
            occupancy.add(foot.buffer(1.5))
    zones['promenade'] = {'poly': poly_json(a['promenade']), 'rect': [round(v, 2) for v in a['promenade'].bounds], 'kind': 'plaza'}

    # the seawall promenade's planters: drawn, walled and planted as the plazas' beds (white, a little higher)
    if getattr(net, 'malecon', None) and net.malecon['beds']:
        plaza_plans.append({'kind': 'malecon', 'beds': [b for b in net.malecon['beds'] if not b.intersection(net.play).is_empty],
                            'pits': [], 'spec': {'soil': 0.2, 'curb': 0.5, 'material': 'white'}})

    # --- regiones para los spawners ---
    clip = net.play.buffer(40.0)   # (as far as the terrain and the backdrop's start: no gap without a floor)
    plaza_geo = unary_union([p['poly'] for p in plazas]) if plazas else None
    regions = {
        'road': a['road'].intersection(clip), 'sidewalk': a['sidewalk'].intersection(clip),
        'promenade': a['promenade'].intersection(net.play), 'plaza': plaza_geo, 'alley': a['alley'],
        # the river walk without what is paved across it (a bridge's road and sidewalks): plants on the earth only
        'bank': a['bank'].intersection(clip).difference(unary_union([g for g in (a['road'], a['sidewalk'], a.get('bridges'),
                                                                                 a.get('footbridge')) if g is not None]).buffer(0.5)),
        'hill': a['hill'].intersection(net.play.buffer(25.0)),
        'yard': unary_union(yards).buffer(-0.4) if yards else None, 'sea': a['sea'], 'river': a['river'],
        'buildings': buildings, 'park': unary_union(parks) if parks else None,
        'garden': unary_union([b for plan in plaza_plans for b in plan['beds']]) if plaza_plans else None,
        'playground': unary_union([pit for plan in plaza_plans for pit in plan['pits']]) if plaza_plans else None,
        'beach': a.get('beach').intersection(box(*data['bounds'])) if a.get('beach') is not None else None,
        'pier': a.get('pier'),
        # la orilla, con la tierra a la izquierda: un desplazamiento positivo cae en el malecón
        'coastline': shapely.geometry.LineString(net.coast.points[::-1]).intersection(net.play),
    }
    if getattr(net, 'malecon', None):
        # the stretch of shore along the seawall promenade (its own rows: bollards, statues, the seawall's rocks)
        regions['malecon_coastline'] = regions['coastline'].intersection(net.malecon['band'].buffer(1.0))             .difference(net.malecon['esplanade'].buffer(3.0))
        regions['malecon'] = a['malecon'].intersection(net.play)
    regions['outside'] = clip.difference(net.play)   # past the town's edge: seen in pans, never walked
    regions['walk'] = unary_union([g for g in (regions['sidewalk'], regions['promenade'], plaza_geo, a['alley'], regions['beach'],
                                               regions['pier'], regions['park']) if g is not None])
    # the well-off blocks and the streets in front of them ('reach' meters out)
    rich = [blk['poly'] for blk in blocks if blk['id'] in wealthy_ids]
    regions['wealthy'] = unary_union(rich).buffer(wealthy.get('reach', 10.0)) if rich else None

    # --- mallas: calles, agua, cerro ---
    mesh = roadmesh.Mesh()
    terrain.block_grounds(net)   # (one partition of the land: the blocks' and plazas' floors meet the streets' exactly)
    roadmesh.road_surfaces(net, mesh, clip)
    for piece in barda_lines:
        mesh.box_along(piece, barda.get('thickness', 0.25), 0.0, barda.get('height', 2.2), barda.get('material', 'plaster'))
    for plan in plaza_plans:
        plazas_module.draw(plan, mesh)
    hill_height = terrain.terrain_surfaces(net, mesh, net.play, seed)
    far = roadmesh.Mesh(cell=1e5)   # el fondo: un solo modelo
    terrain.backdrop(net, far, hill_height, seed)
    road_geo = regions['road']

    def decal_height(x, y):
        return roadmesh.ROAD_Y + 0.012 if road_geo.contains(Point(x, y)) else 0.012

    # --- los presets: spawners, recetas de lote, escenas armadas, lo que cuelga sobre las calles ---
    presets = [json.loads((REPO / p).read_text(encoding='utf-8'))
               for p in brief.get('presets', ['tools/levelbuilder/presets/coastal.json'])]
    lots, pieces, overhead_spec = {}, [], {}
    for preset in presets:
        lots.update(preset.get('lots', {}))
        pieces += preset.get('setpieces', [])
        overhead_spec.update(preset.get('overhead', {}))
    # the brief's own words for the roof signs ("roof_words": {"Chicken": "POLLO SABRO"}: a model's name part -> its word)
    dress.ROOF_WORDS.update(brief.get('roof_words', {}))
    # the brief's own shop words over the preset's ("words": ["POLLO SABRO", {"word": ..., "max_per_town": 3}])
    if brief.get('words'):
        lots = json.loads(json.dumps(lots))
        for recipe in lots.values():
            for item in recipe:
                if item.get('at') == 'word':
                    item['words'] = list(item.get('words', [])) + list(brief['words'])
    report = {}
    pick = lambda role: models_for(catalog, role)  # noqa: E731

    def add_placed(got, zone=None):
        for p in got:
            entities.append({'asset': p['asset'], 'x': p['x'], 'y': p['y'], 'yaw': p['yaw'], 'scale': p.get('scale', 1.0),
                             'role': p['role'], 'zone': p.get('zone', zone), 'z': p.get('z', 0.0), 'spawner': p.get('spawner'),
                             'tilt': p.get('tilt', 0.0), **({'tint': p['tint']} if p.get('tint') else {}),
                             **({'crew': p['crew']} if p.get('crew') else {}),
                             **({'unbreakable': True} if p.get('unbreakable') else {})})

    # cada lote con lo suyo (dress.py), antes que los spawners: la banqueta de una tienda es de la tienda
    buildings_placed = [e for e in entities if e['role'] in ('shop', 'apartment', 'house')]
    panels = []
    dressed = dress.dress_all(buildings_placed, lots, catalog, pick, occupancy, regions, seed, panels)
    add_placed(dressed)
    # los tableros de los letreros (de color, con su franja): mallas nuestras detrás de las letras
    for panel in panels:
        line = shapely.geometry.LineString([panel['a'], panel['b']])
        mesh.box_along(line, panel['thickness'], panel['bottom'], panel['top'], panel['color'])
        if panel.get('band'):
            height = panel['top'] - panel['bottom']
            mesh.box_along(line, panel["thickness"] + 0.02, panel['bottom'], panel['bottom'] + height * 0.22, panel['band'])
    report['lots'] = len(dressed)
    # the gas stations' forecourts: paved, the canopy over the islands and their pumps, the tall sign at the corner
    for blk_id, gp in gas_plans.items():
        got, sign = gasstation.furnish(gp, catalog, pick, random.Random(seed * 37 + len(blk_id)), REPO)
        for e in got:
            e.update(zone=blk_id, scale=1.0)
            entities.append(e)
            m = catalog['models'][e['asset']]
            occupancy.add(parcels.footprint_poly(m, e['x'], e['y'], e['yaw']).buffer(0.3))
        occupancy.add(gp['forecourt'].buffer(-1.0))   # (no yard clutter, trees or planters on the forecourt)
        mesh.flat(gp['forecourt'], 0.02, 'concrete')   # (the pumps' ground; the rest stays earth)
        # parking bays in front of the shops, clear of the canopy and its pumps
        avoid = unary_union([gasstation.model_footprint(catalog['models'][e['asset']], e['x'], e['y'], e['yaw']).buffer(0.4)
                             for e in got]) if got else None
        bays = gasstation.parking_stripes(gp, avoid)
        for stripe in bays:
            mesh.flat(stripe, 0.028, 'paint_yellow')   # (white barely shows on the pale concrete)
        report[f'gas_bays_{blk_id}'] = len(bays)
        if sign:
            sx, sy, cam = sign
            face = (-cam[1], cam[0])
            panel = shapely.geometry.LineString([(sx - face[0] * 1.1, sy - face[1] * 1.1), (sx + face[0] * 1.1, sy + face[1] * 1.1)])
            mesh.box_along(panel, 0.35, 0.0, 6.5, 'sign_green')
            mesh.box_along(panel, 0.4, 4.6, 5.9, 'sign_white')
            f = {'a': (sx - face[0] * 1.1 + cam[0] * 0.2, sy - face[1] * 1.1 + cam[1] * 0.2), 't': face, 'n': cam, 'width': 2.2}
            word = dress.word_on_facade({'words': [gas.get('sign', 'PEMEJ')], 'height': 4.75, 'scale': 0.75, 'margin': 0.1},
                                        f, {'yaw': parcels.yaw_facing(*cam), 'zone': blk_id}, catalog, pick,
                                        random.Random(seed), None)
            add_placed(word)
            occupancy.add(panel.buffer(0.6))
        report[f'gas_station_{blk_id}'] = len(got)
    frng = random.Random(seed * 59 + 3)
    for blk_id, yard_poly, stores in fancy_yards:
        # (the well-off block tiled, not earth: on the block's own outline, which the sidewalk shares, not the yard's shrunk
        # one, which left a strip with no floor at all between the two: the sky showed through, the blue line)
        mesh.flat(next(b.get('ground_poly', b['poly']) for b in blocks if b['id'] == blk_id), 0.02, 'plaza_tiles')
        for e in fancy_furniture(yard_poly, stores, frng, theme.get('fancy_furniture', {})):
            m = catalog['models'].get(e['asset'])
            if m is None:
                continue
            if e['role'] == 'palm':
                # a palm stands in the planter just placed (else none)
                if not entities or entities[-1].get('role') != 'planter' or entities[-1].get('spawner') != 'fancy_block':
                    continue
            elif e['role'] == 'ivy':
                pass   # (on the wall: it takes no floor)
            elif e['role'] == 'beach_umbrella':
                # a parasol over the table just placed
                if not entities or entities[-1].get('role') != 'table_set' or entities[-1].get('spawner') != 'fancy_block':
                    continue
            else:
                # a palm by its trunk (its crown is over everything)
                foot = Point(e['x'], e['y']).buffer(0.45) if e['role'] == 'tall_palm' else                     parcels.footprint_poly(m, e['x'], e['y'], e['yaw'])
                if occupancy.hits(foot.buffer(0.1)) or not yard_poly.buffer(0.3).contains(foot):
                    continue
                occupancy.add(foot.buffer(0.15))
            entities.append(dict(e, role='palm' if e['role'] == 'tall_palm' else e['role'], zone=blk_id,
                                 scale=e.get('scale', 1.0), spawner='fancy_block'))
    # the strip malls' parking lots: paved, a few cars left in them
    lrng = random.Random(seed * 43 + 1)
    for blk_id, lot in strip_lots.items():
        mesh.flat(lot, 0.02, 'asphalt')
        cars = pick('car')
        for x, y in (spawners.poisson(lot.buffer(-2.0), 7.0, lrng)[:4] if cars else []):
            asset = lrng.choice(cars)
            m = catalog['models'][asset]
            yaw = lrng.choice((0.0, 90.0, 180.0, 270.0)) + lrng.uniform(-12, 12)
            foot = parcels.footprint_poly(m, x, y, yaw)
            if lot.contains(foot) and not occupancy.hits(foot.buffer(0.3)):
                occupancy.add(foot.buffer(0.3))
                entities.append({'asset': asset, 'x': round(x, 2), 'y': round(y, 2), 'yaw': round(yaw, 2), 'scale': 1.0,
                                 'role': 'car', 'zone': blk_id, 'spawner': 'strip_mall_cars'})
        occupancy.add(lot.buffer(-1.0))
    # the seawall promenade's landmark: a word in big letters on its esplanade, facing the game's camera
    if brief.get('landmarks', {}).get('letters'):
        signs = landmark_pieces.letters(brief['landmarks']['letters'], net, catalog, pick, occupancy, seed)
        add_placed(signs)
        report['landmark_letters'] = len(signs)
    # el apocalipsis en lugares, luego los postes con sus cables y el papel picado
    staged = setpieces.place_all(pieces, net, buildings_placed, catalog, pick, occupancy, regions, seed)
    add_placed(staged)
    report['setpieces'] = len(staged)
    if overhead_spec.get('power'):
        poles = overhead.power_lines(net, overhead_spec['power'], mesh, catalog, pick, occupancy, seed)
        add_placed(poles)
        report['power_poles'] = len(poles)
    if overhead_spec.get('papel_picado'):
        report['papel_picado'] = overhead.papel_picado(net, overhead_spec['papel_picado'], mesh, regions, seed)
    # el pueblo sigue más allá del área de juego: edificios de fondo, baratos, para los paneos
    back = background_ring(net, brief.get('background', {}), catalog, pick, seed, buildings)
    add_placed(back)
    report['background'] = len(back)
    rocks = sea_rocks(net, brief.get('sea_rocks'), catalog, pick, seed)
    add_placed(rocks)
    report['sea_rocks'] = len(rocks)
    boats = pier_boats(net, brief.get('pier_boats'), catalog, pick, seed)
    add_placed(boats)
    report['pier_boats'] = len(boats)

    # the palapas' room on the sand (their posts and shade: the umbrellas and loungers keep off)
    for px, py in getattr(net, 'palapas', []):
        occupancy.add(Point(px, py).buffer(3.2))

    # --- spawners ---
    # el suelo para las máscaras de altura y pendiente: el cerro donde lo hay, plano lo demás
    hill_area = regions.get('hill')
    ground_height = (lambda x, y: hill_height(x, y) if hill_area is not None and shapely.contains_xy(hill_area, x, y) else 0.0) \
        if hill_height else None
    tidy = set(wealthy.get('clear', [])) if regions.get('wealthy') is not None else set()
    skipped = set(brief.get('skip_spawners', []))   # the brief's "skip_spawners": the presets' spawners it leaves out
    for preset in presets:
        for spec in preset.get('spawners', []):
            if spec['name'] in skipped:
                continue
            if spec['name'] in tidy:
                # wear, litter and grime stay out of the well-off streets
                spec = dict(spec, masks=list(spec.get('masks', [])) + [{'type': 'inside', 'region': 'wealthy', 'invert': True}])
            got = spawners.run(spec, net, regions, catalog, lambda role: models_for(catalog, role), occupancy, seed,
                               mesh=mesh, decal_height=decal_height, ground_height=ground_height)
            report[spec['name']] = len(got)
            for p in got:
                if 'asset' not in p:
                    continue
                z = spec.get('lift', 0.0)   # lo sembrado en una jardinera: sobre su tierra
                if spec.get('where') == 'hill':
                    z = round(hill_height(p['x'], p['y']) - 0.05, 3)
                entities.append({'asset': p['asset'], 'x': p['x'], 'y': p['y'], 'yaw': p['yaw'], 'scale': p['scale'],
                                 'role': p['role'], 'zone': spec.get('where'), 'z': z, 'spawner': spec['name'],
                                 **({'tint': p['tint']} if p.get('tint') else {}),
                                 **({'unbreakable': True} if p.get('unbreakable') else {})})

    # --- desgaste en las fachadas: escurrimientos arriba, aplanado caído abajo ---
    chance = brief.get('facade_decals', 0.6)
    frng = random.Random(seed * 7 + 3)
    for e in entities:
        m = catalog['models'][e['asset']]
        if 'building' not in m['tags'] or frng.random() > chance or e.get('zone') in wealthy_ids:
            continue
        a_pt, b_pt = front_edge(m, e)
        width = math.hypot(b_pt[0] - a_pt[0], b_pt[1] - a_pt[1])
        height = m['size'][2] * e.get('scale', 1.0)
        for _ in range(frng.randint(1, 3)):
            w = frng.uniform(1.5, 3.5)
            if w >= width - 0.5:
                continue
            s = frng.uniform(0.2, width - w - 0.2)
            t0, t1 = s / width, (s + w) / width
            p0 = (a_pt[0] + (b_pt[0] - a_pt[0]) * t0, a_pt[1] + (b_pt[1] - a_pt[1]) * t0)
            p1 = (a_pt[0] + (b_pt[0] - a_pt[0]) * t1, a_pt[1] + (b_pt[1] - a_pt[1]) * t1)
            if frng.random() < 0.6 and height > 3.0:
                top = height * frng.uniform(0.8, 0.95)
                mesh.facade_decal(p0, p1, max(0.3, top - w * 1.2), top, frng.choice(decals.KINDS['drips']))
            else:
                bottom = frng.uniform(0.2, 1.0)
                mesh.facade_decal(p0, p1, bottom, bottom + w * 0.8, frng.choice(decals.KINDS['peeling']))

    # --- las mallas a disco, como entidades de piso ---
    out_dir = OUT_ROOT / scene.name
    names = mesh.write(out_dir, 'ground') + far.write(out_dir, 'backdrop')
    generated = {}
    for name in names:
        path = out_dir / f'{name}.gltf'
        low, high = citykit.model_bounds(path)
        key = f'levelgen/{scene.name}/{name}'
        generated[key] = {'path': path.relative_to(REPO).as_posix(),
                          'size': [round(float(high[0] - low[0]), 2), round(float(high[2] - low[2]), 2), round(float(high[1] - low[1]), 2)],
                          'low': [round(float(v), 3) for v in low], 'high': [round(float(v), 3) for v in high]}
    data['generated'] = generated
    extend_catalog(scene, catalog)
    data['entities'] = []
    for key in generated:
        scene.add(key, 0.0, 0.0, 0.0, 1.0, role='ground', tags=['generated_mesh'], entity_id=key.rsplit('/', 1)[-1])
    # nubes: los anillos del tema, grandes, alrededor del pueblo
    for k, ring in enumerate(models_for(catalog, 'cloud_ring')[:2]):
        c = net.play.centroid
        entities.append({'asset': ring, 'x': round(c.x, 2), 'y': round(c.y, 2), 'yaw': 40.0 * k, 'scale': 6.0 + 2.5 * k,
                         'role': 'cloud_ring', 'zone': None, 'z': 20.0 * k})
    for e in entities:
        scene.add(e['asset'], e['x'], e['y'], e['yaw'], e['scale'], zone=e.get('zone'), role=e['role'], z=e.get('z', 0.0),
                  **({'spawner': e['spawner']} if e.get('spawner') else {}), **({'tilt': e['tilt']} if e.get('tilt') else {}),
                  **({'paint': e['paint']} if e.get('paint') else {}), **({'tint': e['tint']} if e.get('tint') else {}),
                  **({'posts': e['posts']} if e.get('posts') is not None else {}), **({'word': e['word']} if e.get('word') else {}), **({'pristine': True} if e.get('pristine') else {}),
                  **({'unbreakable': True} if e.get('unbreakable') else {}), **({'crew': e['crew']} if e.get('crew') else {}))

    # --- colisión: el mar, el río (salvo los puentes) y el cerro, como cadenas cerradas ---
    outlines = []
    crossings = unary_union([g for g in (a.get('bridges'), a.get('footbridge')) if g is not None and not g.is_empty]) \
        if (a.get('bridges') is not None or a.get('footbridge') is not None) else None
    limit = box(*data['bounds']).buffer(3.0)   # (con la playa entera)
    water = a['sea'].intersection(limit)
    river = a['river'].intersection(limit)
    if crossings is not None:
        river = river.difference(crossings.buffer(0.2))
    for geometry in (water, river, a['hill'].intersection(limit)):
        # without the slivers the areas' cuts leave along the shore (rings of no area): widened, each was a wall of its own
        geometry = unary_union([p for p in polygons(geometry) if p.area > 1.0])
        for p in polygons(geometry.buffer(0.3).simplify(0.2)):
            outlines.append({'layer': 'solid', 'points': [[round(x, 2), round(y, 2)] for x, y in list(p.exterior.coords)[:-1]]})
    # los pretiles de los puentes: el jugador no salta al río por los lados
    for e in net.edges:
        if e.get('bridge'):
            for side in (-1, 1):
                rail = e['line'].offset_curve(side * (e['width'] / 2 + net.brief['sidewalk'] + 0.2))
                for part in polygons(rail.buffer(0.2, cap_style=2)):
                    outlines.append({'layer': 'solid', 'filled': True, 'points': [[round(x, 2), round(y, 2)] for x, y in list(part.exterior.coords)[:-1]]})
    # el mástil de la escultura del muelle, en medio de su plataforma: no se atraviesa
    if getattr(net, 'pier', None):
        mast = Point(net.pier['tip']).buffer(0.45, 8)
        outlines.append({'layer': 'solid', 'filled': True, 'points': [[round(x, 2), round(y, 2)] for x, y in list(mast.exterior.coords)[:-1]]})
    # las jardineras de las plazas: no se pisan (su borde es bajo, pero es un borde). Llenas, no cadenas: lo que quede
    # adentro (un spawner) se ve como no caminable, y nada aparece encerrado en ellas
    for plan in plaza_plans:
        for bed in plan['beds']:
            for part in polygons(bed.simplify(0.1)):
                outlines.append({'layer': 'solid', 'filled': True, 'points': [[round(x, 2), round(y, 2)] for x, y in list(part.exterior.coords)[:-1]]})
    # las bardas: no se cruzan
    for piece in barda_lines:
        for part in polygons(piece.buffer(barda.get('thickness', 0.25) / 2 + 0.05, cap_style=2, join_style=2)):
            outlines.append({'layer': 'solid', 'filled': True, 'points': [[round(x, 2), round(y, 2)] for x, y in list(part.exterior.coords)[:-1]]})
    data['outlines'] = outlines

    # --- entradas, inicio y misión ---
    # en las calles los zombis salen de las coladeras: una tapa en cada uno de los dos puntos de la zona (export.spawn_points);
    # en los callejones al cerro salen de la tierra
    manholes = models_for(catalog, 'manhole')
    for k, (ex, ey, kind) in enumerate(net.entrances):
        zone = {'rect': [round(ex - 2, 2), round(ey - 1, 2), round(ex + 2, 2), round(ey + 1, 2)], 'kind': 'spawn'}
        if kind == 'street' and manholes:
            zone['manhole'] = True
            for j, px in enumerate((ex - 1.0, ex + 1.0)):
                scene.add(manholes[(k + j) % len(manholes)], round(px, 2), round(ey, 2), (k * 53.0 + j * 90.0) % 360.0, 1.0,
                          zone=None, role='manhole')
        zones[f'spawn_{k + 1:02d}'] = zone
    # where the streets leave the town: closed across, past the manholes (closures.py)
    import closures
    closures.close_streets(scene, catalog, models_for, seed)
    start = None
    if plazas:
        c = plazas[0]['poly'].centroid
        start = (round(c.x + 5.0, 2), round(c.y, 2))
    else:
        start = (0.0, 0.0)
    data['start'] = list(start)
    gift = [k for k, m in catalog['models'].items() if 'gift' in m['roles']]
    if gift:
        scene.add('Chest_Special' if 'Chest_Special' in gift else gift[0], start[0] - 3.0, start[1] + 2.0, 30.0, 1.6, role='gift')
    mission = dict({'id': 'survival', 'mode': 'survival', 'kills': 80}, **brief.get('mission', {}))
    mission['start'] = list(start)
    data['missions'] = [mission]

    name_places(scene, catalog, brief)
    reach = reachability(scene, catalog, regions, net, start)
    lines_out = [f'red: {len(net.edges)} ejes, {len(net.blocks)} manzanas ({len(plazas)} plazas), {len(net.entrances)} entradas',
                 f'mallas: {len(names)} cuadros, {mesh.triangles()} triángulos -> {out_dir}',
                 f'entidades: {len(scene.entities)}',
                 'spawners: ' + ', '.join(f'{k} {v}' for k, v in report.items()),
                 reach]
    return '\n'.join(lines_out)


def keep_outside(old, new, block_id):
    """One block made again (the brief's block_overrides, lb.py generate --only-block): the new scene's things of that
    block (its zone, or lying in its polygon) with the old scene's everything else, hand edits included. The dressing
    after the buildings draws from streams shared by the whole town, so a block built anew would move the signs, plants
    and clutter of every block after it: those come from the old scene. The generated meshes (ground, yards) are the new
    run's: the other blocks keep their buildings (block_overrides), so their yards are the same. Returns how many things
    the block had before and has now."""
    from shapely.geometry import Polygon as _Polygon
    poly = new['zones'][block_id]['poly']
    area = _Polygon(poly[0] if isinstance(poly[0][0], list) else poly).buffer(0.3)

    def of_block(e):
        if 'generated_mesh' in e.get('tags', []):
            return False
        return e.get('zone') == block_id or (e.get('zone') is None and area.contains(Point(e['x'], e['y'])))
    kept = [e for e in old['entities'] if not of_block(e) and 'generated_mesh' not in e.get('tags', [])]
    taken = {e['id'] for e in kept}
    meshes = [e for e in new['entities'] if 'generated_mesh' in e.get('tags', [])]
    taken.update(e['id'] for e in meshes)
    inside = []
    for e in new['entities']:
        if not of_block(e):
            continue
        e = dict(e)
        base, k = e['id'], 1
        while e['id'] in taken:
            e['id'] = f'{base}_{block_id}_{k}'
            k += 1
        taken.add(e['id'])
        inside.append(e)
    before = sum(1 for e in old['entities'] if of_block(e))
    new['entities'] = meshes + kept + inside
    return before, len(inside)


# what a block is called, by its type (the missions' generator and the game designer refer to places by these)
PLACE_NAMES = {
    'plazuela_pemex': 'Plazuela PemeJ', 'strip_mall': 'Plaza Comercial', 'civic': 'Plaza Civica',
    'children': 'Plaza de los Ninos', 'dance': 'Plaza del Baile', 'park': 'Parque', 'commercial': 'Manzana Comercial',
    'mixed': 'Manzana Mixta', 'residential': 'Manzana Residencial', 'promenade': 'Malecon',
}


def name_places(scene, catalog, brief):
    """Each block's zone gets a name ("name", unique) and what is in it ("has": its stores' and landmarks' names), and
    the scene's folder a table of them (places.md): where the park, the shopping centres and the gas station are, for
    the missions' generator and the game designer. A brief's "place_names" ({block_id: name}) wins."""
    zones = scene.data['zones']
    used = {}
    forced = brief.get('place_names') or {}
    rows = []
    for zid, zone in sorted(zones.items()):
        if zone.get('kind') not in ('block', 'park', 'plaza', 'gas_station'):
            continue
        has = []
        for e in scene.entities:
            if e.get('zone') != zid:
                continue
            if e.get('role') in ('fancy_store', 'landmark', 'church', 'city_hall', 'gas_canopy'):
                # its name without a pack's prefix ("XX_Kind_") nor "Store_"
                has.append(re.sub(r'^[A-Z]+_[A-Z][a-z]+_', '', e['asset'].rsplit('/', 1)[-1]).removeprefix('Store_'))
            if e.get('role') == 'shop' and e.get('word'):
                has.append(e['word'])
        kind = zone.get('type') or (zid if zid == 'promenade' else zone.get('kind'))
        if zone.get('kind') == 'gas_station' and zone.get('wealthy') and kind not in ('plazuela_pemex', 'strip_mall'):
            kind = 'fancy'
        base = forced.get(zid) or ('Centro Comercial' if kind == 'fancy' else PLACE_NAMES.get(kind, 'Manzana'))
        if zid not in forced:
            used[base] = used.get(base, 0) + 1
            name = base if used[base] == 1 else f'{base} {used[base]}'
        else:
            name = base
        zone['name'] = name
        if has:
            zone['has'] = sorted(set(has))
        r = zone['rect']
        rows.append(f"| {zid} | {name} | {kind} | {(r[0] + r[2]) / 2:.0f}, {(r[1] + r[3]) / 2:.0f} | {', '.join(sorted(set(has)))} |")
    folder = getattr(scene, 'folder', None)
    if folder is not None:
        text = ['# Places of ' + scene.name, '', 'Made by the generator (coastal.name_places): block, name, type, center (m), '
                'what stands in it.', '', '| Block | Name | Type | Center | Has |', '|---|---|---|---|---|'] + rows
        (folder / 'places.md').write_text('\n'.join(text) + '\n', encoding='utf-8')


def reachability(scene, catalog, regions, net, start, cell=1.0):
    """Lo caminable (calles, banquetas, malecón, plazas, callejones, puentes) que se alcanza desde el inicio sin cruzar
    edificios, agua ni cerro; y si cada entrada de zombis se alcanza. Un texto con el resultado."""
    x0, y0, x1, y1 = scene.data["bounds"]
    nx, ny = int((x1 - x0) / cell), int((y1 - y0) / cell)
    xs = x0 + (np.arange(nx) + 0.5) * cell
    ys = y0 + (np.arange(ny) + 0.5) * cell
    gx, gy = np.meshgrid(xs, ys)
    walk = unary_union([g for g in (regions['road'], regions['walk'], net.areas.get('bridges')) if g is not None])
    walk = walk.difference(net.areas['sea']).difference(net.areas['river'].difference(net.areas.get('bridges') or Point(0, 0).buffer(0)))
    walkable = shapely.contains_xy(walk, gx, gy)
    blocked = []
    for e in scene.entities:
        m = catalog['models'].get(e['asset'])
        if m is None or 'ground' in m['tags'] or 'small' in m['tags'] or 'decal' in m['tags']:
            continue
        if 'tree' in m['tags']:
            blocked.append(Point(e['x'], e['y']).buffer(0.4))
        else:
            blocked.append(parcels.footprint_poly(m, e['x'], e['y'], e['yaw'], e.get('scale', 1.0)))
    if blocked:
        walkable &= ~shapely.contains_xy(unary_union(blocked), gx, gy)
    sx, sy = int((start[0] - x0) / cell), int((start[1] - y0) / cell)
    if not (0 <= sx < nx and 0 <= sy < ny) or not walkable[sy, sx]:
        return 'alcance: el inicio no está en un lugar caminable'
    seen = np.zeros_like(walkable)
    stack = [(sy, sx)]
    seen[sy, sx] = True
    while stack:
        j, i = stack.pop()
        for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            a, b = j + dj, i + di
            if 0 <= a < ny and 0 <= b < nx and walkable[a, b] and not seen[a, b]:
                seen[a, b] = True
                stack.append((a, b))
    total, got = int(walkable.sum()), int(seen.sum())
    unreachable = []
    for name, zone in scene.data['zones'].items():
        if zone['kind'] != 'spawn':
            continue
        r = zone['rect']
        i, j = int(((r[0] + r[2]) / 2 - x0) / cell), int(((r[1] + r[3]) / 2 - y0) / cell)
        i, j = min(max(i, 0), nx - 1), min(max(j, 0), ny - 1)
        window = seen[max(j - 2, 0):j + 3, max(i - 2, 0):i + 3]
        if not window.any():
            unreachable.append(name)
    text = f'alcance: {got}/{total} m2 caminables se alcanzan desde el inicio ({100 * got / max(total, 1):.0f}%)'
    if unreachable:
        text += '; entradas sin camino: ' + ', '.join(unreachable)
    return text
