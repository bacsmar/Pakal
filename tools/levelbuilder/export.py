"""De escena a nivel jugable de Pakal (level.json en Assets/zombies/lb_<nombre>/) y a .glb con vistas renderizadas.

El nivel lo arma build_downtown.build_level (personajes, reglas y sonidos del nivel Survival convertido, luz horneada) sin la
ciudad fija de Downtown: lo que se coloca es lo de la escena. Lo que se rompe toma las reglas del objeto del juego original
de su tipo (mission_survival.Props); los edificios y árboles se desvanecen cuando tapan al jugador (su nombre empieza con
Building / Tree); el borde de la escena es una pared invisible.
"""
import argparse
import hashlib
import json
import math
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
sys.path.insert(0, str(HERE.parent / 'zombies_convert'))
import citykit  # noqa: E402
import build_downtown  # noqa: E402
import mission_survival  # noqa: E402
import missions  # noqa: E402
import lightmaps  # noqa: E402
from ops import kind  # noqa: E402
import ops  # noqa: E402
from scene import BASE, DEFAULT_LAYERS  # noqa: E402

ASSETS = REPO / 'build-win64' / 'bin' / 'Assets' / 'zombies'
BATCH_CELL = 32.0   # metros de cada cuadro en que se junta lo fijo


# --- lo fijo, junto: menos llamadas de dibujo ---

def batched(catalog, e):
    """Lo que se junta en los cuadros: de la base, que no se desvanece (edificios, árboles), no se rompe y no es el regalo."""
    if e.get('layer', BASE) != BASE or e.get('role') == 'gift':
        return False
    if 'building' in catalog['models'][e['asset']]['tags'] or kind(catalog, e['asset']) == 'tree':
        return False
    return not (e['asset'] in mission_survival.TEMPLATE_OF and not e.get('unbreakable'))


def batch_static(scene, catalog):
    """Junta lo fijo de la escena en un modelo por cuadro de BATCH_CELL metros (Blender: blender_batch.py), en
    Assets/zombies/downtown/batches/<escena>/. Se rehace solo si cambió. Devuelve la carpeta y los nombres de los cuadros."""
    chunks = {}
    for e in scene.entities:
        if not batched(catalog, e):
            continue
        cell = f'static_{math.floor(e["x"] / BATCH_CELL)}_{math.floor(e["y"] / BATCH_CELL)}'.replace('-', 'm')
        chunks.setdefault(cell, []).append({'path': str(REPO / catalog['models'][e['asset']]['path']), 'x': e['x'], 'y': e['y'],
                                            'z': e.get('z', 0.0), 'yaw': e['yaw'], 'scale': e['scale']})
    out = ASSETS / 'downtown' / 'batches' / scene.name
    out.mkdir(parents=True, exist_ok=True)
    plan = {'chunks': chunks}
    text = json.dumps(plan, sort_keys=True)
    key = hashlib.sha1((text + (HERE / 'blender_batch.py').read_text(encoding='utf-8')).encode()).hexdigest()
    stamp = out / 'batch.key'
    if stamp.is_file() and stamp.read_text() == key and all((out / f'{n}.gltf').is_file() for n in chunks):
        return out, sorted(chunks)
    for old in out.glob('*.gltf'):
        old.unlink()
    plan_path = scene.folder / 'batch_plan.json'
    plan_path.write_text(text, encoding='utf-8')
    result = subprocess.run([str(citykit.BLENDER), '--background', '--python', str(HERE / 'blender_batch.py'), '--',
                             '--plan', str(plan_path), '--out', str(out)], capture_output=True, text=True)
    missing = [n for n in chunks if not (out / f'{n}.gltf').is_file()]
    if missing or 'Traceback' in result.stdout + result.stderr:
        raise RuntimeError(f'no se juntaron {missing}:\n' + (result.stdout + result.stderr)[-3000:])
    stamp.write_text(key)
    print(f'lo fijo junto: {sum(len(c) for c in chunks.values())} modelos en {len(chunks)} cuadros')
    return out, sorted(chunks)


def level_name(scene, mission):
    return f'lb_{scene.name}_{mission["id"]}'


def scene_missions(scene):
    """Las misiones de la escena (las escenas viejas tienen una sola en 'mission')."""
    found = scene.data.get('missions')
    if found:
        return found
    return [dict(scene.data.get('mission', {'mode': 'survival', 'kills': 60}), id='survival')]


