"""Reglas de colocación: qué va en qué tipo de zona, a qué distancia de qué, y qué piso lleva cada zona. Las usan validate
(lo que las rompe sale como problema) y el generador (generate.py), así que una escena hecha a mano y una generada siguen las
mismas reglas.

Tipos de zona de una entidad: el 'kind' de su zona (street, block, park, plaza, lot, yard, spawn) y unos de nombre fijo:
'sidewalks' -> sidewalk (lo de las banquetas), 'streets' -> street (cruces), 'edges' -> edge (barricadas del borde),
ring_* -> ring (la franja del borde), 'entrance' (una entrada del pueblo: junto a una salida de calle).
"""
import math

# Roles de edificio (catalog.py los da por tamaño)
BUILDINGS = {'building', 'house', 'shop', 'diner', 'gas_station', 'low_building', 'apartment', 'library', 'school', 'church',
             'office', 'police_station', 'hotel', 'city_hall'}

GREEN = {'park', 'block', 'ring', 'yard', 'plaza'}
ANY = None

# rol -> tipos de zona donde puede ir
ALLOWED = {
    'street': {'street'},
    'road_marking': {'street'},
    'sidewalk': {'street', 'sidewalk'},
    'street_light': {'sidewalk', 'plaza'},
    'hydrant': {'sidewalk'},
    'traffic_light': {'sidewalk'},
    'sign': {'sidewalk', 'entrance', 'edge'},        # el letrero del pueblo: en las entradas, nunca en un parque
    'town_sign': {'sidewalk', 'entrance', 'edge'},
    'tree': {'sidewalk'} | GREEN,
    'bush': GREEN, 'grass': GREEN, 'garden': GREEN, 'rock': GREEN, 'path': GREEN,
    'landmark': {'park', 'plaza', 'block', 'ring', 'lot'},
    'water_tower': {'park', 'plaza', 'block', 'ring', 'lot'},
    'crashed_car': {'street', 'lot', 'sidewalk'},
    'car': {'street', 'lot'},
    'clutter': {'sidewalk', 'street', 'block', 'ring', 'lot', 'yard', 'edge', 'spawn', 'plaza'},
    'barricade': ANY, 'container': ANY, 'barrier': ANY, 'fence': ANY, 'gift': ANY, 'cover': ANY,
}
for _role in BUILDINGS:
    ALLOWED[_role] = {'block', 'ring', 'lot'}

# (rol, rol, metros): no más cerca que eso (centro a centro)
SPACING = [
    ('tree', 'street_light', 2.5),
    ('tree', 'hydrant', 1.5),
    ('tree', 'traffic_light', 2.5),
    ('traffic_light', 'street_light', 3.0),
    ('hydrant', 'street_light', 1.5),
    ('street_light', 'street_light', 8.0),
    ('hydrant', 'hydrant', 10.0),
    ('sign', 'sign', 20.0),
    ('town_sign', 'town_sign', 20.0),
]

# tipo de zona -> los pisos que puede llevar (los de 'ground'); el primero es el que ponen las operaciones
GROUND = {
    'block': ['Street_Asphalt_9x9', 'Ground_Dirt_9x9', 'Ground_Grass_9x9'],
    'ring': ['Street_Asphalt_9x9', 'Ground_Dirt_9x9', 'Ground_Grass_9x9'],
    'park': ['Ground_Grass_9x9'],
    'yard': ['Ground_Grass_9x9', 'Ground_Dirt_9x9'],
    'plaza': ['Sidewalk_NoCurb_3m', 'Street_Asphalt_9x9'],
    'lot': ['Street_Asphalt_9x9', 'Ground_Dirt_9x9'],
}

FIXED = {'sidewalks': 'sidewalk', 'streets': 'street', 'edges': 'edge'}


def zone_kind(scene, entity):
    """El tipo de zona de una entidad (ver arriba), o None si no se sabe."""
    zone = entity.get('zone') or ''
    if zone in FIXED:
        return FIXED[zone]
    if zone.startswith('ring_'):
        return 'ring'
    found = scene.data['zones'].get(zone)
    return found['kind'] if found else None


def entrances(scene):
    """Los puntos donde una calle sale del pueblo (las zonas spawn): un letrero cerca de ahí está en una entrada."""
    points = []
    for name, zone in scene.data['zones'].items():
        if zone['kind'] == 'spawn':
            r = zone['rect']
            points.append(((r[0] + r[2]) / 2, (r[1] + r[3]) / 2))
    return points


def role_of(entity, catalog):
    if entity.get('role'):
        return entity['role']
    roles = catalog['models'].get(entity['asset'], {}).get('roles', [])
    return roles[0] if roles else None


def too_close(role, x, y, others):
    """Si algo de 'others' (entidades con 'role', 'x', 'y') queda más cerca de lo que SPACING pide para 'role' en (x, y)."""
    for a, b, distance in SPACING:
        if role not in (a, b):
            continue
        other = b if role == a else a
        for e in others:
            if e.get('role') == other and math.hypot(e['x'] - x, e['y'] - y) < distance:
                return True
    return False


def allowed(role, kind):
    rule = ALLOWED.get(role, ANY)
    return rule is ANY or kind is None or kind in rule


def check(scene, catalog):
    """Los problemas de reglas de la escena, como texto (validate los junta con los suyos)."""
    problems = []
    gates = entrances(scene)
    by_role = {}
    for e in scene.entities:
        role = role_of(e, catalog)
        kind = zone_kind(scene, e)
        if role in ('sign', 'town_sign') and kind not in ('sidewalk', 'edge'):
            near_gate = any(math.hypot(e['x'] - gx, e['y'] - gy) < 25.0 for gx, gy in gates)
            kind = 'entrance' if near_gate else kind
        if role and not allowed(role, kind):
            problems.append(f'regla: {e["id"]} ({role}) no va en una zona {kind} ({e.get("zone")})')
        if role == 'ground' and kind in GROUND and e['asset'] not in GROUND[kind]:
            problems.append(f'regla: {e["id"]}: una zona {kind} lleva piso {"/".join(GROUND[kind])}, no {e["asset"]}')
        by_role.setdefault(role, []).append(e)
    for a, b, distance in SPACING:
        for ea in by_role.get(a, []):
            for eb in by_role.get(b, []):
                if ea is eb or (a == b and ea['id'] > eb['id']):
                    continue
                d = math.hypot(ea['x'] - eb['x'], ea['y'] - eb['y'])
                # capas de misiones distintas nunca están juntas
                la, lb = ea.get('layer', 'base'), eb.get('layer', 'base')
                together = la == lb or la in ('base', 'props') or lb in ('base', 'props')
                if together and d < distance:
                    problems.append(f'regla: {ea["id"]} ({a}) y {eb["id"]} ({b}) a {d:.1f} m (mínimo {distance:.1f})')
    return problems


def describe():
    """Las reglas como texto (lb.py rules)."""
    lines = ['Dónde va cada rol (tipos de zona):']
    for role in sorted(ALLOWED):
        rule = ALLOWED[role]
        if role in BUILDINGS and role != 'building':
            continue
        lines.append(f'  {role}: {"cualquiera" if rule is ANY else ", ".join(sorted(rule))}')
    lines.append(f'  (edificios: {", ".join(sorted(BUILDINGS))}: como building)')
    lines.append('Distancias mínimas:')
    lines += [f'  {a} - {b}: {d:.1f} m' for a, b, d in SPACING]
    lines.append('Pisos por tipo de zona:')
    lines += [f'  {kind}: {", ".join(grounds)}' for kind, grounds in GROUND.items()]
    return '\n'.join(lines)
