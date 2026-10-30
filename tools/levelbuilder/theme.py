"""Temas: qué modelos usa un pueblo para cada rol, de paquetes que no son los de Quaternius (extraídos a Assets/ cuando
hacen falta).

Un tema es un JSON (lo trae el juego, no este repositorio):

    {"name": ..., "packs": {"<corto>": "<parte del nombre del paquete>"},
     "extract": ["python", "<herramienta>", "extract", "{regex}", "--pack", "{pack}"],   (opcional: cómo sacar modelos)
     "models_dir": "<carpeta>/{short}/models",                                              (dónde quedan, bajo Assets/)
     "roles": {"<rol>": [{"pack": "<corto>", "models": "<regex>", "role": "<rol más fino>"}]}}

Sus modelos entran al catálogo con id '<models_dir>/<nombre>' (la ruta bajo Assets/ sin .gltf): citykit.find_model y el
export los resuelven ahí, sin copiarlos. Las etiquetas salen del rol (ROLE_TAGS): lo que choca, lo que se desvanece, etc.
"""
import json
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'zombies_convert'))
import citykit  # noqa: E402

# rol -> etiquetas del catálogo (ops.kind y export las leen: building choca y se desvanece, tree choca con su tronco,
# ground no choca, pole choca con su poste, small/decal no chocan)
ROLE_TAGS = {
    'shop': ['building'], 'fancy_store': ['building'], 'apartment': ['building'], 'house': ['building'], 'landmark': ['building', 'landmark'],
    'church': ['building', 'landmark'], 'city_hall': ['building', 'landmark'],
    'kiosk': ['landmark', 'pavilion'], 'fountain': ['landmark'],
    'palm': ['vegetation', 'tree'], 'palm_decor': ['vegetation', 'tree'], 'flower_bush': ['vegetation', 'small'],
    'tree': ['vegetation', 'tree'], 'vine': ['vegetation', 'small'], 'bush': ['vegetation', 'small'], 'grass': ['vegetation', 'small'], 'rock': ['rock'],
    'street_light': ['street_prop', 'light', 'pole'], 'bench': ['street_prop'], 'planter': ['street_prop'],
    'hydrant': ['street_prop'], 'trash': ['clutter', 'small'], 'clutter': ['clutter'], 'barrier': ['barrier'],
    'dumpster': ['clutter', 'cover'], 'car': ['vehicle', 'car', 'cover'], 'bus': ['vehicle', 'cover'],
    'cafe': ['street_prop'], 'stall': ['street_prop', 'cover'], 'blood': ['decal', 'small'],
    'beach_umbrella': ['street_prop'], 'umbrella_open': ['street_prop'], 'lounger': ['street_prop'], 'boat': ['vehicle', 'cover'], 'pier_boat': ['vehicle'], 'ship': ['vehicle'], 'driftwood': ['clutter', 'small'],
    'cloud_ring': ['ground', 'sky'],
    # lo que viste un lote (dress.py): en la pared y la azotea no choca; en el piso, sí
    'sign': ['street_prop', 'small'], 'awning': ['small'], 'aircon': ['small'], 'window_planter': ['vegetation', 'small'],
    'fire_escape': ['small'], 'ivy': ['vegetation', 'small'], 'ivy_draped': ['vegetation', 'small'], 'roof_clutter': ['small'], 'shopfront': ['clutter'],
    'table_set': ['street_prop'], 'doghouse': ['clutter'], 'washing_line': ['small'], 'mailbox': ['street_prop'], 'manhole': ['ground'],
    'trash_bin': ['street_prop'], 'street_tree': ['vegetation', 'tree'], 'flowers': ['vegetation', 'small'],
    # lo que cuelga sobre la calle (overhead.py), las escenas armadas (setpieces.py), el fondo
    'roof_sign': ['small'], 'letter': ['small'], 'landmark_letter': ['street_prop'],   # (the word on the malecon: solid)
    'power_pole': ['street_prop', 'pole'], 'cone': ['clutter', 'small'], 'body': ['decal', 'small'],
    'wreck': ['vehicle', 'cover'], 'dirt_pile': ['clutter', 'cover'], 'explosive_barrel': ['clutter'], 'construction': ['clutter'],
    'wheelbarrow': ['clutter', 'small'], 'background_building': ['ground', 'background'], 'playground': ['street_prop'],
}
ASSETS = citykit.REPO / 'build-win64' / 'bin' / 'Assets'


def load(path):
    theme = json.loads(Path(path).read_text(encoding='utf-8'))
    theme['path'] = str(Path(path).resolve())
    theme.setdefault('models_dir', '{short}/models')
    return theme


def patterns(theme):
    """Por paquete corto, la expresión que junta todos sus modelos del tema."""
    out = {}
    for specs in theme['roles'].values():
        for spec in specs:
            out.setdefault(spec['pack'], []).append(spec['models'])
    return {pack: '|'.join(f'(?:{p})' for p in found) for pack, found in out.items()}