def spawn_points(scene, mission=None):
    """Dos puntos por zona spawn, a lo largo de su lado largo: las salidas del pueblo, o las barricadas del área de la
    misión si se juega en una (sus 'spawns')."""
    points = []
    rects = mission.get('spawns') if mission and mission.get('spawns') else \
        [z['rect'] for z in scene.data['zones'].values() if z['kind'] == 'spawn']
    for rect in rects:
        x0, y0, x1, y1 = rect
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        if x1 - x0 >= y1 - y0:
            points += [(x0 + (x1 - x0) * 0.25, cy), (x0 + (x1 - x0) * 0.75, cy)]
        else:
            points += [(cx, y0 + (y1 - y0) * 0.25), (cx, y0 + (y1 - y0) * 0.75)]
    return points


def mission_layers(mission):
    return [BASE] + list(mission.get('layers', DEFAULT_LAYERS))


def in_mission(entity, mission):
    return mission is None or entity.get('layer', BASE) in mission_layers(mission)


def heavy_buildings(scene, catalog, limit=15000):
    """Los edificios de la escena que pasan de 'limit' triángulos (los enteros de Downtown): el nivel dibuja su copia
    optimizada (buildings.optimized) y choca con el original (la misma planta). Devuelve {modelo: copia}."""
    import buildings
    index_path = HERE / 'assets_index.json'
    triangles = {}
    if index_path.is_file():
        for pack in json.loads(index_path.read_text(encoding='utf-8')).values():
            for name, info in pack['models'].items():
                triangles[name] = info['triangles']
    heavy = sorted({e['asset'] for e in scene.entities if 'building' in catalog['models'][e['asset']]['tags']
                    and triangles.get(e['asset'], 0) > limit})
    if not heavy:
        return {}
    return dict(zip(heavy, buildings.optimized(heavy, limit)))


