"""Operaciones sobre una escena: calles y manzanas, edificios en lotes, dispersar, filas, autos chocados, reemplazar, mover,
borrar, listar y validar. Cada una cambia scene.data; quien la llama guarda la versión (lb.py lo hace con una nota).
"""
import math
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'zombies_convert'))
import citykit  # noqa: E402
from downtown import CROSSING, SEGMENT, STREET_HALF, HALF_CROSSING, SIDE_YAW, building_front_corner, crossing_fillers  # noqa: E402
from scene import footprint, box_of, BASE, DEFAULT_LAYERS  # noqa: E402
import rules  # noqa: E402


# --- qué choca y qué no ---

def kind(catalog, asset):
    """'ground' (se pisa: calles, banquetas, marcas, caminos, pasto), 'tree' (choca solo el tronco) o 'solid'."""
    tags = catalog['models'][asset]['tags']
    if 'ground' in tags or 'decal' in tags or 'small' in tags or 'path' in tags:
        return 'ground'
    if 'tree' in tags:
        return 'tree'
    return 'solid'


def solid_boxes(scene, catalog, skip=None):
    """Las cajas (x0, y0, x1, y1) de lo que ocupa lugar: para no poner cosas encima."""
    boxes = []
    for e in scene.entities:
        if e is skip:
            continue
        k = kind(catalog, e['asset'])
        if k == 'ground':
            continue
        if k == 'tree':
            boxes.append((e['x'] - 0.6, e['y'] - 0.6, e['x'] + 0.6, e['y'] + 0.6))
        elif 'pole' in catalog['models'][e['asset']]['tags']:
            # farolas y semáforos: el poste (el brazo va por arriba, como su colisión en el juego)
            boxes.append((e['x'] - 0.3, e['y'] - 0.3, e['x'] + 0.3, e['y'] + 0.3))
        else:
            boxes.append(box_of(footprint(catalog, e)))
    return boxes


def overlaps(box, boxes, margin=0.0):
    return any(box[0] < b[2] + margin and box[2] > b[0] - margin and box[1] < b[3] + margin and box[3] > b[1] - margin for b in boxes)


def inside(box, rect):
    return box[0] >= rect[0] - 0.01 and box[1] >= rect[1] - 0.01 and box[2] <= rect[2] + 0.01 and box[3] <= rect[3] + 0.01


# --- calles y manzanas ---

