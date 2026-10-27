"""Lightmaps de la base fija de una escena: la luz del cielo, el rebote del sol y la sombra del sol, horneados por texel
con Cycles (blender_lightmap.py) en un atlas que comparten todas las misiones de la escena.

- Recibe lightmap el piso plano de la base (calles, banquetas, losetas, marcas). Los edificios, lo que se rompe, lo de las
  capas de misión, los árboles y lo chico siguen con las sondas de luz (gi.json), y lo fijo tapa la luz en el horneado.
  Los edificios lo probaron y salieron a cuadros: sus muros, simplificados, son cientos de islas chicas de UV que a esta
  densidad toman cada una un valor distinto; las sondas los iluminan parejo.
- Cada modelo que recibe necesita un segundo juego de UV: una copia <modelo>_LM<versión> en assets/generated (la misma
  pasada de Blender que junta las casas, con 'lightmap_uv'), que trae su área.
- El atlas se reparte por área (cuadros, con la misma densidad de texels por metro para todos); cada objeto del nivel
  lleva su rectángulo: "lightmap": [escala, u0, v0] (uv = u0, v0 + uv1 * escala, convención de UV del glTF).
- El atlas queda en Assets/zombies/lightmaps/<clave>.png; se hornea de nuevo solo si cambia la base, la luz o esto.
"""
import hashlib
import json
import math
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'zombies_convert'))
import citykit  # noqa: E402
import downtown  # noqa: E402
import mission_survival  # noqa: E402
from ops import kind  # noqa: E402
from scene import BASE  # noqa: E402

VERSION = 2           # cambia cuando cambia cómo se hacen las UV de lightmap (las copias se hacen de nuevo)
SUFFIX = f'_LM{VERSION}'
ATLAS = 2048          # lado del atlas en texels
MAX_DENSITY = 8.0     # texels por metro, como mucho
FILL = 0.72           # qué parte del atlas se planea llenar (el resto se pierde entre cuadros)
PAD = 2               # texels de margen dentro de cada cuadro (el horneado los rellena estirando la orilla)
SAMPLES = 256


def receives(catalog, e):
    """Lo que lleva lightmap: el piso plano de la base que no se rompe."""
    if e.get('layer', BASE) != BASE or e.get('role') == 'gift':
        return False
    if e['asset'] in mission_survival.TEMPLATE_OF and not e.get('unbreakable'):
        return False
    # del piso, solo lo plano (calles, banquetas, losetas, marcas): las plantas chicas también se pisan, pero son hojas
    return kind(catalog, e['asset']) == 'ground' and flat(e['asset'])


_heights = {}


def flat(asset):
    if asset not in _heights:
        low, high = citykit.model_bounds(citykit.find_model(asset))
        _heights[asset] = high[1] - low[1]
    return _heights[asset] <= 0.3


def occludes(catalog, e):
    """Lo que tapa la luz en el horneado sin recibirla: lo demás de la base que no se rompe."""
    if e.get('layer', BASE) != BASE or e.get('role') == 'gift' or receives(catalog, e):
        return False
    return not (e['asset'] in mission_survival.TEMPLATE_OF and not e.get('unbreakable'))


def uv_models(assets):
    """Las copias con el segundo juego de UV de los modelos (las que falten se hacen en Blender). Devuelve
    {modelo: (copia, área en m²)}."""
    out = citykit.GENERATED
    out.mkdir(parents=True, exist_ok=True)
    names = {a: f'{a}{SUFFIX}' for a in assets}
    chunks = {n: [{'path': str(citykit.find_model(a)), 'x': 0.0, 'y': 0.0, 'z': 0.0, 'yaw': 0.0, 'scale': 1.0}]
              for a, n in names.items() if not ((out / f'{n}.gltf').is_file() and (out / f'{n}.lm.json').is_file())}
    if chunks:
        plan = out / 'plan_lm.json'
        plan.write_text(json.dumps({'chunks': chunks, 'lightmap_uv': True}), encoding='utf-8')
        result = subprocess.run([str(citykit.BLENDER), '--background', '--python', str(HERE / 'blender_batch.py'), '--',
                                 '--plan', str(plan), '--out', str(out)], capture_output=True, text=True)
        missing = [n for n in chunks if not (out / f'{n}.lm.json').is_file()]
        if missing:
            raise RuntimeError(f'Blender no hizo {missing}:\n' + (result.stdout + result.stderr)[-3000:])
        print(f'modelos con UV de lightmap: {len(chunks)}')
    return {a: (n, json.loads((out / f'{n}.lm.json').read_text(encoding='utf-8'))['area']) for a, n in names.items()}


def pack(sizes, side):
    """Cuadros de los lados dados en un atlas de side x side, por estantes (de mayor a menor). Devuelve las esquinas
    (x, y) en el orden de 'sizes', o None si no caben."""
    order = sorted(range(len(sizes)), key=lambda i: -sizes[i])
    corners = [None] * len(sizes)
    x = y = shelf = 0
    for i in order:
        s = sizes[i]
        if x + s > side:
            x, y, shelf = 0, y + shelf, 0
        if y + s > side:
            return None
        corners[i] = (x, y)
        x += s
        shelf = max(shelf, s)
    return corners