def make_build(scene, catalog, mission, batches=None, lighter=None, lightmap=None):
    """La función que build_level llama con el nivel en construcción: pone cada entidad y arma el script de la misión.
    'batches' (carpeta, nombres): lo fijo ya junto por batch_static; esas entidades solo ponen su colisión.
    'lightmap' (atlas, lugares): lo que dio lightmaps.layout y el atlas horneado; esas entidades se dibujan con su copia con
    UV de lightmap y dicen dónde están en el atlas."""
    atlas, placed = lightmap or (None, {})
    def build(level, hollywood):
        object_ids = {}   # id de entidad -> id de objeto del nivel (lo que se rompe)
        templates = {d['name']: d['components'] for d in hollywood['destructibles']}
        props = mission_survival.Props(level, templates)
        gift = None
        # la base primero (lo fijo: lo único que cuenta para la luz horneada), luego las capas de la misión
        chosen = [e for e in scene.entities if e.get('layer', BASE) == BASE]
        chosen += [e for e in scene.entities if e.get('layer', BASE) != BASE and in_mission(e, mission)]
        for e in chosen:
            if e.get('layer', BASE) != BASE:
                level.static = False
            asset, x, y, yaw, s = e['asset'], e['x'], e['y'], e['yaw'], e['scale']
            k = kind(catalog, asset)
            if batches and batched(catalog, e):
                if k != 'ground':
                    level.collide(asset, x, y, yaw, s, shrink=0.05)   # se dibuja con su cuadro; aquí solo choca
                continue
            if e.get('role') == 'gift':
                gift = (x, y)
                level.place(asset, x, y, yaw, scale=s, object_id=mission_survival.GIFT_ID, label='GiftBox')
            elif k == 'ground':
                level.place(placed[e['id']]['asset'] if e['id'] in placed else asset, x, y, yaw, z=e.get('z', 0.0), scale=s,
                            label=e['id'])
            elif k == 'tree':
                level.place(asset, x, y, yaw, scale=s, label=f'Tree_{e["id"]}')
                level.solid(circle(x, y, 0.25 * s), 'trunk')
            elif 'building' in catalog['models'][asset]['tags']:
                level.collide(asset, x, y, yaw, s, shrink=0.15)
                drawn = (lighter or {}).get(asset, asset)   # la copia optimizada, si la hay
                if e['id'] in placed:
                    drawn = placed[e['id']]['asset']           # (con UV de lightmap)
                level.place(drawn, x, y, yaw, scale=s, label=f'Building_{e["id"]}')
            elif e.get('role') in OWN_PROPS:
                object_ids[e['id']] = own_prop(level, asset, x, y, yaw, s, e['role'])
            elif asset in mission_survival.TEMPLATE_OF and not e.get('unbreakable'):
                object_ids[e['id']] = props.add(asset, x, y, yaw, scale=s)
            elif 'pole' in catalog['models'][asset]['tags']:
                level.solid(circle(x, y, 0.2 * s), 'pole')   # el poste; el brazo pasa por arriba
                level.place(asset, x, y, yaw, scale=s, label=e['id'])
            else:
                level.collide(asset, x, y, yaw, s, shrink=0.05)
                level.place(asset, x, y, yaw, scale=s, label=e['id'])
            if placed and level.objects and level.objects[-1]['name'] in (e['id'], f'Building_{e["id"]}', f'Tree_{e["id"]}'):
                if e['id'] in placed:
                    level.objects[-1]['lightmap'] = placed[e['id']]['lightmap']
                if e['id'] in placed or lightmaps.occludes(catalog, e):
                    level.objects[-1]['baked'] = True   # su sombra ya está en el lightmap del piso
        if batches:
            folder, names = batches
            for name in names:
                relative = Path(os.path.relpath(folder / f'{name}.gltf', level.asset_dir)).as_posix()
                level.objects.append({'name': name, 'id': -1, 'mesh': relative, 'x': 0.0, 'y': 0.0, 'z': 0.0, 'rot': [0, 0, 0], 'scale': 1.0})
        # el borde: paredes invisibles
        x0, y0, x1, y1 = scene.data['bounds']
        for wall in ([(x0 - 1, y0 - 1), (x1 + 1, y0 - 1), (x1 + 1, y0), (x0 - 1, y0)], [(x0 - 1, y1), (x1 + 1, y1), (x1 + 1, y1 + 1), (x0 - 1, y1 + 1)],
                     [(x0 - 1, y0), (x0, y0), (x0, y1), (x0 - 1, y1)], [(x1, y0), (x1 + 1, y0), (x1 + 1, y1), (x1, y1)]):
            level.solid(wall, 'edge')
        if mission.get('area'):
            # la misión se juega en su área: además de las barricadas en las calles, paredes invisibles en su orilla (entre
            # las casas de las manzanas del borde también se saldría)
            ax0, ay0, ax1, ay1 = mission['area']
            for wall in ([(ax0 - 1, ay0 - 1), (ax1 + 1, ay0 - 1), (ax1 + 1, ay0), (ax0 - 1, ay0)],
                         [(ax0 - 1, ay1), (ax1 + 1, ay1), (ax1 + 1, ay1 + 1), (ax0 - 1, ay1 + 1)],
                         [(ax0 - 1, ay0), (ax0, ay0), (ax0, ay1), (ax0 - 1, ay1)], [(ax1, ay0), (ax1 + 1, ay0), (ax1 + 1, ay1), (ax1, ay1)]):
                level.solid(wall, 'area')
        start = mission_start(scene, catalog, mission)
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        anchors = {str(mission_survival.FOCUS_ID): (cx, cy, 0.0)}
        title = mission.get('title') or f'{scene.name.upper()}: {mission["mode"].upper()}'
        if mission['mode'] == 'survival':
            script, _ = missions.survival(mission, title)
        elif mission['mode'] == 'destroy':
            targets = [object_ids.get(t) for t in mission.get('targets', [])]
            if not targets or None in targets:
                raise ValueError(f'misión {mission["id"]}: los objetivos deben ser entidades que se rompen ({mission.get("targets")})')
            # anclas en los objetivos: la cámara los recorre al empezar
            target_anchors = []
            for k, entity_id in enumerate(mission['targets']):
                e = scene.get(entity_id)
                anchors[str(46000 + k)] = (e['x'], e['y'], 0.0)
                target_anchors.append(46000 + k)
            script, _ = missions.destroy(mission, title, targets, target_anchors)
        elif mission['mode'] == 'holdout':
            script, _ = missions.holdout(mission, title)
        elif mission['mode'] == 'defend':
            goal = [e for e in chosen if e.get('role') == 'objective']
            if not goal:
                raise ValueError(f'misión {mission["id"]}: defend necesita una entidad con rol objective')
            anchors['46900'] = (goal[0]['x'], goal[0]['y'], 0.0)
            script, _ = missions.defend(mission, title, object_ids[goal[0]['id']], 46900, OWN_PROPS['objective']['Health']['HealthValue'])
        elif mission['mode'] == 'explore':
            script, _ = missions.explore(mission, title)
        elif mission['mode'] == 'escort':
            # el sobreviviente sale junto al jugador y va por las calles, de punto en punto, hasta el más lejano
            spots = token_spots(scene, catalog, int(mission.get('checkpoints', 4)) + 6, int(mission.get('seed', 1)), spacing=24.0, green=False, start=start)
            far = max(spots, key=lambda p: math.hypot(p[0] - start[0], p[1] - start[1]))
            way = [p for p in spots if p != far and math.hypot(p[0] - start[0], p[1] - start[1]) < math.hypot(far[0] - start[0], far[1] - start[1])]
            way = route_from(start, way)[:int(mission.get('checkpoints', 4)) - 1] + [far]
            anchors['49500'] = (start[0] + 1.5, start[1], 0.0)
            route = []
            for k, (px, py) in enumerate(way):
                anchors[str(49501 + k)] = (px, py, 0.0)
                route.append(49501 + k)
            script, _ = missions.escort(mission, title, mission.get('hero', 'lis'), 49500, route)
        elif mission['mode'] == 'crates':
            # cajas de provisiones infectadas por el pueblo, en el orden de un recorrido desde el inicio
            spots = token_spots(scene, catalog, int(mission.get('crates', 6)), int(mission.get('seed', 1)), spacing=22.0, green=True, start=start)
            crates, crate_anchors = [], []
            for k, (px, py) in enumerate(route_from(start, spots)):
                anchors[str(46500 + k)] = (px, py, 0.0)
                crate_anchors.append(46500 + k)
                crates.append(own_prop(level, 'Prop_Crate', px, py, (k * 37.0) % 360.0, 1.2, 'crate'))
            script, _ = missions.crates(mission, title, crates, crate_anchors)
        elif mission['mode'] == 'race':
            spots = token_spots(scene, catalog, int(mission.get('checkpoints', 6)), int(mission.get('seed', 1)), spacing=25.0, green=False, start=start)
            route = route_from(start, spots)
            checkpoints = []
            for k, (px, py) in enumerate(route):
                anchors[str(48000 + k)] = (px, py, 0.0)
                checkpoints.append(48000 + k)
            script, _ = missions.race(mission, title, checkpoints)
        elif mission['mode'] == 'rescue':
            spots = token_spots(scene, catalog, int(mission.get('keys', 3)) + 1, int(mission.get('seed', 1)), spacing=30.0, start=start)
            spots.sort(key=lambda p: -math.hypot(p[0] - start[0], p[1] - start[1]))
            anchors['49000'] = (spots[0][0], spots[0][1], 0.0)   # la jaula: el lugar más lejos del inicio
            keys = []
            for k, (px, py) in enumerate(spots[1:]):
                anchors[str(49001 + k)] = (px, py, 0.0)
                keys.append(49001 + k)
            script, _ = missions.rescue(mission, title, keys, 49000)
        elif mission['mode'] == 'collect':
            token_anchors = []
            for k, (tx, ty) in enumerate(token_spots(scene, catalog, int(mission.get('tokens', 20)), int(mission.get('seed', 1)), start=start)):
                anchors[str(47000 + k)] = (tx, ty, 0.0)
                token_anchors.append(47000 + k)
            script, _ = missions.collect(dict(mission, tokens=len(token_anchors)), title, token_anchors)
        else:
            raise ValueError(f'modo de misión desconocido: {mission["mode"]}')
        result = {
            'spawn': start,
            'spawners': spawn_points(scene, mission),
            'points': {str(mission_survival.START_POINT_ID): start},
            'anchors': anchors,
            'script': script,
            'enemyPool': mission_survival.ENEMY_POOL,
            'giBounds': (x0, y0, x1, y1),
        }
        if atlas:
            result['lightmap'] = {'texture': lightmaps.relative(atlas, level.asset_dir)}
        # lo que se rompe suelta dinero, salud o un power-up (el juego original solo soltaba dinero); los autos, más
        for d in level.destructibles:
            components = dict(d['components'])
            if 'Barricade' in components or 'Objective' in components:
                continue   # lo de defender no suelta nada
            explosive = 'Explosive' in components
            pool = DROPS_LOOT if d['name'] == 'Loot' else DROPS_EXPLOSIVE if explosive else DROPS
            components['PickupSpawn'] = {'Pickups': [{'Pickup': float(i)} for i in pool], 'Random': False,
                                         'SpawnRate': 0.8 if explosive else 1.0 if d['name'] in ('Crate', 'Loot') else 0.55, 'SpawnTimer': 1000.0, 'UseSpawnTimer': False}
            d['components'] = components
        if gift is None:
            print('aviso: la escena no tiene caja de regalo (una entidad con rol gift, p. ej. Chest_Special)')
        return result
    return build


