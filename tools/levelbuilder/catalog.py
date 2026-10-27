"""Catálogo de los modelos de los paquetes libres (Quaternius, CC0): qué hay, cuánto mide y para qué sirve.

Recorre los paquetes instalados por tools/fetch_free_assets.py (Downtown City, Zombie Apocalypse Kit, Stylized Nature) y escribe
catalog.json: por modelo su id (el nombre del archivo), paquete, ruta, tamaño real en metros, pivote, etiquetas y roles.
Los roles dicen "sirve como...": un edificio pequeño sirve como tienda o casa, un auto como auto chocado, etc.
"""
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'zombies_convert'))
import citykit  # noqa: E402

CATALOG = HERE / 'catalog.json'

# Paquete -> carpeta donde están sus .gltf
PACKS = {
    'downtown-city': citykit.CITY / 'Exports',
    'zombie-kit': citykit.KIT,
    'nature': citykit.NATURE / 'glTF',
    'medieval': citykit.MEDIEVAL / 'glTF',   # piezas modulares de casas (paredes, ventanas, techos de teja)
    'own': citykit.OWN,   # nuestras (own_assets.py): losetas de piso
    'generated': citykit.GENERATED,   # las casas del generador de edificios (buildings.py)
}

# Reglas por nombre: (expresión, etiquetas, roles). Un modelo junta lo de todas las reglas que le aplican.
RULES = [
    (r'^Building_Small', ['building'], ['house', 'shop', 'diner', 'gas_station', 'low_building']),
    (r'^Building_House', ['building', 'generated'], ['house', 'low_building']),
    (r'^Building_Office', ['building', 'generated'], ['office', 'apartment', 'shop', 'hotel']),
    (r'^Building_Medium', ['building'], ['apartment', 'library', 'school', 'church', 'shop']),
    (r'^Building_Large', ['building'], ['office', 'police_station', 'hotel', 'city_hall']),
    (r'^(Brick|Metal|Trim|Cornice|Roof|Door|Entrance|Stairs|Floor)', ['modular', 'building_part'], ['facade_piece']),
    (r'^(Wall|Corner|Window|WindowShutters|Overhang|Balcony|HoleCover|Stair)_', ['modular', 'building_part'], ['facade_piece']),
    (r'^Prop_(Crate|Brick\d)', ['clutter'], ['clutter', 'cover']),
    (r'^Prop_(MetalFence|WoodenFence|Fence)', ['street_prop', 'barrier'], ['fence']),
    (r'^Prop_(Chimney|Support|Vine|ExteriorBorder)', ['building_part'], ['facade_piece']),
    (r'^Street_', ['street', 'ground'], ['street']),
    (r'^Sidewalk_', ['sidewalk', 'ground'], ['sidewalk']),
    (r'^Decal_', ['decal', 'ground'], ['road_marking']),
    (r'^Vehicle_', ['vehicle', 'car'], ['car', 'crashed_car', 'cover']),
    (r'Armored', ['armored'], ['police_vehicle', 'barricade']),
    (r'^Vehicle_Truck', [], ['police_vehicle', 'delivery_truck']),
    (r'^(FireHydrant)', ['street_prop', 'breakable'], ['hydrant']),
    (r'^(TrafficCone|TrafficBarrier|PlasticBarrier)', ['street_prop', 'barrier'], ['barrier', 'fence']),
    # 'pole': choca solo con su poste (en el pivote), no con la caja del brazo que sale sobre la calle
    (r'^StreetLights', ['street_prop', 'light', 'pole'], ['street_light']),
    (r'^TrafficLight', ['street_prop', 'pole'], ['traffic_light']),
    (r'^TownSign', ['street_prop', 'sign'], ['sign', 'town_sign']),
    (r'^WaterTower', ['landmark'], ['water_tower', 'landmark']),
    (r'^Container_', ['container', 'cover'], ['container', 'barricade']),
    (r'^(Barrel|Pallet|TrashBag|CinderBlock|Pipes|Wheel|Couch)', ['clutter'], ['clutter', 'cover']),
    (r'^Chest', ['chest'], ['gift']),
    (r'^Prop_(ACUnit|Bollard|Drain|ManholeCover|Planter)', ['street_prop'], ['clutter']),
    (r'^(CommonTree|Pine|DeadTree|TwistedTree)', ['vegetation', 'tree'], ['tree']),
    (r'^(Bush|Grass|Flower|Fern|Clover|Plant|Mushroom|Petal)', ['vegetation', 'small'], ['bush', 'grass', 'garden']),
    (r'^(Rock|Pebble)', ['rock'], ['rock']),
    (r'^RockPath', ['ground', 'path'], ['path']),
    (r'^Ground_Grass', ['ground', 'grass_ground'], ['ground']),
    (r'^Ground_Dirt', ['ground', 'dirt_ground'], ['ground']),
    (r'^Ground_Sand', ['ground', 'sand_ground'], ['ground']),
    (r'^Ground_Path', ['ground', 'path'], ['path']),
    (r'^Characters_(GermanShepherd|Pug)', ['animal', 'dog'], ['dog']),
    (r'^Characters_', ['character'], ['survivor']),
    (r'^Zombie_', ['character', 'enemy'], ['zombie']),
    (r'^(Axe|Guitar|Knife|Pistol|Rifle|SMG|Shotgun|Spear|WoodenBat)', ['weapon'], ['weapon']),
    (r'^Blood', ['decal'], ['blood']),
]

