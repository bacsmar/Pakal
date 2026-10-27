"""Generador de edificios: arma casas con las piezas modulares del kit Medieval Village (paredes de 2 m, techos de teja
hechos por medida) y oficinas con las de Downtown City (muros de 2 y 4 m, columnas de esquina, pretiles, losas de techo). Una receta (planta, pisos, estilo, semilla) da la lista de piezas; Blender las junta en un solo modelo
(blender_batch.py, la misma pasada que junta lo fijo), que entra al catálogo como cualquier otro: el juego dibuja la casa
como un objeto, no como cuarenta.

Ejes de la casa: los del nivel (x este, y norte), centro de la planta en (0, 0), el frente (la puerta) al sur. Una pieza de
pared tiene su cara de afuera hacia el +z del modelo (el sur del nivel con giro 0).
"""
import hashlib
import json
import random
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'zombies_convert'))
import citykit  # noqa: E402

KIT = 'medieval'
WALL = 2.0          # ancho de una pieza de pared
STORY = 3.12        # alto de un piso
MAX_TRIANGLES = 9000  # una casa junta se simplifica hasta esto (las paredes de piedra traen cada piedra modelada)
VERSION = 2           # cambia cuando cambia cómo se arma o se optimiza una casa (los modelos se hacen de nuevo)

# Los techos de teja que hay, por (ancho en x, fondo en z) en metros de planta
ROOFS = {(4, 4): 'Roof_RoundTiles_4x4', (4, 6): 'Roof_RoundTiles_4x6', (4, 8): 'Roof_RoundTiles_4x8',
         (6, 4): 'Roof_RoundTiles_6x4', (6, 6): 'Roof_RoundTiles_6x6', (6, 8): 'Roof_RoundTiles_6x8',
         (6, 10): 'Roof_RoundTiles_6x10', (6, 12): 'Roof_RoundTiles_6x12', (6, 14): 'Roof_RoundTiles_6x14',
         (8, 8): 'Roof_RoundTiles_8x8', (8, 10): 'Roof_RoundTiles_8x10', (8, 12): 'Roof_RoundTiles_8x12',
         (8, 14): 'Roof_RoundTiles_8x14'}
GABLES = {4: 'Roof_Front_Brick4', 6: 'Roof_Front_Brick6', 8: 'Roof_Front_Brick8'}

STYLES = {
    # material de la planta baja, de los pisos de arriba, esquinas
    'plaster': ('Plaster', 'Plaster', 'Corner_Exterior_Wood'),
    'brick': ('UnevenBrick', 'UnevenBrick', 'Corner_Exterior_Brick'),
    'mixed': ('UnevenBrick', 'Plaster', 'Corner_Exterior_Brick'),
}


def footprints():
    """Las plantas posibles: las que tienen techo."""
    return sorted(ROOFS)


def recipe(width, depth, stories=1, style='plaster', seed=1, shutters=True, chimney=True, arcade=None, roof=None):
    """Una receta de casa. width (x) y depth (y) en metros, de las plantas de ROOFS.
    arcade: 'front' (la planta baja del frente es un portal: arcos en la fachada y el muro con la puerta 2 m adentro) o
    'all' (arcos en los cuatro lados y nada más: un kiosco). roof: 'tower' (el techo de torre, para un campanario de 4 x 4
    cuyo último piso son arcos)."""
    if (width, depth) not in ROOFS:
        raise ValueError(f'no hay techo para {width}x{depth}; plantas: {footprints()}')
    if roof == 'tower' and (width, depth) != (4, 4):
        raise ValueError('el techo de torre es de 4 x 4')
    r = {'kit': KIT, 'width': width, 'depth': depth, 'stories': stories, 'style': style, 'seed': seed,
         'shutters': shutters, 'chimney': chimney}
    if arcade:
        r['arcade'] = arcade     # solo si se pide: las recetas de antes conservan su nombre
    if roof:
        r['roof'] = roof
    return r