def route_from(start, spots):
    """Los puntos en el orden de un recorrido: cada uno el más cercano al anterior, empezando junto al inicio."""
    left, route, here = list(spots), [], start
    while left:
        nearest = min(left, key=lambda p: math.hypot(p[0] - here[0], p[1] - here[1]))
        left.remove(nearest)
        route.append(nearest)
        here = nearest
    return route


def mission_start(scene, catalog, mission):
    """Dónde empieza el jugador en una misión: donde diga la misión ("start": [x, y]); si no, un lugar libre de una calle
    elegido con el id de la misión (cada misión empieza en otro lado, y siempre en el mismo para esa misión), lejos de las
    entradas de los zombis y no en la orilla del pueblo."""
    if mission.get('start'):
        return tuple(mission['start'])
    plazas = [z['rect'] for z in scene.data['zones'].values() if z['kind'] == 'plaza']
    if mission.get('mode') == 'explore' and plazas:
        # recorrer el pueblo empieza en su plaza: frente al kiosco (que ocupa el centro), entre los macetones
        r = plazas[0]
        return (round((r[0] + r[2]) / 2, 2), round((r[1] + r[3]) / 2 - 4.5, 2))
    seed = int(hashlib.sha1((scene.name + '/' + mission['id']).encode()).hexdigest()[:8], 16)
    spawns = [((z['rect'][0] + z['rect'][2]) / 2, (z['rect'][1] + z['rect'][3]) / 2)
              for z in scene.data['zones'].values() if z['kind'] == 'spawn']
    x0, y0, x1, y1 = mission.get('area') or scene.data['bounds']   # una misión de área empieza adentro de ella
    cx, cy, reach = (x0 + x1) / 2, (y0 + y1) / 2, 0.35 * min(x1 - x0, y1 - y0)
    if mission.get('spawns'):
        spawns = [((s[0] + s[2]) / 2, (s[1] + s[3]) / 2) for s in mission['spawns']]
    # las misiones de antes en la escena ya tienen el suyo: este queda a 25 m o más de ellos
    earlier = []
    for other in scene_missions(scene):
        if other['id'] == mission['id']:
            break
        earlier.append(mission_start(scene, catalog, other))
    candidates = [(x, y) for x, y in token_spots(scene, catalog, 150, seed, spacing=3.0, green=False, start=(1e9, 1e9))
                  if math.hypot(x - cx, y - cy) <= reach and all(math.hypot(x - sx, y - sy) >= 30.0 for sx, sy in spawns)]
    if not candidates:
        return tuple(scene.data['start'])
    apart = lambda p: min((math.hypot(p[0] - ex, p[1] - ey) for ex, ey in earlier), default=1e9)
    for spot in candidates:
        if apart(spot) >= 25.0:
            return spot
    return max(candidates, key=apart)   # un pueblo chico: el más lejos de los otros inicios