def layout(scene, catalog, lighter=None):
    """Qué recibe lightmap y dónde va en el atlas. Devuelve {id de entidad: {'asset': copia con UV, 'uv': [escala, u0, v0]
    en ejes de Blender (v desde abajo), 'lightmap': [escala, u0, v0] para el juego (v del glTF, desde arriba)}}."""
    lighter = lighter or {}
    chosen = [e for e in scene.entities if receives(catalog, e)]
    if not chosen:
        return {}
    models = uv_models(sorted({lighter.get(e['asset'], e['asset']) for e in chosen}))
    areas = [models[lighter.get(e['asset'], e['asset'])][1] * e['scale'] ** 2 for e in chosen]
    density = min(MAX_DENSITY, math.sqrt(FILL * ATLAS * ATLAS / max(sum(areas), 1.0)))
    while True:
        sizes = [max(8, int(math.ceil(math.sqrt(a) * density)) + 2 * PAD) for a in areas]
        corners = pack(sizes, ATLAS)
        if corners:
            break
        density *= 0.92
    print(f'lightmap: {len(chosen)} objetos, {density:.1f} texels/m en {ATLAS}x{ATLAS}')
    result = {}
    for e, size, (x, y) in zip(chosen, sizes, corners):
        scale = (size - 2 * PAD) / ATLAS
        u0, v0 = (x + PAD) / ATLAS, (y + PAD) / ATLAS
        result[e['id']] = {'asset': models[lighter.get(e['asset'], e['asset'])][0], 'uv': [scale, u0, v0],
                           'lightmap': [round(scale, 7), round(u0, 7), round(1.0 - v0 - scale, 7)]}
    return result


def bake(scene, catalog, placed):
    """Hornea el atlas (si no está ya) y devuelve su ruta. 'placed' es lo que dio layout()."""
    lighting = downtown.LIGHTING
    sun_render = lighting['sunDirection']                       # ejes del juego: x este, y arriba, z sur
    sun_travel = (sun_render[0], -sun_render[2], sun_render[1])  # ejes del nivel: x este, y norte, z arriba
    receivers, occluders = [], []
    for e in scene.entities:
        where = {'x': e['x'], 'y': e['y'], 'z': e.get('z', 0.0), 'yaw': e['yaw'], 'scale': e['scale']}
        if e['id'] in placed:
            info = placed[e['id']]
            receivers.append(dict(where, path=str(citykit.find_model(info['asset'])), uv=info['uv'],
                                  reference=kind(catalog, e['asset']) == 'ground'))
        elif occludes(catalog, e):
            occluders.append(dict(where, path=str(citykit.find_model(e['asset']))))
    plan = {'atlas': ATLAS, 'samples': SAMPLES, 'sun': {'travel': sun_travel, 'color': lighting['sunColor']},
            'sky': {'top': lighting['skyTop'], 'horizon': lighting['skyHorizon']}, 'ambient': lighting['ambient'],
            'bounds': scene.data['bounds'], 'receivers': receivers, 'occluders': occluders}
    text = json.dumps(plan, sort_keys=True)
    key = hashlib.sha1((text + (HERE / 'blender_lightmap.py').read_text(encoding='utf-8')).encode()).hexdigest()[:16]
    folder = citykit_assets() / 'lightmaps'
    folder.mkdir(parents=True, exist_ok=True)
    atlas = folder / f'{scene.name}_{key}.png'
    if atlas.is_file():
        print('lightmap: sin cambios')
        return atlas
    for old in folder.glob(f'{scene.name}_*.png'):
        old.unlink()   # el de antes de esta escena
    plan_path = scene.folder / 'lightmap_plan.json'
    plan_path.write_text(text, encoding='utf-8')
    print(f'lightmap: horneando {len(receivers)} objetos que reciben y {len(occluders)} que tapan...')
    log = scene.folder / 'lightmap.log'   # todo lo que dice Blender; aquí solo su avance
    done = False
    with open(log, 'w', encoding='utf-8') as out:
        process = subprocess.Popen([str(citykit.BLENDER), '--background', '--python', str(HERE / 'blender_lightmap.py'), '--',
                                    '--plan', str(plan_path), '--out', str(atlas)], stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, encoding='utf-8', errors='replace')
        for line in process.stdout:
            out.write(line)
            out.flush()
            if line.startswith('LIGHTMAP'):
                print(' ', line.rstrip(), flush=True)
                done = done or line.startswith('LIGHTMAP_DONE')
        process.wait()
    if not atlas.is_file() or not done:
        raise RuntimeError(f'el horneado del lightmap falló (ver {log}):\n' + log.read_text(encoding='utf-8')[-3000:])
    return atlas


def citykit_assets():
    return HERE.parent.parent / 'build-win64' / 'bin' / 'Assets' / 'zombies'


def relative(atlas, level_dir):
    return Path(os.path.relpath(atlas, level_dir)).as_posix()