def streets(scene, catalog, cols, rows, arm=6, ring=16.0):
    """Una cuadrícula de cols x rows cruces unidos por calles de cuatro carriles ('arm' tramos de 6 m entre cruces), con
    banquetas, línea amarilla y asfalto bajo las manzanas. Crea las zonas block_i_j (entre calles), street_h_j y street_v_i,
    una franja 'ring_*' alrededor (para edificios del borde) y una zona spawn en cada salida de calle, con contenedores que la
    cierran. Ajusta los límites de la escena a todo eso. Devuelve los cruces."""
    pitch = CROSSING + SEGMENT * arm
    xs = [(i - (cols - 1) / 2) * pitch for i in range(cols)]
    ys = [(j - (rows - 1) / 2) * pitch for j in range(rows)]
    zones = scene.data['zones']
    for x in xs:
        for y in ys:
            scene.add('Street_4WayIntersection', x, y, role='street', zone='streets', tags=['street'])
            for fx, fy, yaw in crossing_fillers(x, y):   # la calle bajo los pasos de cebra
                scene.add('Street_4Lane_Short', fx, fy, yaw, role='street', zone='streets', tags=['street'])
    for j, y in enumerate(ys):
        for i in range(cols - 1):
            for k in range(arm):
                sx = xs[i] + HALF_CROSSING + SEGMENT * (k + 0.5)
                scene.add('Street_4Lane', sx, y, 0.0, role='street', zone=f'street_h{j}', tags=['street'])
                scene.add('Decal_DoubleYellow_Straight', sx, y, 0.0, z=0.09, role='road_marking', zone=f'street_h{j}')
            zones[f'street_h{j}'] = {'rect': [xs[0], y - STREET_HALF, xs[-1], y + STREET_HALF], 'kind': 'street'}
    for i, x in enumerate(xs):
        for j in range(rows - 1):
            for k in range(arm):
                sy = ys[j] + HALF_CROSSING + SEGMENT * (k + 0.5)
                scene.add('Street_4Lane', x, sy, 90.0, role='street', zone=f'street_v{i}', tags=['street'])
                scene.add('Decal_DoubleYellow_Straight', x, sy, 90.0, z=0.09, role='road_marking', zone=f'street_v{i}')
            zones[f'street_v{i}'] = {'rect': [x - STREET_HALF, ys[0], x + STREET_HALF, ys[-1]], 'kind': 'street'}
    # banqueta donde el cruce termina antes que las banquetas de las calles
    reach = (STREET_HALF + HALF_CROSSING) / 2
    depth = (STREET_HALF - 6.0) / 2 + 6.0
    corner_scale = (HALF_CROSSING - STREET_HALF) / 3.0 * 1.03
    for x in xs:
        for y in ys:
            for sx in (-1, 1):
                for sy in (-1, 1):
                    for gx, gy in ((reach, depth), (depth, reach)):
                        scene.add('Sidewalk_NoCurb_3m', x + sx * gx, y + sy * gy, z=-0.04, scale=round(corner_scale, 4),
                                  role='sidewalk', zone='streets')
    # manzanas, con asfalto de 9 m
    for i in range(cols - 1):
        for j in range(rows - 1):
            rect = [xs[i] + STREET_HALF, ys[j] + STREET_HALF, xs[i + 1] - STREET_HALF, ys[j + 1] - STREET_HALF]
            zones[f'block_{i}_{j}'] = {'rect': rect, 'kind': 'block'}
            ground(scene, catalog, rect, f'block_{i}_{j}')
    # el borde: franjas para edificios alrededor, y las salidas de las calles cerradas con contenedores
    x0, x1 = xs[0] - STREET_HALF, xs[-1] + STREET_HALF
    y0, y1 = ys[0] - STREET_HALF, ys[-1] + STREET_HALF
    for name, rect in (('ring_s', [x0 - ring, y0 - ring, x1 + ring, y0]), ('ring_n', [x0 - ring, y1, x1 + ring, y1 + ring]),
                       ('ring_w', [x0 - ring, y0, x0, y1]), ('ring_e', [x1, y0, x1 + ring, y1])):
        zones[name] = {'rect': rect, 'kind': 'block'}
    for x in xs:
        for y in ys:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                if (dx == 1 and x == xs[-1]) or (dx == -1 and x == xs[0]) or (dy == 1 and y == ys[-1]) or (dy == -1 and y == ys[0]):
                    exit_street(scene, x, y, dx, dy)
    scene.data['bounds'] = [x0 - ring, y0 - ring, x1 + ring, y1 + ring]
    scene.data['grid'] = {'xs': xs, 'ys': ys}     # los cruces: sidewalks() pone las cosas de las banquetas con ellos
    scene.data['start'] = [xs[len(xs) // 2] + 3.0, ys[len(ys) // 2] - 3.0]
    return [(x, y) for x in xs for y in ys]


def ground(scene, catalog, rect, zone, asset='Street_Asphalt_9x9'):
    """Losetas de piso de 9 m (asfalto, o el pasto o la tierra de own_assets.py) que cubren un rectángulo."""
    low, high = catalog['models'][asset]['low'], catalog['models'][asset]['high']
    ax0, ay0, ax1, ay1 = building_front_corner(low, high, 0.0)
    size = ax1 - ax0
    # tantas losetas como quepan redondeando, escaladas para cubrir justo la zona (30 cm de más por lado, bajo las banquetas)
    width, height = rect[2] - rect[0] + 0.6, rect[3] - rect[1] + 0.6
    nx, ny = max(1, round(width / size)), max(1, round(height / size))
    scale = round(max(width / nx, height / ny) / size, 4)
    step = size * scale
    for ix in range(nx):
        for iy in range(ny):
            scene.add(asset, rect[0] - 0.3 + ix * step - ax0 * scale, rect[1] - 0.3 + iy * step - ay0 * scale, scale=scale,
                      role='ground', zone=zone)


def set_ground(scene, catalog, zone, asset):
    """Cambia el piso de una zona (las losetas con rol ground de esa zona) por otro modelo."""
    floor = [e for e in scene.entities if e.get('zone') == zone and e.get('role') == 'ground']
    size = lambda name: catalog['models'][name]['high'][0] - catalog['models'][name]['low'][0]
    if floor and abs(size(floor[0]['asset']) - size(asset)) > 0.01:
        # otro tamaño de loseta (las de 3 m de banqueta por las de 9 de asfalto): se vuelve a cubrir la zona
        ids = {e['id'] for e in floor}
        scene.remove(lambda e: e['id'] in ids)
        floor = []
    if not floor:
        ground(scene, catalog, scene.zone_rect(zone), zone, asset)
        return
    for e in floor:
        e['asset'] = asset


def exit_street(scene, x, y, dx, dy):
    """Una salida de la cuadrícula: tres contenedores atravesados en la punta del cruce, y una zona spawn detrás."""
    ex, ey = x + dx * HALF_CROSSING, y + dy * HALF_CROSSING
    cx, cy = ex - dx * 1.6, ey - dy * 1.6
    across = (1, 0) if dx == 0 else (0, 1)
    yaw = 0.0 if dx == 0 else 90.0
    for k, model in zip((-1, 0, 1), ('Container_Red', 'Container_Green', 'Container_Red')):
        scene.add(model, cx + across[0] * k * 5.9, cy + across[1] * k * 5.9, yaw, role='barricade', zone='edges')
    # los zombis entran justo por dentro de la barricada
    sx, sy = x + dx * (HALF_CROSSING - 7.5), y + dy * (HALF_CROSSING - 7.5)
    half_across = 5.0
    rect = [sx - (half_across if dx == 0 else 1.0), sy - (half_across if dy == 0 else 1.0),
            sx + (half_across if dx == 0 else 1.0), sy + (half_across if dy == 0 else 1.0)]
    side = {(1, 0): 'e', (-1, 0): 'w', (0, 1): 'n', (0, -1): 's'}[(dx, dy)]
    scene.data['zones'][f'spawn_{side}_{x:.0f}_{y:.0f}'] = {'rect': rect, 'kind': 'spawn'}


def sidewalks(scene, catalog, seed=1, lights=True, trees=True, hydrants=True, trash=True, traffic_lights=True):
    """Lo de las banquetas de cada tramo de calle (necesita streets()): una farola cada 18 m, un árbol a la mitad entre dos
    farolas (nunca encima de una), a veces un hidrante en la orilla y bolsas de basura, y un semáforo junto a cada cruce (a
    la derecha de quien llega). Nada se pone donde ya hay algo."""
    grid = scene.data.get('grid')
    if not grid:
        raise ValueError('primero streets(): las banquetas siguen las calles')
    rng = random.Random(seed)
    xs, ys = grid['xs'], grid['ys']
    tree_models = TREES_MIX
    boxes = solid_boxes(scene, catalog)
    placed = []

    def put(asset, x, y, yaw, role, scale=1.0, half=None):
        e = {'asset': asset, 'x': x, 'y': y, 'yaw': yaw, 'scale': scale}
        box = (x - half, y - half, x + half, y + half) if half else box_of(footprint(catalog, e))
        if overlaps(box, boxes, 0.2) or rules.too_close(role, x, y, placed):
            return
        entity = scene.add(asset, x, y, yaw, scale, zone='sidewalks', role=role)
        if role in ('hydrant', 'clutter'):
            entity['layer'] = 'props'   # lo que se rompe no es parte del pueblo fijo
        placed.append(entity)
        boxes.append(box)

    # (inicio del tramo, fin, eje fijo, horizontal)
    runs = [(xs[i] + HALF_CROSSING, xs[i + 1] - HALF_CROSSING, y, True) for y in ys for i in range(len(xs) - 1)]
    runs += [(ys[j] + HALF_CROSSING, ys[j + 1] - HALF_CROSSING, x, False) for x in xs for j in range(len(ys) - 1)]
    for start, end, axis, horizontal in runs:
        length = end - start
        for side in (-1, 1):
            def at(along, lateral):
                return (start + along, axis + side * lateral) if horizontal else (axis + side * lateral, start + along)
            # el brazo de la farola sobre el asfalto
            light_yaw = (180.0 if side < 0 else 0.0) if horizontal else (90.0 if side < 0 else 270.0)
            spots = [a for a in (6.0 + 18.0 * k for k in range(10)) if a < length - 3.0]
            if lights:
                for a in spots:
                    put('StreetLights', *at(a, 7.6), light_yaw, 'street_light', half=0.3)
            if trees:
                for a in (s + 9.0 for s in spots):
                    if a < length - 3.0:
                        put(rng.choice(tree_models), *at(a, 7.6), rng.uniform(0, 360), 'tree', round(rng.uniform(0.85, 1.0), 2), half=0.7)
            if hydrants and rng.random() < 0.5:
                put('FireHydrant', *at(rng.uniform(2.0, length - 2.0), 6.4), 0.0, 'hydrant')
            if traffic_lights:
                # a la entrada del cruce, del lado derecho de quien llega: al final del tramo de un lado, al principio del otro
                along = length - 1.5 if side > 0 else 1.5
                put(rng.choice(['TrafficLight_1', 'TrafficLight_2']), *at(along, 7.6), light_yaw, 'traffic_light', half=0.3)
            if trash:
                for _ in range(rng.randint(0, 2)):
                    put('TrashBag_1', *at(rng.uniform(2.0, length - 2.0), rng.uniform(7.2, 8.4)), rng.uniform(0, 360), 'clutter')
    return placed


def park(scene, catalog, zone, seed=1, trees=14, bushes=20, grass=40, rocks=6, paths=True):
    """Un parque en una zona: piso de pasto, caminos de piedra en cruz (losetas planas: las piedras modeladas cuestan ~2000
    triángulos cada una), árboles, arbustos, pasto y algunas rocas."""
    rng = random.Random(seed)
    r = scene.zone_rect(zone)
    scene.data['zones'][zone]['kind'] = 'park'
    set_ground(scene, catalog, zone, rules.GROUND['park'][0])   # pasto, no el asfalto de la manzana
    cx, cy = (r[0] + r[2]) / 2, (r[1] + r[3]) / 2
    placed = []
    if paths:
        # losetas de 2 m (pivote en su esquina +x, -y como las de piso): una fila este-oeste y una norte-sur
        tile = 2.0
        for k in range(int((r[2] - r[0]) // tile)):
            x0 = r[0] + k * tile
            placed.append(scene.add('Ground_Path_2x2', x0 + tile, cy - tile / 2, 0.0, z=0.02,
                                    zone=zone, role='path'))
        for k in range(int((r[3] - r[1]) // tile)):
            y0 = r[1] + k * tile
            if y0 + tile <= cy - tile / 2 or y0 >= cy + tile / 2:
                placed.append(scene.add('Ground_Path_2x2', cx + tile / 2, y0, 0.0, z=0.02, zone=zone, role='path'))
    # nada encima de los caminos
    lanes = [(r[0], cy - 1.4, r[2], cy + 1.4), (cx - 1.4, r[1], cx + 1.4, r[3])]
    placed += scatter(scene, catalog, ['CommonTree_1', 'CommonTree_2', 'CommonTree_3', 'CommonTree_5', 'Pine_1', 'Pine_2'], zone,
                      trees, seed, gap=2.5, role='tree', scale=(0.8, 1.0), blockers=lanes)
    placed += scatter(scene, catalog, ['Bush_Common', 'Bush_Common_Flowers'], zone, bushes, seed + 1, gap=0.4, role='bush', blockers=lanes)
    placed += scatter(scene, catalog, ['Grass_Common_Tall', 'Grass_Wispy_Tall', 'Grass_Common_Short', 'Flower_3_Group', 'Flower_4_Group'],
                      zone, grass, seed + 2, role='grass', blockers=lanes)
    placed += scatter(scene, catalog, ['Rock_Medium_1', 'Rock_Medium_2', 'Rock_Medium_3'], zone, rocks, seed + 3, gap=1.0, role='rock',
                      blockers=lanes)
    return placed


def town_sign(scene, catalog, side=None, seed=1):
    """El letrero del pueblo en una entrada (una salida de calle): en la banqueta, del lado derecho de quien entra, mirando
    hacia afuera. 'side' (n, s, e, w) elige la salida; si no, una al azar."""
    rng = random.Random(seed)
    exits = [(name, z['rect']) for name, z in scene.data['zones'].items() if z['kind'] == 'spawn' and (not side or name.split('_')[1] == side)]
    if not exits:
        return None
    name, r = rng.choice(sorted(exits))
    direction = {'n': (0, 1), 's': (0, -1), 'e': (1, 0), 'w': (-1, 0)}[name.split('_')[1]]
    cx, cy = (r[0] + r[2]) / 2, (r[1] + r[3]) / 2
    # hacia adentro del pueblo, a la derecha de quien entra (entrar es ir contra 'direction')
    inward = (-direction[0], -direction[1])
    right = (inward[1], -inward[0])
    x, y = cx + inward[0] * 10.0 + right[0] * 8.6, cy + inward[1] * 10.0 + right[1] * 8.6
    yaw = {(0, 1): 180.0, (0, -1): 0.0, (1, 0): 90.0, (-1, 0): 270.0}[direction]
    boxes = solid_boxes(scene, catalog)
    for step in range(12):
        e = {'asset': 'TownSign', 'x': x + inward[0] * step, 'y': y + inward[1] * step, 'yaw': yaw, 'scale': 1.0}
        if not overlaps(box_of(footprint(catalog, e)), boxes, 0.2):
            return scene.add('TownSign', e['x'], e['y'], yaw, zone='sidewalks', role='town_sign')
    return None


# --- edificios ---

def lot(scene, catalog, asset, zone, side, offset=0.0, role=None, entity_id=None, gap=0.4):
    """Un edificio en un lado de una zona (manzana o franja del borde), con el frente en el borde de ese lado mirando hacia
    afuera (S = el borde sur, mirando al sur). 'offset' son los metros desde el inicio del lado (S y N desde el oeste, E y W
    desde el sur); si choca con algo se corre hasta el primer lugar libre. Devuelve la entidad o None si no cabe."""
    model = catalog['models'][asset]
    yaw = SIDE_YAW[side]
    ax0, ay0, ax1, ay1 = building_front_corner(model['low'], model['high'], yaw)
    r = scene.zone_rect(zone)
    boxes = solid_boxes(scene, catalog)
    length = (r[2] - r[0]) if side in 'SN' else (r[3] - r[1])
    while offset < length:
        if side == 'S':
            x, y = r[0] + offset - ax0, r[1] - ay0
        elif side == 'N':
            x, y = r[0] + offset - ax0, r[3] - ay1
        elif side == 'E':
            x, y = r[2] - ax1, r[1] + offset - ay0
        else:
            x, y = r[0] - ax0, r[1] + offset - ay0
        box = (x + ax0, y + ay0, x + ax1, y + ay1)
        if inside(box, r) and not overlaps(box, boxes, gap):
            return scene.add(asset, x, y, yaw, zone=zone, role=role or 'building', entity_id=entity_id, tags=['building'])
        offset += 0.5
    return None


def fill_side(scene, catalog, assets, zone, side, roles=None, seed=1):
    """Llena un lado de una zona de edificios, uno tras otro (los modelos y roles se van turnando)."""
    rng = random.Random(seed)
    placed = []
    offset = 0.0
    r = scene.zone_rect(zone)
    along = (lambda b: b[0] - r[0]) if side in 'SN' else (lambda b: b[1] - r[1])
    while True:
        k = len(placed)
        role = roles[k % len(roles)] if roles else None
        # el modelo que toca (al azar), o si no cabe, el que quede más cerca de donde iba el siguiente
        order = assets[:] if seed is None else rng.sample(assets, len(assets))
        best = None
        for asset in order:
            entity = lot(scene, catalog, asset, zone, side, offset, role)
            if entity is None:
                continue
            start = along(box_of(footprint(catalog, entity)))
            scene.remove(lambda e: e is entity)
            if best is None or start < best[1] - 0.5:
                best = (asset, start)
        if best is None:
            return placed
        entity = lot(scene, catalog, best[0], zone, side, offset, role)
        placed.append(entity)
        box = box_of(footprint(catalog, entity))
        offset = ((box[2] - r[0]) if side in 'SN' else (box[3] - r[1])) + 0.8


def houses(scene, catalog, zone, sides='NSEW', seed=1, designs=6, gap=2.0):
    """Casas del generador de edificios (buildings.py) en los lados de una zona, con el frente (la puerta) hacia afuera y un
    patio entre una y otra. Hace 'designs' diseños distintos (planta, pisos, estilo) y los va turnando; los modelos que
    falten se hacen con Blender y entran al catálogo. Devuelve las casas puestas."""
    import buildings
    import catalog as catalog_module
    rng = random.Random(seed)
    options = [(6, 8), (6, 6), (8, 8), (8, 10), (4, 6), (6, 10)]
    recipes = []
    for k in range(designs):
        w, d = rng.choice(options)
        recipes.append(buildings.recipe(w, d, rng.choice((1, 1, 2)), rng.choice(('plaster', 'mixed', 'brick', 'plaster')),
                                        rng.randint(1, 1000)))
    names = buildings.build(recipes)
    if any(n not in catalog['models'] for n in names):
        catalog.clear()
        catalog.update(catalog_module.build_catalog())
    placed = []
    for side in sides:
        r = scene.zone_rect(zone)
        length = (r[2] - r[0]) if side in 'SN' else (r[3] - r[1])
        offset = 1.0
        while offset < length:
            name = rng.choice(names)
            e = lot(scene, catalog, name, zone, side, offset, role='house', gap=gap)
            if e is None:
                break
            placed.append(e)
            box = box_of(footprint(catalog, e))
            offset = ((box[2] - r[0]) if side in 'SN' else (box[3] - r[1])) + gap
    for e in list(placed):
        placed += flower_beds(scene, catalog, e, zone, rng)
    return placed


def offices(scene, catalog, zone, sides='NSEW', seed=1, designs=6, gap=0.4, stories=(3, 4)):
    """Oficinas del generador de edificios (buildings.py, piezas de Downtown) en los lados de una zona, con el frente (locales
    y puerta) hacia la calle, casi pegadas: una fachada continua de centro. Hace 'designs' diseños (planta, pisos, estilo)
    y los va turnando; los que falten se hacen con Blender y entran al catálogo. Devuelve los edificios puestos."""
    import buildings
    import catalog as catalog_module
    rng = random.Random(seed)
    sizes = [(8, 8), (8, 12), (12, 8), (12, 12), (16, 12), (12, 16), (16, 16)]
    recipes = []
    for k in range(designs):
        w, d = rng.choice(sizes)
        recipes.append(buildings.office_recipe(w, d, rng.randint(*stories), rng.choice(('brick', 'brick', 'trim', 'metal')),
                                               rng.randint(1, 1000)))
    names = buildings.build(recipes)
    if any(n not in catalog['models'] for n in names):
        catalog.clear()
        catalog.update(catalog_module.build_catalog())
    placed = []
    for side in sides:
        r = scene.zone_rect(zone)
        length = (r[2] - r[0]) if side in 'SN' else (r[3] - r[1])
        offset = 1.0
        misses = 0
        while offset < length and misses < len(names):
            name = names[(len(placed) + misses) % len(names)]
            e = lot(scene, catalog, name, zone, side, offset, role='office', gap=gap)
            if e is None:
                misses += 1   # no cabe: se prueba el siguiente diseño (uno más angosto puede cerrar la fila)
                continue
            misses = 0
            placed.append(e)
            box = box_of(footprint(catalog, e))
            offset = ((box[2] - r[0]) if side in 'SN' else (box[3] - r[1])) + gap
    return placed


# Los árboles que se mezclan en banquetas, plazas y patios: los comunes y algunos pinos (los torcidos miden 16 m y pesan
# 10 mil triángulos; los secos, para algo más lúgubre, se piden aparte)
TREES_MIX = ['CommonTree_1', 'CommonTree_2', 'CommonTree_3', 'CommonTree_4', 'CommonTree_5', 'Pine_1', 'Pine_3', 'Pine_5']


def _refresh(catalog, names):
    import catalog as catalog_module
    if any(n not in catalog['models'] for n in names):
        catalog.clear()
        catalog.update(catalog_module.build_catalog())


def row_of(scene, catalog, zone, sides, names, role, gap=0.4, start=1.0):
    """Edificios uno tras otro en los lados de una zona, turnando los modelos (si uno no cabe se prueba el siguiente):
    una fachada continua. Devuelve los puestos."""
    placed = []
    for side in sides:
        r = scene.zone_rect(zone)
        length = (r[2] - r[0]) if side in 'SN' else (r[3] - r[1])
        offset, misses = start, 0
        while offset < length and misses < len(names):
            e = lot(scene, catalog, names[(len(placed) + misses) % len(names)], zone, side, offset, role=role, gap=gap)
            if e is None:
                misses += 1
                continue
            misses = 0
            placed.append(e)
            box = box_of(footprint(catalog, e))
            offset = ((box[2] - r[0]) if side in 'SN' else (box[3] - r[1])) + gap
    return placed


def mixed(scene, catalog, zone, sides='NSEW', seed=1, courtyard=True):
    """Una manzana de centro histórico: casas de dos pisos (teja) y oficinas de tres o cuatro pisos, turnándose, casi
    pegadas en los lados; adentro un patio de pasto con algunos árboles."""
    import buildings
    rng = random.Random(seed)
    houses_r = [buildings.recipe(*rng.choice([(6, 8), (8, 8), (6, 10), (8, 10)]), 2, rng.choice(('plaster', 'mixed', 'brick')),
                                 rng.randint(1, 1000)) for _ in range(3)]
    offices_r = [buildings.office_recipe(*rng.choice([(8, 8), (8, 12), (12, 8), (12, 12)]), rng.randint(3, 4),
                                         rng.choice(('brick', 'brick', 'trim')), rng.randint(1, 1000)) for _ in range(3)]
    house_names, office_names = buildings.build(houses_r), buildings.build(offices_r)
    names = [n for pair in zip(house_names, office_names) for n in pair]
    _refresh(catalog, names)
    placed = row_of(scene, catalog, zone, sides, names, 'building')
    if courtyard:
        set_ground(scene, catalog, zone, 'Ground_Grass_9x9')
        placed += scatter(scene, catalog, TREES_MIX, zone, 4, seed, gap=1.5, scale=(0.8, 1.0))
    return placed


def plaza(scene, catalog, zone, seed=1):
    """La plaza mayor: piso de losas, el kiosco (arcos con techo de teja) al centro con macetones alrededor, una hilera de
    árboles por dentro de la orilla y farolas en las esquinas."""
    import buildings
    rng = random.Random(seed)
    scene.data['zones'][zone]['kind'] = 'plaza'
    set_ground(scene, catalog, zone, rules.GROUND['plaza'][0])
    r = scene.zone_rect(zone)
    cx, cy = (r[0] + r[2]) / 2, (r[1] + r[3]) / 2
    kiosk = buildings.build([buildings.recipe(4, 4, 1, 'plaster', seed, shutters=False, chimney=False, arcade='all')])[0]
    _refresh(catalog, [kiosk])
    placed = [scene.add(kiosk, cx, cy, 0.0, zone=zone, role='landmark', tags=['building'])]
    for dx, dy in ((-6, -6), (6, -6), (6, 6), (-6, 6)):
        placed.append(scene.add('Prop_Planter_Single', cx + dx, cy + dy, 0.0, zone=zone, role='clutter'))
    inset, spacing = 3.5, 7.0
    # (las esquinas quedan para las farolas)
    for x0, y0, x1, y1 in ((r[0] + inset + 3.5, r[1] + inset, r[2] - inset - 3.5, r[1] + inset),
                           (r[0] + inset + 3.5, r[3] - inset, r[2] - inset - 3.5, r[3] - inset),
                           (r[0] + inset, r[1] + inset + spacing, r[0] + inset, r[3] - inset - spacing),
                           (r[2] - inset, r[1] + inset + spacing, r[2] - inset, r[3] - inset - spacing)):
        n = max(1, int(math.hypot(x1 - x0, y1 - y0) // spacing))
        for k in range(n + 1):
            t = k / n
            placed.append(scene.add(rng.choice(TREES_MIX), x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, rng.uniform(0, 360),
                                    zone=zone, role='tree', scale=round(rng.uniform(0.8, 1.0), 2)))
    for x, y in ((r[0] + 1.5, r[1] + 1.5), (r[2] - 1.5, r[1] + 1.5), (r[2] - 1.5, r[3] - 1.5), (r[0] + 1.5, r[3] - 1.5)):
        yaw = math.degrees(math.atan2(cy - y, cx - x)) + 90.0
        placed.append(scene.add('StreetLights', x, y, yaw, zone=zone, role='street_light'))
    return placed


def church(scene, catalog, zone, side, seed=1):
    """La iglesia en el lado de una manzana que da a la plaza: la nave (8 x 14, dos pisos de altura, la puerta y el hastial al
    frente) y el campanario (4 x 4, cuatro pisos, el último de arcos, techo de torre) a su izquierda. Devuelve las dos."""
    import buildings
    nave, tower = buildings.build([buildings.recipe(8, 14, 2, 'brick', seed, shutters=False, chimney=False)]) + \
        buildings.build([buildings.recipe(4, 4, 4, 'brick', seed, shutters=False, chimney=False, roof='tower')])
    _refresh(catalog, [nave, tower])
    r = scene.zone_rect(zone)
    length = (r[2] - r[0]) if side in 'SN' else (r[3] - r[1])
    start = (length - 12.4) / 2
    placed = []
    for name, offset, role in ((tower, start, 'landmark'), (nave, start + 4.4, 'church')):
        e = lot(scene, catalog, name, zone, side, offset, role=role, gap=0.2)
        if e:
            placed.append(e)
    return placed


def palace(scene, catalog, zone, side, seed=1):
    """El palacio de gobierno: dos edificios de piedra con molduras, de tres pisos, juntos en el lado que da a la plaza."""
    import buildings
    names = buildings.build([buildings.office_recipe(16, 12, 3, 'trim', seed), buildings.office_recipe(12, 12, 3, 'trim', seed + 1)])
    _refresh(catalog, names)
    return row_of(scene, catalog, zone, side, names, 'city_hall', gap=0.2, start=2.0)


def portales(scene, catalog, zone, side, seed=1):
    """Casas con portal (arcos en la planta baja del frente) en el lado que da a la plaza: los portales de los centros."""
    import buildings
    rng = random.Random(seed)
    names = buildings.build([buildings.recipe(*size, 2, rng.choice(('plaster', 'mixed')), rng.randint(1, 1000), chimney=False,
                                              arcade='front') for size in ((8, 8), (6, 8), (8, 10))])
    _refresh(catalog, names)
    return row_of(scene, catalog, zone, side, names, 'shop', gap=0.0, start=1.0)


def flower_beds(scene, catalog, building, zone, rng, count=6):
    """Flores y arbustos con flores alrededor de una casa (contra sus paredes, en el patio), sin tapar caminos ni la puerta."""
    x0, y0, x1, y1 = box_of(footprint(catalog, building))
    r = scene.zone_rect(zone)
    boxes = solid_boxes(scene, catalog, skip=building)
    assets = ['Flower_3_Group', 'Flower_4_Group', 'Flower_3_Group', 'Bush_Common_Flowers']
    placed = []
    for _ in range(count * 20):
        if len(placed) >= count:
            break
        side = rng.randrange(4)
        t = rng.uniform(0.15, 0.85)
        d = rng.uniform(0.4, 1.3)   # qué tan lejos de la pared
        x, y = ((x0 + (x1 - x0) * t, y0 - d), (x1 + d, y0 + (y1 - y0) * t), (x0 + (x1 - x0) * t, y1 + d), (x0 - d, y0 + (y1 - y0) * t))[side]
        spot = (x - 0.5, y - 0.5, x + 0.5, y + 0.5)
        if not inside(spot, r) or overlaps(spot, boxes, 0.1):
            continue
        asset = rng.choice(assets)
        placed.append(scene.add(asset, x, y, rng.uniform(0, 360), round(rng.uniform(0.8, 1.1), 2), zone=zone,
                                role='bush' if asset.startswith('Bush') else 'garden'))
        boxes.append(spot)
    return placed


# --- dispersar, filas, autos chocados ---

# Grupos de cosas regadas en calles y plazas (lo que deja una noche de zombis): (nombre, [(modelos, cuántos)], peso)
CLUTTER_GROUPS = [
    ('barricade', [(['TrafficBarrier_1', 'TrafficBarrier_2', 'PlasticBarrier'], 2), (['TrafficCone_1', 'TrafficCone_2'], 3)], 3),
    ('trash', [(['TrashBag_1', 'TrashBag_2'], 3), (['Barrel'], 1), (['Pallet', 'Pallet_Broken'], 1)], 4),
    ('barrels', [(['Barrel'], 3)], 3),
    ('junk', [(['Couch', 'Wheels_Stack', 'Pipes', 'CinderBlock'], 2), (['TrashBag_1', 'TrashBag_2'], 2)], 2),
    ('crates', [(['Prop_Crate'], 2), (['Barrel', 'Pallet'], 1)], 2),
    ('wheels', [(['Wheel', 'Wheels_Stack'], 2), (['TrashBag_1', 'CinderBlock', 'Pipes'], 2)], 2),
    ('container', [(['Container_Red', 'Container_Green'], 1), (['Pallet', 'Pallet_Broken', 'Barrel'], 2)], 1),
]
BLOOD = ['Blood_1', 'Blood_2', 'Blood_3']


# Los modos que se juegan en una parte del pueblo cerrada con barricadas (los demás recorren el pueblo entero)
FENCED_MODES = ('survival', 'holdout', 'defend')


def action_area(scene, mission_id):
    """Dónde se juega una misión de pelea: una ventana de 2 x 2 manzanas de la cuadrícula, elegida con el id de la
    misión (cada misión en otro sector). None cuando el pueblo no es más grande que eso (se juega todo)."""
    import zlib
    grid = scene.data.get('grid')
    if not grid:
        return None
    xs, ys = grid['xs'], grid['ys']
    if len(xs) < 4 and len(ys) < 4:
        return None
    windows = [(i, j) for i in range(max(1, len(xs) - 2)) for j in range(max(1, len(ys) - 2))]
    i, j = windows[zlib.crc32(f'{scene.name}/{mission_id}'.encode()) % len(windows)]
    i1, j1 = min(i + 2, len(xs) - 1), min(j + 2, len(ys) - 1)
    # la orilla (paredes invisibles, export.py) pasa detrás de donde puede quedar la barricada más lejana (cordon: hasta
    # 9.5 m pasando el cruce, y el grueso del contenedor): lo primero con que el jugador topa es la barricada
    edge = HALF_CROSSING + 11.0
    return {'cols': [i, i1], 'rows': [j, j1],
            'rect': [round(xs[i] - edge, 2), round(ys[j] - edge, 2), round(xs[i1] + edge, 2), round(ys[j1] + edge, 2)]}


def cordon(scene, catalog, area, gates=False):
    """Cierra con barricadas (contenedores atravesados, como las salidas del pueblo) cada calle que sale del área, justo
    pasando el cruce; los zombis entran por ahí: una zona de aparición adentro de cada barricada. Con 'gates' (defender) el
    contenedor de en medio es una puerta de tres vallas que los zombis rompen y el jugador repara (rol 'gate'), y los zombis
    aparecen afuera, entre la puerta y la orilla del área. Devuelve (puestos, rectángulos de aparición)."""
    xs, ys = scene.data['grid']['xs'], scene.data['grid']['ys']
    (i0, i1), (j0, j1) = area['cols'], area['rows']
    placed, spawns = [], []
    boxes = solid_boxes(scene, catalog)
    for i in range(i0, i1 + 1):
        for j in range(j0, j1 + 1):
            for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                ni, nj = i + di, j + dj
                inside = i0 <= ni <= i1 and j0 <= nj <= j1
                if inside or not (0 <= ni < len(xs) and 0 <= nj < len(ys)):
                    continue   # adentro, o la orilla del pueblo (sus salidas ya están cerradas)
                x, y = xs[i], ys[j]
                across = (0, 1) if di else (1, 0)
                yaw = 90.0 if di else 0.0
                def fence_at(bx, by):
                    pieces = [{'asset': 'Container_Green', 'x': bx + across[0] * k * 5.9, 'y': by + across[1] * k * 5.9,
                               'yaw': yaw, 'scale': 1.0, 'role': 'barricade'} for k in (-1, 1)]
                    if gates:
                        # la puerta: tres vallas con huecos más angostos que un zombi
                        pieces += [{'asset': 'TrafficBarrier_1', 'x': bx + across[0] * k * 2.0, 'y': by + across[1] * k * 2.0,
                                    'yaw': yaw, 'scale': 1.0, 'role': 'gate'} for k in (-1, 0, 1)]
                    else:
                        pieces.append({'asset': 'Container_Red', 'x': bx, 'y': by, 'yaw': yaw, 'scale': 1.0, 'role': 'barricade'})
                    return pieces
                # justo pasando el cruce, o más allá por la calle si ahí hay algo (basura, una farola); una puerta, cerca del
                # cruce (afuera tiene que quedar lugar para que aparezcan)
                reaches = (1.8, 3.5) if gates else (1.8, 3.5, 5.5, 7.5, 9.5)
                for reach in reaches:
                    fence = fence_at(x + di * (HALF_CROSSING + reach), y + dj * (HALF_CROSSING + reach))
                    if not any(overlaps(box_of(footprint(catalog, e)), boxes, 0.05) for e in fence):
                        break
                else:
                    # no cupo en ningún lugar: justo pasando el cruce, y fuera la basura suelta, el semáforo o el hidrante que estorbe
                    reach = 1.8
                    fence = fence_at(x + di * (HALF_CROSSING + reach), y + dj * (HALF_CROSSING + reach))
                    walls = [box_of(footprint(catalog, e)) for e in fence]
                    in_way = lambda e: (e['x'] - 0.3, e['y'] - 0.3, e['x'] + 0.3, e['y'] + 0.3) if e.get('role') == 'traffic_light' else box_of(footprint(catalog, e))
                    scene.remove(lambda e: e.get('role') in ('clutter', 'traffic_light', 'hydrant') and overlaps(in_way(e), walls, 0.05))
                    boxes = solid_boxes(scene, catalog)
                for e in fence:
                    placed.append(scene.add(e['asset'], e['x'], e['y'], yaw, role=e['role'], zone='streets'))
                    boxes.append(box_of(footprint(catalog, e)))
                if gates:
                    # afuera: de pasando la puerta hasta antes de la orilla del área (HALF_CROSSING + 11)
                    near, far = HALF_CROSSING + reach + 2.8, HALF_CROSSING + 10.0
                    mid, half = (near + far) / 2, max(1.0, (far - near) / 2)
                else:
                    # los zombis brincan la barricada: aparecen justo adentro, sobre el paso de cebra
                    mid, half = HALF_CROSSING - 2.0, 1.0
                sx, sy = x + di * mid, y + dj * mid
                spawns.append([round(sx - (half if di else 5.0), 2), round(sy - (5.0 if di else half), 2),
                               round(sx + (half if di else 5.0), 2), round(sy + (5.0 if di else half), 2)])
    return placed, spawns


def mission_clutter(scene, catalog, mission_id, groups=14, wrecks=3, mode=None):
    """La capa propia de una misión (<id>_props): cosas regadas y carambolas (clutter), con una semilla sacada de su id
    (cada misión distinta, y la misma si se vuelve a hacer). Una misión de pelea (FENCED_MODES) en un pueblo grande se juega
    en su área (action_area): barricadas en sus calles de salida (cordon) y todo lo regado adentro. Devuelve (capa, área o
    None, rectángulos de aparición)."""
    import zlib
    layer = f'{mission_id}_props'
    scene.remove(lambda e: e.get('layer') == layer)
    area = action_area(scene, mission_id) if mode in FENCED_MODES else None
    spawns = []
    placed = []
    if area:
        fence, spawns = cordon(scene, catalog, area, gates=mode == 'defend')
        placed += fence
    if mode == 'defend':
        # lo que se defiende: la camioneta blindada de los sobrevivientes en el cruce del centro del área (o del pueblo)
        xs, ys = scene.data['grid']['xs'], scene.data['grid']['ys']
        ci = area['cols'][0] + 1 if area else len(xs) // 2
        cj = area['rows'][0] + 1 if area else len(ys) // 2
        placed.append(scene.add('Vehicle_Truck_Armored', xs[ci], ys[cj], 45.0, role='objective', zone='streets'))
    seed = zlib.crc32(f'{scene.name}/{mission_id}'.encode()) % 100000
    placed += clutter(scene, catalog, groups, seed, wrecks=wrecks, within=area['rect'] if area else None)
    placed += loot(scene, catalog, seed, within=area['rect'] if area else None)
    for e in placed:
        e['layer'] = layer
    return layer, area, spawns


def loot(scene, catalog, seed=1, count=2, within=None):
    """Cofres de botín (el Chest del kit, rol 'loot'): se rompen fácil y siempre sueltan salud o un power-up (export.py). En
    lugares libres de calles, plazas y estacionamientos, dentro de 'within' si se da."""
    rng = random.Random(seed + 77)
    zones = [name for name, z in scene.data['zones'].items() if z['kind'] in ('street', 'plaza', 'lot')]
    placed = []
    for _ in range(count * 12):
        if len(placed) >= count or not zones:
            break
        got = scatter(scene, catalog, ['Chest'], rng.choice(zones), 1, rng.randint(1, 100000), gap=1.0, role='loot')
        for e in got:
            if within and not (within[0] + 2 <= e['x'] <= within[2] - 2 and within[1] + 2 <= e['y'] <= within[3] - 2):
                scene.remove(lambda x, e=e: x is e)
            else:
                placed.append(e)
    return placed


def clutter(scene, catalog, groups=12, seed=1, kinds=('street', 'plaza', 'lot'), wrecks=3, within=None):
    """Grupos de cosas regadas (barricadas con conos, basura, barriles, chatarra, cajas, llantas, contenedores; manchas de
    sangre en el piso) en calles, plazas y estacionamientos, y 'wrecks' carambolas de autos chocados en las calles, en
    lugares libres: lo que una misión agrega a su capa para que cada una se vea distinta y tenga cobertura. Lo que se rompe
    (barriles, basura, conos, autos) suelta dinero, salud o algún power-up (export.py)."""
    rng = random.Random(seed)
    placed = []

    def clip(r):
        # 'within': solo la parte de cada zona que cae en ese rectángulo (el área de la misión)
        if not within:
            return r
        c = [max(r[0], within[0]), max(r[1], within[1]), min(r[2], within[2]), min(r[3], within[3])]
        return c if c[2] - c[0] > 4.0 and c[3] - c[1] > 4.0 else None

    streets = [name for name, z in scene.data['zones'].items() if z['kind'] == 'street' and clip(z['rect'])]
    for _ in range(wrecks if streets else 0):
        zone = rng.choice(streets)
        r = clip(scene.zone_rect(zone))
        if r[2] - r[0] < 9.0 or r[3] - r[1] < 9.0:
            continue
        placed += wreck(scene, catalog, rng.uniform(r[0] + 4, r[2] - 4), rng.uniform(r[1] + 4, r[3] - 4), rng.randint(2, 4),
                        rng.randint(1, 10000), zone)
    zones = [name for name, z in scene.data['zones'].items() if z['kind'] in kinds and clip(z['rect'])]
    if not zones:
        return placed
    boxes = solid_boxes(scene, catalog)
    weights = [w for _, _, w in CLUTTER_GROUPS]
    done = 0
    for _ in range(groups * 20):
        if done >= groups:
            break
        zone = rng.choice(zones)
        r = clip(scene.zone_rect(zone))
        cx, cy = rng.uniform(r[0] + 1.5, r[2] - 1.5), rng.uniform(r[1] + 1.5, r[3] - 1.5)
        if overlaps((cx - 2.0, cy - 2.0, cx + 2.0, cy + 2.0), boxes, 0.5):
            continue   # el grupo necesita su lugar
        _, members, _ = rng.choices(CLUTTER_GROUPS, weights)[0]
        line = rng.uniform(0, 180)
        k = 0
        done += 1
        if rng.random() < 0.5:   # una mancha de sangre bajo el grupo (se pisa: no estorba)
            placed.append(scene.add(rng.choice(BLOOD), cx + rng.uniform(-1, 1), cy + rng.uniform(-1, 1), rng.uniform(0, 360),
                                    round(rng.uniform(0.8, 1.3), 2), zone=zone, role='blood'))
        for assets, count in members:
            for _ in range(count):
                asset = rng.choice(assets)
                if asset not in catalog['models']:
                    continue
                # en hilera (barricadas) o amontonados alrededor del centro
                along = (k - 2) * 1.6
                x = cx + along * math.cos(math.radians(line)) + rng.uniform(-0.5, 0.5)
                y = cy + along * math.sin(math.radians(line)) + rng.uniform(-0.5, 0.5)
                k += 1
                e = {'asset': asset, 'x': x, 'y': y, 'yaw': line + rng.uniform(-20, 20), 'scale': 1.0}
                box = box_of(footprint(catalog, e))
                if not inside(box, r) or overlaps(box, boxes, 0.2):
                    continue
                roles = catalog['models'][asset]['roles']
                placed.append(scene.add(asset, x, y, e['yaw'], 1.0, zone=zone, role=roles[0] if roles else 'clutter'))
                boxes.append(box)
    return placed


def scatter(scene, catalog, assets, zone, count, seed=1, gap=0.5, role=None, scale=(1.0, 1.0), blockers=()):
    """'count' modelos (al azar de 'assets') en lugares libres de una zona, girados al azar."""
    rng = random.Random(seed)
    r = scene.zone_rect(zone)
    boxes = solid_boxes(scene, catalog)
    placed = []
    for _ in range(count * 40):
        if len(placed) >= count:
            break
        asset = rng.choice(assets)
        s = round(rng.uniform(*scale), 2)
        e = {'asset': asset, 'x': rng.uniform(r[0], r[2]), 'y': rng.uniform(r[1], r[3]), 'yaw': rng.uniform(0, 360), 'scale': s}
        k = kind(catalog, asset)
        box = (e['x'] - 0.6, e['y'] - 0.6, e['x'] + 0.6, e['y'] + 0.6) if k == 'tree' else box_of(footprint(catalog, e))
        if not inside(box, r) or (k != 'ground' and overlaps(box, boxes, gap)) or overlaps(box, list(blockers)):
            continue
        roles = catalog['models'][asset]['roles']
        placed.append(scene.add(asset, e['x'], e['y'], e['yaw'], s, zone=zone, role=role or (roles[0] if roles else None)))
        if k != 'ground':
            boxes.append(box)
    return placed


def row(scene, catalog, asset, start, end, spacing, yaw=None, role=None, zone=None, skip_blocked=True):
    """Una fila de modelos de 'start' a 'end' (metros), uno cada 'spacing' (cercas de barreras, postes, árboles de calle)."""
    (x0, y0), (x1, y1) = start, end
    length = math.hypot(x1 - x0, y1 - y0)
    if yaw is None:
        yaw = (math.degrees(math.atan2(y1 - y0, x1 - x0)) + 90.0) % 360.0   # el largo del modelo (su x) a lo largo de la fila
    boxes = solid_boxes(scene, catalog)
    placed = []
    n = int(length // spacing) + 1
    for k in range(n):
        t = k * spacing / max(length, 0.001)
        e = {'asset': asset, 'x': x0 + (x1 - x0) * t, 'y': y0 + (y1 - y0) * t, 'yaw': yaw, 'scale': 1.0}
        box = box_of(footprint(catalog, e))
        if skip_blocked and kind(catalog, asset) != 'ground' and overlaps(box, boxes, 0.1):
            continue
        roles = catalog['models'][asset]['roles']
        placed.append(scene.add(asset, e['x'], e['y'], yaw, zone=zone, role=role or (roles[0] if roles else None)))
        boxes.append(box)
    return placed


def wreck(scene, catalog, x, y, count=4, seed=1, zone=None):
    """Autos chocados creíbles: uno al centro y los demás pegados a él en ángulos raros, como una carambola, con conos y
    una barrera alrededor."""
    rng = random.Random(seed)
    cars = ['Vehicle_Pickup', 'Vehicle_Sports', 'Vehicle_Truck', 'Vehicle_Sports_Armored', 'Vehicle_Pickup_Armored']
    boxes = solid_boxes(scene, catalog)
    own = []   # sus autos: entre ellos se tocan (la huella recortada), con lo demás no
    area = scene.zone_rect(zone) if zone in scene.data['zones'] else None   # la carambola no se sale de su calle
    placed = []
    heading = rng.uniform(0, 360)
    cx, cy = x, y
    for k in range(count):
        for _ in range(30):
            asset = rng.choice(cars)
            yaw = heading + rng.uniform(-70, 70) + (180 if rng.random() < 0.3 else 0)
            e = {'asset': asset, 'x': cx, 'y': cy, 'yaw': yaw, 'scale': 1.0}
            box = box_of(footprint(catalog, e, 0.3))
            full = box_of(footprint(catalog, e))
            if not overlaps(full, boxes, 0.0) and not overlaps(box, own, 0.0) and (area is None or inside(box, area)):
                placed.append(scene.add(asset, cx, cy, yaw, zone=zone, role='crashed_car', tags=['breakable']))
                own.append(box)
                break
            angle = math.radians(heading + rng.uniform(-60, 60))
            cx, cy = cx + math.cos(angle) * 1.2, cy + math.sin(angle) * 1.2
        angle = math.radians(heading + rng.uniform(-50, 50))
        step = rng.uniform(3.0, 4.2)
        cx, cy = cx + math.cos(angle) * step, cy + math.sin(angle) * step
    for _ in range(rng.randint(3, 6)):
        a = rng.uniform(0, 2 * math.pi)
        d = rng.uniform(4.5, 7.0)
        e = {'asset': 'TrafficCone_1', 'x': x + math.cos(a) * d, 'y': y + math.sin(a) * d, 'yaw': rng.uniform(0, 360), 'scale': 1.0}
        box = box_of(footprint(catalog, e))
        if not overlaps(box, boxes + own, 0.1):
            placed.append(scene.add('TrafficCone_1', e['x'], e['y'], e['yaw'], zone=zone, role='barrier'))
            boxes.append(box)   # el siguiente cono no se le encima
    return placed


# --- cambiar lo que hay ---

def replace(scene, catalog, entities, asset):
    """Cambia el modelo de esas entidades (conservan id, lugar, giro, zona y rol)."""
    if asset not in catalog['models']:
        raise ValueError(f'{asset} no está en el catálogo')
    for e in entities:
        e['asset'] = asset
    return len(entities)


def move(entity, x=None, y=None, yaw=None):
    if x is not None:
        entity['x'] = round(x, 3)
    if y is not None:
        entity['y'] = round(y, 3)
    if yaw is not None:
        entity['yaw'] = round(yaw % 360.0, 2)


def validate(scene, catalog):
    """Problemas de la escena, como texto: modelos desconocidos, cosas fuera de los límites o de su zona, sólidos encimados."""
    problems = []
    bounds = scene.data['bounds']
    solids = []
    for e in scene.entities:
        if e['asset'] not in catalog['models']:
            problems.append(f'{e["id"]}: el modelo {e["asset"]} no está en el catálogo')
            continue
        k = kind(catalog, e['asset'])
        # un árbol ocupa su tronco (la copa puede pasar sobre lo de al lado), un poste su poste (el brazo va por arriba)
        pole = 'pole' in catalog['models'][e['asset']]['tags']
        box = (e['x'] - 0.4, e['y'] - 0.4, e['x'] + 0.4, e['y'] + 0.4) if k == 'tree' or pole else box_of(footprint(catalog, e))
        center = ((box[0] + box[2]) / 2, (box[1] + box[3]) / 2)
        if not (bounds[0] <= center[0] <= bounds[2] and bounds[1] <= center[1] <= bounds[3]):
            problems.append(f'{e["id"]}: fuera de los límites de la escena')
        zone = scene.data['zones'].get(e.get('zone') or '')
        if zone and k != 'ground' and zone['kind'] != 'street' and not inside(box, [zone['rect'][0] - 1, zone['rect'][1] - 1, zone['rect'][2] + 1, zone['rect'][3] + 1]):
            problems.append(f'{e["id"]}: se sale de su zona {e["zone"]}')
        if k == 'solid':
            solids.append((e, box))
        elif k == 'tree':
            solids.append((e, (e['x'] - 0.4, e['y'] - 0.4, e['x'] + 0.4, e['y'] + 0.4)))
    for i in range(len(solids)):
        for j in range(i + 1, len(solids)):
            a, b = solids[i][1], solids[j][1]
            ix = min(a[2], b[2]) - max(a[0], b[0])
            iy = min(a[3], b[3]) - max(a[1], b[1])
            crash = solids[i][0].get('role') == solids[j][0].get('role') == 'crashed_car'   # una carambola, a propósito
            la, lb = solids[i][0].get('layer', BASE), solids[j][0].get('layer', BASE)
            together = la == lb or la in [BASE] + DEFAULT_LAYERS or lb in [BASE] + DEFAULT_LAYERS   # capas de misiones distintas no
            if ix > 0.3 and iy > 0.3 and not crash and together:
                problems.append(f'{solids[i][0]["id"]} y {solids[j][0]["id"]} se enciman ({ix:.1f} x {iy:.1f} m)')
    if not any(z['kind'] == 'spawn' for z in scene.data['zones'].values()):
        problems.append('no hay zonas spawn: los zombis no tienen por dónde entrar')
    return problems + rules.check(scene, catalog)