def token_spots(scene, catalog, count, seed, spacing=6.0, green=True, start=None):
    """Lugares para las fichas de una misión collect: en calles, banquetas y zonas verdes, libres de lo sólido y separados
    entre sí; nunca junto al inicio del jugador (tiene que salir a buscarlas)."""
    import random
    rng = random.Random(seed)
    boxes = ops.solid_boxes(scene, catalog)
    kinds = ('street', 'park', 'plaza', 'lot') if green else ('street',)
    places = [z['rect'] for z in scene.data['zones'].values() if z['kind'] in kinds]
    start = start or scene.data.get('start', [0.0, 0.0])
    spots = []
    for _ in range(count * 200):
        if len(spots) >= count:
            break
        r = rng.choice(places)
        x, y = rng.uniform(r[0] + 1.0, r[2] - 1.0), rng.uniform(r[1] + 1.0, r[3] - 1.0)
        if ops.overlaps((x - 0.8, y - 0.8, x + 0.8, y + 0.8), boxes):
            continue
        if math.hypot(x - start[0], y - start[1]) < 8.0 or any(math.hypot(x - sx, y - sy) < spacing for sx, sy in spots):
            continue
        spots.append((round(x, 2), round(y, 2)))
    return spots


# Lo que sueltan los props al romperse (se elige uno al azar de la lista: los repetidos pesan más). Ids del juego:
# 25006 dinero, 15020 salud, 15022 velocidad, 15023 daño, 15024 defensa, 15030 bombardeo
DROPS = [25006, 25006, 25006, 25006, 15020, 15020, 15020, 15022, 15023, 15024]
DROPS_EXPLOSIVE = [15020, 15020, 15023, 15030, 25006]
DROPS_LOOT = [15020, 15020, 15022, 15023, 15024, 15030]   # a loot chest: health or a boost, always


