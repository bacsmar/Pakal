"""La escena: la fuente de verdad de un nivel (scenes/<nombre>/scene.json).

Una escena tiene entidades (un modelo del catálogo puesto en un lugar, con un id legible como building_library_01), zonas
(rectángulos con nombre y tipo: calle, manzana, parque, spawn...), el punto de inicio y la misión. Todo en metros, con los
ejes del nivel: x al este, y al norte; el giro (yaw) en grados, 0 = el frente del modelo mira al sur.

Capas: cada entidad está en una. 'base' es el pueblo fijo (calles, edificios, árboles, el parque): va en todas las misiones
y es lo único que cuenta para la luz horneada. 'props' (lo que se rompe en las banquetas) también va en todas. Las demás
son aditivas: una misión dice cuáles lleva (p. ej. 'wrecks', los autos chocados de la misión de destruir).

Cada cambio guarda una versión numerada (versions/scene_vNNN.json) con una nota, para poder volver a cualquiera.
"""

BASE = 'base'
DEFAULT_LAYERS = ['props']   # las capas que una misión lleva si no dice otra cosa (además de la base)
import copy
import json
import re
import sys
from datetime import datetime
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'zombies_convert'))
import citykit  # noqa: E402

SCENES = HERE / 'scenes'


def empty(name, width, height):
    return {
        'name': name,
        'version': 0,
        'bounds': [-width / 2, -height / 2, width / 2, height / 2],   # x0, y0, x1, y1: el jugador no sale de aquí
        'start': [0.0, 0.0],
        'missions': [{'id': 'survival', 'mode': 'survival', 'kills': 60}],   # cada una sale como un nivel
        'zones': {},
        'entities': [],
    }


class Scene:
    def __init__(self, name):
        self.name = name
        self.folder = SCENES / name
        self.path = self.folder / 'scene.json'
        self.data = json.loads(self.path.read_text(encoding='utf-8')) if self.path.is_file() else None

    # --- guardar y versiones ---

    def exists(self):
        return self.data is not None

    def save(self, note):
        """Guarda la escena como la versión siguiente, con su nota en history.json."""
        self.data['version'] += 1
        version = self.data['version']
        (self.folder / 'versions').mkdir(parents=True, exist_ok=True)
        text = json.dumps(self.data, indent=1, ensure_ascii=False)
        self.path.write_text(text, encoding='utf-8')
        (self.folder / 'versions' / f'scene_v{version:03d}.json').write_text(text, encoding='utf-8')
        history = self.history()
        history.append({'version': version, 'note': note, 'at': datetime.now().isoformat(timespec='seconds'),
                        'entities': len(self.data['entities'])})
        (self.folder / 'history.json').write_text(json.dumps(history, indent=1, ensure_ascii=False), encoding='utf-8')
        return version

    def history(self):
        path = self.folder / 'history.json'
        return json.loads(path.read_text(encoding='utf-8')) if path.is_file() else []

    def restore(self, version):
        """Vuelve a una versión anterior (queda guardada como una versión nueva: nada se pierde)."""
        path = self.folder / 'versions' / f'scene_v{version:03d}.json'
        if not path.is_file():
            raise ValueError(f'no existe la versión {version}')
        current = self.data['version']
        self.data = json.loads(path.read_text(encoding='utf-8'))
        self.data['version'] = current
        return self.save(f'restaurada la versión {version}')

    # --- entidades ---

    @property
    def entities(self):
        return self.data['entities']

    def get(self, entity_id):
        for entity in self.entities:
            if entity['id'] == entity_id:
                return entity
        return None

    def new_id(self, base):
        """El siguiente id libre: base_01, base_02..."""
        base = re.sub(r'[^a-z0-9_]+', '_', base.lower()).strip('_')
        taken = {e['id'] for e in self.entities}
        n = 1
        while f'{base}_{n:02d}' in taken:
            n += 1
        return f'{base}_{n:02d}'

    def add(self, asset, x, y, yaw=0.0, scale=1.0, zone=None, role=None, tags=None, entity_id=None, z=0.0, **extra):
        entity = {'id': entity_id or self.new_id(role or asset), 'layer': BASE, 'asset': asset, 'x': round(x, 3), 'y': round(y, 3),
                  'z': round(z, 3), 'yaw': round(yaw % 360.0, 2), 'scale': scale, 'zone': zone, 'role': role,
                  'tags': tags or []}
        entity.update(extra)
        if self.get(entity['id']):
            raise ValueError(f'ya hay una entidad {entity["id"]}')
        self.entities.append(entity)
        return entity

    def remove(self, predicate):
        before = len(self.entities)
        self.data['entities'] = [e for e in self.entities if not predicate(e)]
        return before - len(self.entities)

    def select(self, zone=None, role=None, asset=None, tag=None, layer=None):
        return [e for e in self.entities
                if (zone is None or e.get('zone') == zone) and (role is None or e.get('role') == role)
                and (asset is None or e['asset'] == asset) and (tag is None or tag in e.get('tags', []))
                and (layer is None or e.get('layer', BASE) == layer)]

    def layers(self):
        found = {}
        for e in self.entities:
            found[e.get('layer', BASE)] = found.get(e.get('layer', BASE), 0) + 1
        return found

    # --- zonas ---

    def zone_rect(self, zone):
        z = self.data['zones'].get(zone)
        if z is None:
            raise ValueError(f'no hay zona {zone}')
        return z['rect']


def footprint(catalog, entity, shrink=0.0):
    """Las cuatro esquinas (metros, ejes del nivel) del rectángulo que ocupa una entidad en el piso."""
    model = catalog['models'][entity['asset']]
    return citykit.footprint(model['low'], model['high'], entity['x'], entity['y'], entity['yaw'], entity['scale'], shrink)


def box_of(corners):
    xs = [c[0] for c in corners]
    ys = [c[1] for c in corners]
    return min(xs), min(ys), max(xs), max(ys)


def snapshot(data):
    return copy.deepcopy(data)
