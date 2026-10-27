"""Generador procedural: un pueblo completo a partir de un "brief" (unos cuantos datos) y una semilla. Usa las mismas
operaciones que lb.py (ops.py), que siguen las reglas de rules.py; el resultado se revisa con validate y se ajusta a mano.

Brief (JSON, todo opcional):
    {"grid": [3, 3], "arm": 5, "seed": 1,
     "blocks": {"park": 1, "downtown": 1, "shops": 1, "houses": 1, "civic": 0, "parking": 0},
     "ring": true, "sidewalks": true, "sign": true, "gift": true, "landmark": true,
     "mission": {"kills": 60}}

Trazado: "layout": "grid" (el de siempre: los tipos de manzana de "blocks") o "historic" (un centro histórico de
ciudad española o mexicana: cuadrícula de 3 x 3 manzanas, la plaza mayor al centro con su kiosco; la iglesia al norte de
la plaza, el palacio de gobierno al oriente, portales al poniente; en las esquinas "parks" jardines (2) y "parking"
estacionamientos (1), y las demás manzanas mixtas de casas y oficinas bajas con patios arbolados).

Tipos de manzana:
  park      parque (pasto, caminos, árboles, arbustos) con la torre de agua y la caja de regalo
  downtown  edificios grandes y medianos en los cuatro lados (oficinas, hotel, ayuntamiento)
  shops     calle de tiendas: chicos y medianos al norte y al sur; el centro de asfalto con autos estacionados
  houses    casas armadas por el generador de edificios (buildings.py) en los cuatro lados, pasto y árboles en medio
  civic     comisaría, biblioteca, escuela e iglesia
  parking   un estacionamiento: filas de autos y contenedores
Si hay más manzanas que las pedidas, las que sobran son de tiendas o de casas al azar.
"""
import json
import random

import ops
import rules

SMALL, MEDIUM, LARGE = 'Building_Small_1', 'Building_Medium_2_001', 'Building_Large_2'
CARS = ['Vehicle_Pickup', 'Vehicle_Sports', 'Vehicle_Truck']

DEFAULT = {'grid': [3, 3], 'arm': 5, 'seed': 1, 'blocks': {'park': 1, 'downtown': 1, 'shops': 1, 'houses': 1},
           'ring': True, 'sidewalks': True, 'sign': True, 'gift': True, 'landmark': True, 'mission': {'kills': 60}}


def read_brief(text):
    brief = json.loads(json.dumps(DEFAULT))
    if text:
        given = json.loads(text)
        brief.update({k: v for k, v in given.items() if k != 'blocks'})
        if 'blocks' in given:
            brief['blocks'] = given['blocks']
        if given.get('layout') == 'historic':
            # el centro histórico tiene su cuadrícula (3 x 3 manzanas) y calles más cortas entre cruces, si no se piden otras
            brief['grid'] = given.get('grid', [4, 4])
            brief['arm'] = given.get('arm', 5)
            brief['landmark'] = given.get('landmark', False)   # el kiosco es su monumento (sin torre de agua)
    return brief


def assign_blocks(names, wanted, rng):
    """A cada manzana su tipo: los pedidos primero (las del centro para el parque y el centro), luego tiendas o casas."""
    order = sorted(names)
    rng.shuffle(order)
    kinds = []
    for kind in ('park', 'downtown', 'civic', 'shops', 'houses', 'parking'):
        kinds += [kind] * int(wanted.get(kind, 0))
    while len(kinds) < len(order):
        kinds.append(rng.choice(['shops', 'houses']))
    return dict(zip(order, kinds[:len(order)]))