def name_of(r):
    if r['kit'] == OFFICE_KIT:
        key = hashlib.sha1((json.dumps(r, sort_keys=True) + str(OFFICE_VERSION)).encode()).hexdigest()[:8]
        return f'Building_Office_{r["width"]}x{r["depth"]}_{r["stories"]}_{r["style"]}_{key}'
    key = hashlib.sha1((json.dumps(r, sort_keys=True) + str(VERSION) + str(MAX_TRIANGLES)).encode()).hexdigest()[:8]
    return f'Building_House_{r["width"]}x{r["depth"]}_{r["stories"]}_{r["style"]}_{key}'


# --- Oficinas (Downtown City) ---
#
# Las piezas de muro miden 2 o 4 m de ancho y 3 de alto, con la cara de afuera en el pivote (z = 0 del modelo) y el grueso
# hacia adentro: la misma convención que las de la casa. Las columnas de esquina tienen el pivote en la esquina de afuera
# (base, tramo, remate y tapa); los pretiles (Cornice_*) miden 1 m sobre el último piso, en tramos de 2 m cuyos extremos
# (90Angle_L a la izquierda y _R a la derecha, vistos desde afuera) doblan la esquina; el techo son losas de 4 x 4.
OFFICE_KIT = 'downtown'
OFFICE_VERSION = 3
OFFICE_MAX_TRIANGLES = 20000   # sus ventanas son más finas que las de la casa: a 9000 se deformaban
OFFICE_STORY = 3.0
OFFICE_STYLES = {
    # planta baja (local: vitrina, muro), pisos de arriba (ventanas de 2 m, de 4 m, muro), esquinas (base, tramo, remate,
    # tapa), pretil (centro, esquina), puerta
    'brick': {'shop': 'Metal_FirstFloor_Window', 'ground_wall': 'Brick_Plain_3',
              'windows2': ['Brick_Window_Trim', 'Brick_Window_Trim_Single', 'Brick_Window_Square_Single'],
              'windows4': ['Brick_Inset_Window', 'Brick_RedWhite_DoubleWindow', 'Brick_Window_CurvedDouble', 'Brick_Inset_Window_Curved'],
              'wall': 'Brick_Plain_3',
              'corner': ('Brick_CornerColumn_Bottom', 'Brick_CornerColumn_Center', 'Brick_CornerColumn_Top', 'Brick_CornerColumn_Cap'),
              'cornice': ('Cornice_Brick_Center', 'Cornice_Brick_90Angle_L', 'Cornice_Brick_90Angle_R'), 'door': 'DoorFrame_Trim'},
    'trim': {'shop': 'Trim_FirstFloor_Window_001', 'ground_wall': 'Trim_FirstFloor_Wall',
             'windows2': ['Trim_Window'], 'windows4': [], 'wall': 'Trim_Plain_3',
             'corner': ('Trim_Column_Bottom', 'Trim_Column_Center', 'Trim_Column_Top', None),
             'cornice': ('Cornice_Trim_Center', 'Cornice_Trim_90Angle_L', 'Cornice_Trim_90Angle_R'), 'door': 'DoorFrame_Trim'},
    'metal': {'shop': 'Metal_FirstFloor_Window', 'ground_wall': 'Metal_FirstFloor_Wall',
              'windows2': ['Metal_FullWindow', 'Metal_Window_Half'], 'windows4': ['Metal_Window'], 'wall': 'Metal_Plain_3',
              'corner': ('Metal_Column_Bottom', 'Metal_Column_Center', 'Metal_Column_Top', None),
              'cornice': ('Cornice_Metal_Center', 'Cornice_Metal_90Angle_L', 'Cornice_Metal_90Angle_R'), 'door': 'DoorFrame_Metal_Single'},
}


def office_recipe(width, depth, stories=4, style='brick', seed=1):
    """Una receta de oficina: planta width (x) por depth (y) en múltiplos de 4 m, 'stories' pisos de 3 m."""
    if width % 4 or depth % 4 or width < 8 or depth < 8:
        raise ValueError(f'una oficina mide múltiplos de 4 m, de 8 en adelante (no {width}x{depth})')
    if style not in OFFICE_STYLES:
        raise ValueError(f'estilo {style}: {sorted(OFFICE_STYLES)}')
    return {'kit': OFFICE_KIT, 'width': width, 'depth': depth, 'stories': stories, 'style': style, 'seed': seed}