# Props nuestros que se rompen (no del juego original): la puerta de una barricada que el jugador repara parado junto a ella
# (vida, y ms de nada a entera) y lo que se defiende. Un zombi pega ~11 cada ~2 s.
OWN_PROPS = {
    'gate': {'Health': {'HealthValue': 120.0}, 'Barricade': {'RepairTime': 3500.0}},
    'objective': {'Health': {'HealthValue': 1500.0}, 'Objective': {}},
    'crate': {'Health': {'HealthValue': 40.0}},
    'loot': {'Health': {'HealthValue': 25.0}},
}


def own_prop(level, model, x, y, yaw, scale, role):
    """Un prop de OWN_PROPS (como mission_survival.Props.add, con sus componentes). Devuelve su id de objeto."""
    import citykit as ck
    low, high = level.model(model)
    corners = ck.footprint(low, high, x, y, yaw, scale, 0.0)
    xs, ys = [c[0] for c in corners], [c[1] for c in corners]
    box = (min(xs), min(ys), max(xs), max(ys))
    level.add_box(corners, high[1] * scale)
    object_id = level.next_id
    level.next_id += 1
    name = {'gate': 'Barricade', 'objective': 'Objective', 'crate': 'Crate', 'loot': 'Loot'}[role]
    level.place(model, x, y, yaw, scale=scale, object_id=object_id, label=name)
    level.walls.append({'shape': 'rect', 'x': round((box[0] + box[2]) * 50, 2), 'y': round((box[1] + box[3]) * 50, 2),
                        'w': round((box[2] - box[0]) * 100, 2), 'h': round((box[3] - box[1]) * 100, 2), 'id': object_id, 'source': 'prop'})
    level.destructibles.append({'id': object_id, 'name': name, 'x': round(x * 100, 2), 'y': round(y * 100, 2),
                                'components': OWN_PROPS[role]})
    return object_id


def circle(x, y, radius, n=6):
    return [(x + radius * math.cos(2 * math.pi * i / n), y + radius * math.sin(2 * math.pi * i / n)) for i in range(n)]


def build_levels(scene, catalog, gi=True, only=None, batch=False, lightmap=True):
    """Escribe un nivel jugable por misión (o solo la misión 'only'). Con 'batch', lo fijo va junto en pocos modelos; con
    'lightmap', el piso y los edificios de la base llevan la luz horneada por texel (lightmaps.py).
    Devuelve las rutas de sus level.json."""
    args = argparse.Namespace(assets=ASSETS, characters='kit', no_gi=not gi, survival='hollywood_s01t01')
    batches = batch_static(scene, catalog) if batch else None
    lighter = heavy_buildings(scene, catalog)   # los edificios pesados se dibujan con su copia optimizada
    if lighter:
        print('edificios optimizados: ' + ', '.join(lighter.values()))
    baked = None
    if lightmap and gi and not batch:
        placed = lightmaps.layout(scene, catalog, lighter)
        if placed:
            baked = (lightmaps.bake(scene, catalog, placed), placed)
    paths = []
    for mission in scene_missions(scene):
        if only and mission['id'] != only:
            continue
        name = level_name(scene, mission)
        build_downtown.build_level(name, make_build(scene, catalog, mission, batches, lighter, baked), args, base=False)
        paths.append(ASSETS / name / 'level.json')
    return paths