def parked_cars(scene, catalog, zone, rng, layer='props'):
    """Autos estacionados en dos filas en el centro de una zona (se rompen: van en la capa props)."""
    r = scene.zone_rect(zone)
    cx, cy = (r[0] + r[2]) / 2, (r[1] + r[3]) / 2
    placed = []
    for dy in (-3.2, 3.2):
        for k in range(-3, 4):
            if rng.random() < 0.35:
                continue   # lugares vacíos
            asset = rng.choice(CARS)
            e = {'asset': asset, 'x': cx + k * 3.4, 'y': cy + dy, 'yaw': 90.0 if dy < 0 else 270.0, 'scale': 1.0}
            box = ops.box_of(ops.footprint(catalog, e))
            if not ops.inside(box, r) or ops.overlaps(box, ops.solid_boxes(scene, catalog), 0.3):
                continue
            car = scene.add(asset, e['x'], e['y'], e['yaw'], zone=zone, role='car')
            car['layer'] = layer
            placed.append(car)
    return placed


def build_block(scene, catalog, zone, kind, rng, report):
    seed = rng.randint(1, 10000)
    if kind == 'park':
        report.setdefault('park', []).append(zone)
        return ops.park(scene, catalog, zone, seed)
    if kind == 'downtown':
        # oficinas del generador de edificios (piezas de Downtown): fachadas continuas de 3 a 6 pisos en los cuatro lados
        scene.data['zones'][zone]['kind'] = 'block'
        return ops.offices(scene, catalog, zone, 'NSEW', seed)
    if kind == 'shops':
        placed = []
        for side in 'NS':
            placed += ops.fill_side(scene, catalog, [SMALL, MEDIUM], zone, side, ['shop', 'diner', 'shop', 'apartment'], seed + ord(side))
        scene.data['zones'][zone]['kind'] = 'block'
        # el centro es un estacionamiento: una zona 'lot' propia para que los autos estén donde las reglas los dejan
        r = scene.zone_rect(zone)
        cy = (r[1] + r[3]) / 2
        lot = f'{zone}_lot'
        scene.data['zones'][lot] = {'rect': [r[0] + 2.0, cy - 6.5, r[2] - 2.0, cy + 6.5], 'kind': 'lot'}
        return placed + parked_cars(scene, catalog, lot, rng)
    if kind == 'houses':
        # casas del generador de edificios (piezas del kit medieval), con pasto y árboles en los patios
        ops.set_ground(scene, catalog, zone, 'Ground_Grass_9x9')
        placed = ops.houses(scene, catalog, zone, 'NSEW', seed)
        placed += ops.scatter(scene, catalog, ['CommonTree_1', 'CommonTree_2', 'CommonTree_5', 'Bush_Common'], zone, 6, seed, gap=1.5,
                              scale=(0.8, 1.0))   # su rol: el del catálogo (árbol o arbusto)
        return placed
    if kind == 'civic':
        placed = []
        for side, asset, role in (('N', LARGE, 'police_station'), ('S', MEDIUM, 'library'), ('E', MEDIUM, 'school'), ('W', MEDIUM, 'church')):
            e = ops.lot(scene, catalog, asset, zone, side, 2.0, role)
            if e:
                placed.append(e)
        return placed
    if kind == 'mixed':
        scene.data['zones'][zone]['kind'] = 'block'
        return ops.mixed(scene, catalog, zone, 'NSEW', seed)
    if kind in ('church', 'palace', 'portales'):
        # el lado que da a la plaza lleva lo suyo; los otros tres, la manzana mixta
        scene.data['zones'][zone]['kind'] = 'block'
        side = report['facing'][zone]
        special = {'church': ops.church, 'palace': ops.palace, 'portales': ops.portales}[kind](scene, catalog, zone, side, seed)
        others = ''.join(s for s in 'NSEW' if s != side)
        return special + ops.mixed(scene, catalog, zone, others, seed)
    if kind == 'plaza':
        report.setdefault('park', []).append(zone)
        return ops.plaza(scene, catalog, zone, seed)
    if kind == 'parking':
        scene.data['zones'][zone]['kind'] = 'lot'
        placed = parked_cars(scene, catalog, zone, rng)
        placed += ops.scatter(scene, catalog, ['Container_Red', 'Container_Green'], zone, 2, seed, gap=1.0, role='container')
        return placed
    raise ValueError(f'tipo de manzana desconocido: {kind}')