# Lo que se rompe con disparos (las reglas del objeto del juego original del mismo tipo: ver mission_survival.TEMPLATE_OF)
BREAKABLE = {'TrafficCone_1', 'TrafficCone_2', 'FireHydrant', 'TrashBag_1', 'TrashBag_2', 'Barrel', 'Vehicle_Pickup',
             'Vehicle_Sports', 'Vehicle_Truck', 'Vehicle_Pickup_Armored', 'Vehicle_Sports_Armored'}

# Lo que un pueblito de EE. UU. suele pedir, y qué hacer cuando no hay un modelo exacto
TOWN_NEEDS = {
    'house': 'Building_Small con techo de pizarra (Roof_Slate_*) o armada con piezas Brick_*',
    'shop': 'Building_Small; la planta baja de Metal_FirstFloor_Window parece escaparate',
    'diner': 'Building_Small con Metal_FirstFloor_Window y autos en el estacionamiento',
    'gas_station': 'no hay surtidores: Building_Small, contenedores y autos en una explanada',
    'police_station': 'Building_Large con vehículos blindados y barreras al frente',
    'library': 'Building_Medium',
    'school': 'Building_Medium con un patio (zona park) al lado',
    'church': 'Building_Medium; no hay campanario',
    'park': 'zona con árboles, arbustos, pasto y caminos de piedra (kit de naturaleza)',
    'tree': 'CommonTree, Pine, DeadTree, TwistedTree',
    'car': 'Vehicle_Pickup, Vehicle_Sports, Vehicle_Truck',
    'crashed_car': 'vehículos girados y amontonados (op wreck)',
    'fence': 'no hay rejas: TrafficBarrier / PlasticBarrier en fila',
    'bench': 'no hay bancas: Couch en un patio, o nada',
    'mailbox': 'no hay',
    'street_light': 'StreetLights',
    'hydrant': 'FireHydrant (se rompe y echa agua)',
    'sign': 'TownSign',
}


def classify(name):
    tags, roles = [], []
    for pattern, more_tags, more_roles in RULES:
        if re.search(pattern, name):
            tags += [t for t in more_tags if t not in tags]
            roles += [r for r in more_roles if r not in roles]
    if name in BREAKABLE:
        tags.append('breakable')
    return tags, roles


def build_catalog():
    """Lee todos los modelos y escribe catalog.json. Devuelve el catálogo."""
    entries = {}
    for pack, folder in PACKS.items():
        if not folder.exists():
            print(f'catálogo: falta el paquete {pack} ({folder}); instálalo con tools/fetch_free_assets.py')
            continue
        for path in sorted(folder.rglob('*.gltf')):
            name = path.stem
            if name in entries:
                continue
            if citykit.find_model(name) != path:
                continue  # hay copias (el kit trae varias carpetas): se queda la misma que usan las herramientas
            try:
                low, high = citykit.model_bounds(path)
            except (KeyError, ValueError, IndexError):
                continue
            tags, roles = classify(name)
            entries[name] = {
                'pack': pack,
                'path': path.relative_to(citykit.REPO).as_posix(),
                # tamaño en metros: ancho (x, este), fondo (z del modelo, hacia el sur) y alto
                'size': [round(float(high[0] - low[0]), 2), round(float(high[2] - low[2]), 2), round(float(high[1] - low[1]), 2)],
                # dónde queda el origen del modelo dentro de su caja (low y high en ejes del modelo, y arriba)
                'low': [round(float(v), 3) for v in low],
                'high': [round(float(v), 3) for v in high],
                'tags': tags,
                'roles': roles,
            }
    catalog = {'models': entries, 'needs': TOWN_NEEDS}
    CATALOG.write_text(json.dumps(catalog, indent=1, ensure_ascii=False), encoding='utf-8')
    return catalog


def load_catalog():
    if not CATALOG.is_file():
        return build_catalog()
    return json.loads(CATALOG.read_text(encoding='utf-8'))


def summary(catalog, wanted=None):
    """Texto corto: cuántos modelos por paquete y etiqueta, y para cada necesidad del pueblito qué sirve o qué falta."""
    models = catalog['models']
    lines = []
    packs = {}
    for entry in models.values():
        packs[entry['pack']] = packs.get(entry['pack'], 0) + 1
    lines.append('paquetes: ' + ', '.join(f'{p} {n}' for p, n in sorted(packs.items())))
    tags = {}
    for entry in models.values():
        for tag in entry['tags']:
            tags[tag] = tags.get(tag, 0) + 1
    lines.append('etiquetas: ' + ', '.join(f'{t} {n}' for t, n in sorted(tags.items())))
    for need, alternative in catalog['needs'].items():
        if wanted and need not in wanted:
            continue
        fits = sorted(n for n, e in models.items() if need in e['roles'])
        lines.append(f'{need}: ' + (', '.join(fits[:6]) + (' ...' if len(fits) > 6 else '') if fits else '(nada exacto)') + f'  -> {alternative}')
    return '\n'.join(lines)


def find(catalog, role=None, tag=None):
    """Los ids de modelos con ese rol y/o etiqueta."""
    return sorted(n for n, e in catalog['models'].items()
                  if (role is None or role in e['roles']) and (tag is None or tag in e['tags']))