# --- Blender: .glb de la escena y vistas ---

def blender_plan(scene, catalog, mission=None):
    entities = []
    for e in scene.entities:
        if not in_mission(e, mission):
            continue
        model = catalog['models'].get(e['asset'])
        if model is None:
            continue
        entities.append({'id': e['id'], 'path': str(REPO / model['path']), 'x': e['x'], 'y': e['y'], 'z': e.get('z', 0.0),
                         'yaw': e['yaw'], 'scale': e['scale']})
    return {'bounds': scene.data['bounds'], 'entities': entities}


def render(scene, catalog, glb=True, labels=None, mission=None):
    """Arma la escena en Blender: escribe <escena>/<nombre>.glb (cada nodo se llama como el id de su entidad) y las vistas
    renders/top.png (cenital) y renders/perspective.png. 'labels' ('id' o 'role') escribe esos textos sobre la cenital.
    Devuelve las rutas de las imágenes."""
    out = scene.folder / 'renders'
    out.mkdir(parents=True, exist_ok=True)
    plan = scene.folder / 'blender_plan.json'
    chosen = None
    if mission:
        chosen = next((m for m in scene_missions(scene) if m['id'] == mission), None)
        if chosen is None:
            raise ValueError(f'no hay misión {mission}')
        out = out / mission
        out.mkdir(parents=True, exist_ok=True)
    plan.write_text(json.dumps(blender_plan(scene, catalog, chosen)), encoding='utf-8')
    command = [str(citykit.BLENDER), '--background', '--python', str(HERE / 'blender_scene.py'), '--', '--plan', str(plan),
               '--top', str(out / 'top.png'), '--perspective', str(out / 'perspective.png')]
    if glb:
        command += ['--glb', str(scene.folder / f'{scene.name}.glb')]
    result = subprocess.run(command, capture_output=True, text=True)
    if not (out / 'top.png').is_file() or 'Traceback' in result.stdout + result.stderr:
        raise RuntimeError('Blender falló:\n' + (result.stdout + result.stderr)[-3000:])
    images = [out / 'top.png', out / 'perspective.png']
    if labels:
        images.append(label_top(scene, out / 'top.png', labels, chosen))
    return images


def label_top(scene, image_path, field, mission=None):
    """Copia de la vista cenital con el id o el rol de cada entidad que no es piso, y los nombres de las zonas."""
    from PIL import Image, ImageDraw
    image = Image.open(image_path).convert('RGB')
    draw = ImageDraw.Draw(image)
    x0, y0, x1, y1 = scene.data['bounds']
    w, h = image.size
    span = max(x1 - x0, y1 - y0) * 1.04          # lo mismo que la cámara de blender_scene.py
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2

    def pixel(x, y):
        k = max(w, h) / span   # la escala ortográfica de Blender cubre el lado mayor de la imagen
        return w / 2 + (x - cx) * k, h / 2 - (y - cy) * k

    for name, zone in scene.data['zones'].items():
        if zone['kind'] in ('street',):
            continue
        zx0, zy0 = pixel(zone['rect'][0], zone['rect'][3])
        zx1, zy1 = pixel(zone['rect'][2], zone['rect'][1])
        draw.rectangle([zx0, zy0, zx1, zy1], outline=(255, 220, 0) if zone['kind'] != 'spawn' else (255, 60, 60), width=2)
        draw.text((zx0 + 4, zy0 + 4), name, fill=(255, 220, 0))
    for e in scene.entities:
        if not in_mission(e, mission):
            continue
        if e.get('role') in (None, 'street', 'ground', 'sidewalk', 'road_marking', 'grass', 'bush', 'garden', 'rock', 'path'):
            continue
        px, py = pixel(e['x'], e['y'])
        text = str(e.get(field) or e['id'])
        draw.text((px + 1, py + 1), text, fill=(0, 0, 0))
        draw.text((px, py), text, fill=(255, 255, 255))
    out = image_path.with_name(f'top_{field}.png')
    image.save(out)
    return out