def apply_materials(theme, materials):
    """The theme's own floors: its "materials" {name: spec} over the generator's (roadmesh.MATERIALS), key by key. A
    texture ("color", "normal", "orm") is a path under Assets/ (a licensed pack's, extracted there), so the free
    defaults stay in the public code and a game picks what it has licensed:

        "materials": {"grass": {"color": "<packs>/<pack>/textures/Grass_01.png", "normal": "...", "repeat": 4.0}}
    """
    for name, spec in theme.get('materials', {}).items():
        merged = dict(materials.get(name, {}))
        if 'color' in spec:
            # (the default's normal and ORM maps belong to its own texture: they go with it unless the theme names its own)
            merged.pop('normal', None)
            merged.pop('orm', None)
        for key, value in spec.items():
            if key in ('color', 'normal', 'orm') and isinstance(value, str):
                path = ASSETS / value
                if not path.is_file():
                    raise FileNotFoundError(f'theme material {name}: no {path}')
                value = path
            merged[key] = value
        materials[name] = merged


def material_tints(theme):
    """The theme's tints for materials of its models ("material_tints": [{"models": regex of the model file's stem, "match":
    regex of the material's name, "tint": [r, g, b, a], "blend": "alpha"|"opaque"...}]): written as the engine's sidecar
    beside each model (<model>.gltf.materials.xml), e.g. a demo scene's shop glass, teal as Unity's glass shader shows it."""
    rules = theme.get('material_tints', [])
    if not rules:
        return 0
    written = 0
    folders = {models_dir(theme, short) for short in patterns(theme)}
    for path in (p for folder in sorted(folders) if folder.is_dir() for p in folder.rglob('*.gltf')):
        wanted = [r for r in rules if re.search(r['models'], path.stem)]
        if not wanted:
            continue
        names = [m.get('name', '') for m in json.loads(path.read_text(encoding='utf-8')).get('materials', [])]
        lines = []
        for name in dict.fromkeys(names):
            rule = next((r for r in wanted if re.search(r['match'], name)), None)
            if rule:
                attrs = f' tint="{" ".join(str(v) for v in rule["tint"])}"' + (f' blend="{rule["blend"]}"' if rule.get('blend') else '')
                lines.append(f'  <material name="{name}"{attrs}/>')
        if lines:
            text = '<materials>\n' + '\n'.join(lines) + '\n</materials>\n'
            path.with_name(path.name + '.materials.xml').write_text(text, encoding='utf-8')
            written += 1
    return written


def models_dir(theme, short):
    return ASSETS / theme['models_dir'].format(short=short)


def missing(theme):
    """Por paquete, si le faltan modelos: alguna expresión de un rol que no encuentra nada extraído."""
    out = {}
    for role, specs in theme['roles'].items():
        for spec in specs:
            folder = models_dir(theme, spec['pack'])
            names = [p.stem for p in folder.glob('*.gltf')] if folder.is_dir() else []
            if not any(re.search(spec['models'], n) for n in names):
                out.setdefault(spec['pack'], []).append(role)
    return out


def extract(theme, only_missing=True):
    """Saca de su archivo los modelos del tema (con el comando 'extract' del tema), paquete por paquete."""
    if 'extract' not in theme:
        raise ValueError('el tema no dice cómo extraer modelos ("extract")')
    wanted = patterns(theme)
    lacking = missing(theme) if only_missing else {p: [] for p in wanted}
    for short, regex in wanted.items():
        if short not in lacking:
            continue
        command = [part.format(regex=regex, pack=theme['packs'][short]) for part in theme['extract']]
        if command[0] == 'python':
            command[0] = sys.executable
        print(f'extrayendo de {short}: {", ".join(lacking.get(short, [])) or "todo"}', flush=True)
        subprocess.run(command, cwd=str(citykit.REPO), check=True)


def add_to_catalog(catalog, theme):
    """Los modelos extraídos del tema, como entradas del catálogo (en memoria). Devuelve cuántos."""
    count = 0
    for role, specs in theme['roles'].items():
        for spec in specs:
            folder = models_dir(theme, spec['pack'])
            if not folder.is_dir():
                continue
            fine = spec.get('role', role)
            for path in sorted(folder.glob('*.gltf')):
                if not re.search(spec['models'], path.stem):
                    continue
                key = path.relative_to(ASSETS).with_suffix('').as_posix()
                if key in catalog['models']:
                    entry = catalog['models'][key]
                    for r in (role, fine):
                        if r not in entry['roles']:
                            entry['roles'].append(r)
                    if spec.get('pose'):
                        entry['pose'] = spec['pose']
                    if spec.get('turn'):
                        entry['turn'] = spec['turn']
                    continue
                try:
                    low, high = citykit.model_bounds(path)
                except (KeyError, ValueError, IndexError):
                    continue
                tags = list(dict.fromkeys(ROLE_TAGS.get(fine, []) + ROLE_TAGS.get(role, []) + ['theme']))
                catalog['models'][key] = {
                    'pack': f'{theme["name"]}:{spec["pack"]}',
                    'path': path.relative_to(citykit.REPO).as_posix(),
                    'size': [round(float(high[0] - low[0]), 2), round(float(high[2] - low[2]), 2), round(float(high[1] - low[1]), 2)],
                    'low': [round(float(v), 3) for v in low],
                    'high': [round(float(v), 3) for v in high],
                    'tags': tags,
                    **({'pose': spec['pose']} if spec.get('pose') else {}),
                    # a model whose front is not its +z (some packs' demo groups face -z): turned this much more
                    **({'turn': spec['turn']} if spec.get('turn') else {}),
                    'roles': list(dict.fromkeys([role, fine])),
                }
                count += 1
    return count


def by_role(catalog, role):
    """Los ids de los modelos del tema con ese rol."""
    return sorted(k for k, m in catalog['models'].items() if role in m['roles'] and 'theme' in m['tags'])