def office_parts(r):
    """Las piezas de la oficina: [(modelo, x, y, z, giro)] en ejes del edificio (el frente al sur, como la casa)."""
    rng = random.Random(r['seed'])
    w, d, stories = r['width'], r['depth'], r['stories']
    s = OFFICE_STYLES[r['style']]
    out = []
    sides = [((0.0, -d / 2), (1, 0), 0.0, w),     # sur, el frente
             ((w / 2, 0.0), (0, 1), 90.0, d),      # este
             ((0.0, d / 2), (-1, 0), 180.0, w),    # norte
             ((-w / 2, 0.0), (0, -1), 270.0, d)]   # oeste
    # un patrón de ventanas por edificio: tramos de 4 m (una pieza de 4, o dos de 2) que se repiten en todos los pisos
    use4 = bool(s['windows4']) and rng.random() < 0.6
    window4 = rng.choice(s['windows4']) if use4 else None
    window2 = rng.choice(s['windows2'])
    for si, ((cx, cy), (ax, ay), yaw, length) in enumerate(sides):
        front = si == 0
        bays = length // 4
        door_bay = bays // 2
        for story in range(stories):
            z = story * OFFICE_STORY
            for b in range(bays):
                along = (b - (bays - 1) / 2) * 4.0
                x, y = cx + ax * along, cy + ay * along
                halves = [(x - ax * 1.0, y - ay * 1.0), (x + ax * 1.0, y + ay * 1.0)]
                if story == 0:
                    # planta baja: el frente es de locales (vitrinas) con la puerta al centro; los lados alternan
                    for h, (hx, hy) in enumerate(halves):
                        if front and b == door_bay and h == 0:
                            out.append((s['door'], hx, hy, z, yaw))
                            # la hoja (1 m, con el pivote en su orilla derecha) al centro del marco, un poco adentro
                            inward = (-1 if cx > 0 else 1 if cx < 0 else 0, -1 if cy > 0 else 1 if cy < 0 else 0)
                            out.append(('Door_1', hx + ax * 0.5 + inward[0] * 0.1, hy + ay * 0.5 + inward[1] * 0.1, z, yaw))
                        elif front or (b + h) % 2 == 0:
                            out.append((s['shop'], hx, hy, z, yaw))
                        else:
                            out.append((s['ground_wall'], hx, hy, z, yaw))
                elif not front and b % 3 == 2:
                    for hx, hy in halves:
                        out.append((s['wall'], hx, hy, z, yaw))   # un tramo ciego de vez en cuando en los lados
                elif window4:
                    out.append((window4, x, y, z, yaw))
                else:
                    for hx, hy in halves:
                        out.append((window2, hx, hy, z, yaw))
        # el pretil sobre el último piso, cada 2 m; sus extremos doblan la esquina
        top = stories * OFFICE_STORY
        slots = length // 2
        for k in range(slots):
            along = (k - (slots - 1) / 2) * 2.0
            piece = s['cornice'][1] if k == 0 else s['cornice'][2] if k == slots - 1 else s['cornice'][0]
            out.append((piece, cx + ax * along, cy + ay * along, top, yaw))
    top = stories * OFFICE_STORY
    bottom, center, upper, cap = s['corner']
    for (cx, cy), yaw in (((w / 2, -d / 2), 0.0), ((w / 2, d / 2), 90.0), ((-w / 2, d / 2), 180.0), ((-w / 2, -d / 2), 270.0)):
        for story in range(stories):
            piece = bottom if story == 0 else upper if story == stories - 1 else center
            out.append((piece, cx, cy, story * OFFICE_STORY, yaw))
        if cap:
            out.append((cap, cx, cy, top - 2.0, yaw))
    # el techo: losas de 4 x 4 (su cara queda 0.2 m bajo el pivote)
    for i in range(w // 4):
        for j in range(d // 4):
            out.append(('Roof_4x4', -w / 2 + 2.0 + 4.0 * i, -d / 2 + 2.0 + 4.0 * j, top + 0.2, 0.0))
    return out


def parts(r):
    """Las piezas del edificio: [(modelo, x, y, z, giro)] en ejes del edificio."""
    if r['kit'] == OFFICE_KIT:
        return office_parts(r)
    rng = random.Random(r['seed'])
    w, d, stories = r['width'], r['depth'], r['stories']
    ground_mat, upper_mat, corner = STYLES[r['style']]
    out = []
    # los cuatro lados: (centro del lado, dirección a lo largo, giro de la cara de afuera, número de piezas)
    sides = [((0.0, -d / 2), (1, 0), 0.0, w // 2),     # sur, el frente
             ((w / 2, 0.0), (0, 1), 90.0, d // 2),      # este
             ((0.0, d / 2), (-1, 0), 180.0, w // 2),    # norte
             ((-w / 2, 0.0), (0, -1), 270.0, d // 2)]   # oeste
    door_side, door_slot = 0, (w // 2) // 2
    arcade, tower = r.get('arcade'), r.get('roof') == 'tower'
    for story in range(stories):
        z = story * STORY
        material = ground_mat if story == 0 else upper_mat
        for s, ((cx, cy), (ax, ay), yaw, count) in enumerate(sides):
            for k in range(count):
                along = (k - (count - 1) / 2) * WALL
                x, y = cx + ax * along, cy + ay * along
                if arcade == 'all' or (tower and story == stories - 1):
                    out.append(('Wall_Arch', x, y, z, yaw))   # kiosco, o los vanos de las campanas
                    continue
                if arcade == 'front' and story == 0 and s == 0:
                    # el portal: el arco en la fachada, el techo del pasillo, y el muro (con la puerta) 2 m adentro
                    out.append(('Wall_Arch', x, y, z, yaw))
                    out.append(('Floor_Brick', x, y + 1.0, z + STORY - 0.02, yaw))
                    if k == door_slot:
                        out.append((f'Wall_{material}_Door_Flat', x, y + 2.0, z, yaw))
                        out.append(('Door_1_Flat', x - ax * 0.53, y + 2.0, z, yaw))
                    else:
                        out.append((f'Wall_{material}_Window_Wide_Flat', x, y + 2.0, z, yaw))
                        out.append(('Window_Wide_Flat1', x, y + 2.0, z, yaw))
                    continue
                if story == 0 and s == door_side and k == door_slot:
                    out.append((f'Wall_{material}_Door_Flat', x, y, z, yaw))
                    out.append(('Door_1_Flat', x - ax * 0.53, y - ay * 0.53, z, yaw))
                    continue
                window = rng.random() < (0.55 if story == 0 else 0.7)
                if window:
                    out.append((f'Wall_{material}_Window_Wide_Flat', x, y, z, yaw))
                    out.append(('Window_Wide_Flat1', x, y, z, yaw))
                    if r['shutters'] and material == 'Plaster':
                        out.append((rng.choice(['WindowShutters_Wide_Flat_Open', 'WindowShutters_Wide_Flat_Closed']), x, y, z, yaw))
                else:
                    plain = 'Wall_Plaster_WoodGrid' if material == 'Plaster' and rng.random() < 0.4 else f'Wall_{material}_Straight'
                    out.append((plain, x, y, z, yaw))
        # esquinas: la pieza mira hacia afuera en diagonal; se pone en cada esquina con el giro del lado que empieza ahí
        for (cx, cy), yaw in (((w / 2, -d / 2), 0.0), ((w / 2, d / 2), 90.0), ((-w / 2, d / 2), 180.0), ((-w / 2, -d / 2), 270.0)):
            out.append((corner, cx, cy, z, yaw))
    top = stories * STORY
    if arcade == 'all':
        for i in range(w // 2):   # el piso del kiosco
            for j in range(d // 2):
                out.append(('Floor_Brick', -w / 2 + 1.0 + 2.0 * i, -d / 2 + 1.0 + 2.0 * j, 0.02, 0.0))
    if tower:
        out.append(('Roof_Tower_RoundTiles', 0.0, 0.0, top, 0.0))
        return out
    out.append((ROOFS[(w, d)], 0.0, 0.0, top, 0.0))
    # los hastiales (el triángulo bajo el techo) en los lados cortos del techo: al sur y al norte (la cumbrera va de norte a sur)
    if w in GABLES:
        out.append((GABLES[w], 0.0, -d / 2, top, 0.0))
        out.append((GABLES[w], 0.0, d / 2, top, 180.0))
    if r['chimney']:
        out.append((rng.choice(['Prop_Chimney', 'Prop_Chimney2']), w / 2 - 1.0, d / 4, top, 0.0))
    return out


def build(recipes):
    """Hace los modelos de las recetas que falten (Blender junta sus piezas en uno: blender_batch.py) en
    examples/zombies_module/assets/generated/. Devuelve sus nombres, en el orden de las recetas."""
    out = citykit.GENERATED
    out.mkdir(parents=True, exist_ok=True)
    names = [name_of(r) for r in recipes]
    chunks = {}
    for r, name in zip(recipes, names):
        if (out / f'{name}.gltf').is_file() or name in chunks:
            continue
        chunks[name] = [{'path': str(citykit.find_model(asset)), 'x': x, 'y': y, 'z': z, 'yaw': yaw, 'scale': 1.0}
                        for asset, x, y, z, yaw in parts(r)]
        (out / f'{name}.json').write_text(json.dumps(r, sort_keys=True), encoding='utf-8')   # su receta, junto al modelo
    if chunks:
        plan = out / 'plan.json'
        budget = OFFICE_MAX_TRIANGLES if all(r['kit'] == OFFICE_KIT for r in recipes) else MAX_TRIANGLES
        plan.write_text(json.dumps({'chunks': chunks, 'optimize': {'max_triangles': budget}}), encoding='utf-8')
        result = subprocess.run([str(citykit.BLENDER), '--background', '--python', str(HERE / 'blender_batch.py'), '--',
                                 '--plan', str(plan), '--out', str(out)], capture_output=True, text=True)
        missing = [n for n in chunks if not (out / f'{n}.gltf').is_file()]
        if missing:
            raise RuntimeError(f'Blender no hizo {missing}:\n' + (result.stdout + result.stderr)[-3000:])
        print(f'casas nuevas: {len(chunks)} ({", ".join(chunks)})')
    return names


OPTIMIZED_SUFFIX = '_Opt'


def optimized(assets, max_triangles=15000):
    """Copias optimizadas de modelos pesados (los edificios enteros de Downtown: 18 a 45 mil triángulos): la misma pasada
    que a las casas (materiales repetidos en uno, caras coplanares juntas, simplificar hasta max_triangles). Quedan en
    examples/zombies_module/assets/generated/ como <modelo>_Opt; devuelve sus nombres."""
    out = citykit.GENERATED
    out.mkdir(parents=True, exist_ok=True)
    names = [f'{a}{OPTIMIZED_SUFFIX}' for a in assets]
    chunks = {n: [{'path': str(citykit.find_model(a)), 'x': 0.0, 'y': 0.0, 'z': 0.0, 'yaw': 0.0, 'scale': 1.0}]
              for a, n in zip(assets, names) if not (out / f'{n}.gltf').is_file()}
    if chunks:
        plan = out / 'plan_opt.json'
        plan.write_text(json.dumps({'chunks': chunks, 'optimize': {'max_triangles': max_triangles}}), encoding='utf-8')
        result = subprocess.run([str(citykit.BLENDER), '--background', '--python', str(HERE / 'blender_batch.py'), '--',
                                 '--plan', str(plan), '--out', str(out)], capture_output=True, text=True)
        missing = [n for n in chunks if not (out / f'{n}.gltf').is_file()]
        if missing:
            raise RuntimeError(f'Blender no hizo {missing}:\n' + (result.stdout + result.stderr)[-3000:])
        for line in (result.stdout or '').splitlines():
            if line.startswith('OPTIMIZED'):
                print(line)
    return names