def generate(scene, catalog, brief):
    """Arma la escena (debe estar vacía) según el brief. Devuelve un resumen como texto."""
    rng = random.Random(brief.get('seed', 1))
    historic = brief.get('layout') == 'historic'
    cols, rows = brief.get('grid', [4, 4]) if historic else brief['grid']
    ops.streets(scene, catalog, cols, rows, brief.get('arm', 5))
    blocks = [name for name, z in scene.data['zones'].items() if name.startswith('block_')]
    lines = []
    report = {}
    if historic:
        # la plaza en la manzana del centro; alrededor, lo que da a ella (y el lado con que le da la cara)
        ci, cj = (cols - 2) // 2, (rows - 2) // 2
        kinds = {b: 'mixed' for b in blocks}
        kinds[f'block_{ci}_{cj}'] = 'plaza'
        report['facing'] = {}
        for (di, dj), kind, side in (((0, 1), 'church', 'S'), ((1, 0), 'palace', 'W'), ((-1, 0), 'portales', 'E'), ((0, -1), 'mixed', 'N')):
            name = f'block_{ci + di}_{cj + dj}'
            if name in kinds:
                kinds[name] = kind
                report['facing'][name] = side
        # las demás (las esquinas): jardines y un estacionamiento entre las manzanas, que no todo sea edificio
        rest = sorted(b for b in blocks if kinds[b] == 'mixed' and b not in report['facing'])
        rng.shuffle(rest)
        wanted = ['park'] * int(brief.get('parks', 2)) + ['parking'] * int(brief.get('parking', 1))
        for name, kind in zip(rest, wanted):
            kinds[name] = kind
    else:
        kinds = assign_blocks(blocks, brief.get('blocks', {}), rng)
    for zone in sorted(kinds):
        placed = build_block(scene, catalog, zone, kinds[zone], rng, report)
        lines.append(f'{zone}: {kinds[zone]} ({len(placed)} cosas)')
    if brief.get('ring', True):
        # el borde: casas y oficinas bajas mirando hacia adentro (los edificios enteros del paquete miden hasta 28 m)
        mix = {'ring_s': 'N', 'ring_n': 'S', 'ring_w': 'E', 'ring_e': 'W'}
        for zone, side in mix.items():
            ops.mixed(scene, catalog, zone, side, rng.randint(1, 10000), courtyard=False)
    if brief.get('sidewalks', True):
        ops.sidewalks(scene, catalog, rng.randint(1, 10000))
    if brief.get('sign', True):
        sign = ops.town_sign(scene, catalog, seed=rng.randint(1, 10000))
        lines.append(f'letrero: {sign["id"] if sign else "no cupo"}')
    parks = report.get('park', [])
    if brief.get('landmark', True) and parks:
        ops.scatter(scene, catalog, ['WaterTower'], parks[0], 1, rng.randint(1, 10000), gap=2.0, role='landmark')
    if brief.get('gift', True):
        where = parks[0] if parks else sorted(kinds)[0]
        gift = ops.scatter(scene, catalog, ['Chest_Special'], where, 1, rng.randint(1, 10000), gap=1.0, role='gift', scale=(1.6, 1.6))
        lines.append(f'regalo: {gift[0]["id"] if gift else "no cupo"} en {where}')
    mission = brief.get('mission')
    if mission:
        layer, area, spawns = ops.mission_clutter(scene, catalog, 'survival', mode='survival')
        fenced = {'area': area['rect'], 'spawns': spawns} if area else {}
        scene.data['missions'] = [{'id': 'survival', 'mode': 'survival', 'kills': int(mission.get('kills', 60)),
                                   'layers': ['props', layer], **fenced,
                                   'title': mission.get('title', f'{scene.name.upper()}: kill {int(mission.get("kills", 60))} of the infected')}]
    problems = ops.validate(scene, catalog)
    lines.append(f'{len(scene.entities)} entidades; validate: {len(problems)} problemas')
    lines += ['  ' + p for p in problems[:30]]
    return '\n'.join(lines)
