"""Lo que trae cada lote además de su edificio: la decoración de la fachada, la banqueta de enfrente, la azotea, el patio
de atrás y el coche en la calle. La idea de los mapas de demostración de los paquetes (cada tienda con su decoración, las fachadas
vestidas); aquí, genérica, por rol.

Una receta es la lista de anexos de un rol de edificio (el preset la trae en "lots"):

    {"lots": {"shop": [
        {"role": "awning", "at": "wall", "height": 2.6, "center": true, "chance": 0.8},
        {"role": "sign", "at": "wall", "height": [3.4, 4.2], "center": true, "chance": 0.9},
        {"role": "aircon", "at": "wall", "height": [2.6, 6.0], "count": [0, 2]},
        {"role": "ivy", "at": "wall", "height": 0.0, "count": [0, 2], "chance": 0.5},
        {"role": "shopfront", "at": "front", "out": [0.5, 1.4], "count": [1, 3]},
        {"role": "car", "at": "curb", "chance": 0.5},
        {"role": "doghouse", "at": "back", "out": [1.0, 3.0], "chance": 0.3},
        {"role": "roof_clutter", "at": "roof", "count": [1, 3], "only": "Apartment|Shop"}]}}

'at':
  wall   sobre la fachada, a 'height' metros del piso, pegado a ella (mira a la calle); 'center' lo centra
  front  en la banqueta, a 'out' metros de la fachada
  curb   en la calle, junto a la guarnición frente al lote, a lo largo (un coche estacionado)
  back   atrás del edificio, en el patio, a 'out' metros
  roof   sobre la azotea, adentro de la planta
  word   una palabra con letras sueltas sobre la fachada (word_on_facade), con un tablero detrás si lo pide
  roof_front  un letrero sobre la azotea junto al frente, mirando a la calle, parado según la postura de su modelo
              (el tema: "pose": {"tilt": grados sobre el eje X del nivel, "turn": grados sobre el vertical})
'chance' (1 por omisión) para el anexo entero; 'count' [a, b] cuántos; 'only' una expresión que el modelo del edificio
debe cumplir (las azoteas planas); 'min_height': solo en edificios de esa altura o más; 'corner': true solo en los lotes
de esquina; 'yaw': facing | along | random.
Lo que va en el piso (front, curb, back) ocupa su lugar en la grilla y respeta lo ocupado; lo de la pared y la azotea no
choca entre sí (cada edificio lleva la cuenta de lo que ya tiene).
"""
import math
import random
import re

from shapely.geometry import Point

import citykit_path  # noqa: F401
import citykit  # noqa: E402
import parcels  # noqa: E402


def _span(value, rng):
    if isinstance(value, (list, tuple)):
        return rng.uniform(value[0], value[1])
    return float(value)


def frame(model, entity):
    """El marco de un edificio puesto: la base de su fachada (a -> b), su normal hacia la calle, su fondo, su altura."""
    scale = entity.get('scale', 1.0)
    corners = citykit.footprint(model['low'], model['high'], entity['x'], entity['y'], entity['yaw'], scale)
    a, b = corners[3], corners[2]
    yaw = math.radians(entity['yaw'])
    n = (math.sin(yaw), -math.cos(yaw))
    width = math.hypot(b[0] - a[0], b[1] - a[1])
    t = ((b[0] - a[0]) / width, (b[1] - a[1]) / width) if width > 1e-6 else (1.0, 0.0)
    depth = math.hypot(corners[1][0] - corners[2][0], corners[1][1] - corners[2][1])
    return {'a': a, 'b': b, 't': t, 'n': n, 'width': width, 'depth': depth, 'top': model['high'][1] * scale,
            'corners': corners, 'model': model, 'scale': scale}


# what a shop's word is when its roof shows one of these signs (the sign model's name holds the key)
ROOF_WORDS = {'Beer': 'CHEVE RICA', 'Donut': 'DONAS', 'Pizza': 'PIZZAS', 'Icecream': 'NIEVES', 'Soda': 'REFRESCOS',
              'Hotdog': 'HOT DOGS', 'Burger': 'HAMBURGUESAS', 'Milkshake': 'MALTEADAS', 'Taco': 'TACOS', 'Coffee': 'CAFE',
              'Chicken': 'POLLOS', 'Noodle': 'FIDEOS'}


def wall_inset(model, low, high):
    """How far the model's front wall stands behind its box's front (meters, unscaled) between heights 'low' and 'high':
    an eave, a porch or a jutting sign band sets the box's front ahead of the wall things hang on."""
    key = (model.get('path'), round(low, 1), round(high, 1))
    if key not in _insets:
        _insets[key] = 0.0
        if model.get('path'):
            v = citykit.model_vertices(citykit.REPO / model['path'])
            band = v[(v[:, 1] >= low) & (v[:, 1] <= high)]
            if len(band):
                # the front most of what stands in the band, over the middle of the width (not a side post or pillar)
                x0, x1 = model['low'][0], model['high'][0]
                mid = band[(band[:, 0] > x0 + 0.25 * (x1 - x0)) & (band[:, 0] < x1 - 0.25 * (x1 - x0))]
                if len(mid):
                    _insets[key] = max(0.0, float(model['high'][2] - mid[:, 2].max()))
    return _insets[key]


_insets = {}


def dims(model):
    """Ancho, fondo y alto de un modelo (metros)."""
    return model['size'][0], model['size'][1], model['size'][2]


def dress(entity, recipe, catalog, models_for, occupancy, regions, rng, panels=None, counts=None):
    """Los anexos de un edificio puesto ({asset, x, y, yaw, scale, role, corner?}) según su receta. Devuelve entidades
    con altura ('z')."""
    model = catalog['models'][entity['asset']]
    f = frame(model, entity)
    a, t, n, width = f['a'], f['t'], f['n'], f['width']
    name = entity['asset'].rsplit('/', 1)[-1]
    wall_used = []    # (s0, s1, z0, z1) en la fachada
    roof_front_taken = False
    word_done = False
    roof_used = []
    out = []
    road = regions.get('road')
    walk = regions.get('walk')
    yard = regions.get('yard')

    def ground_ok(foot, within):
        if within is not None and not within.contains(foot):
            return False
        claim = foot.buffer(0.15)
        if occupancy.hits(claim):
            return False
        occupancy.add(foot.buffer(0.05))
        return True

    # the roof's sign before the facade's word: the word says what the sign shows (a beer glass: CHEVE RICA)
    recipe = sorted(recipe, key=lambda sp: 0 if sp['at'] == 'roof_front' else 1)
    roof_word = None
    for spec in recipe:
        # a building told its word ("word": the e-Ozzo at the back of a gas station) gets its sign whatever the draws
        forced = spec['at'] == 'word' and entity.get('word')
        if rng.random() > spec.get('chance', 1.0) and not forced:
            continue
        if spec.get('decay') and entity.get('pristine'):
            continue   # (a pristine building, as the e-Ozzo: no ivy, no junk on the roof)
        if spec.get('only') and not re.search(spec['only'], name):
            continue
        if spec.get('corner') and not entity.get('corner') and not forced:
            continue
        if f['top'] < spec.get('min_height', 0.0):
            continue
        models = models_for(spec['role']) if spec['at'] != 'word' else ['(letters)']
        if not models:
            continue
        count = rng.randint(*spec['count']) if 'count' in spec else 1
        at = spec['at']
        for _ in range(count):
            asset = rng.choice(models)
            m = catalog['models'].get(asset, {'size': [0, 0, 0], 'low': [0, 0, 0], 'high': [0, 0, 0]})
            w, d, h = dims(m)
            scale = _span(spec.get('scale', 1.0), rng)
            w, d, h = w * scale, d * scale, h * scale
            if at == 'wall':
                if w > width - 0.4:
                    continue
                z = _span(spec.get('height', 3.0), rng)
                if z + h > f['top'] + 0.5 and not spec.get('above'):
                    continue
                s = (width - w) / 2 if spec.get('center') else rng.uniform(0.2, width - w - 0.2)
                if any(s < u1 and s + w > u0 and z < v1 and z + h > v0 for u0, u1, v0, v1 in wall_used):
                    continue
                wall_used.append((s, s + w, z, z + h))
                # su espalda (la Z menor del modelo) contra la pared, su centro a lo ancho en 'c', su base a 'z'
                c = s + w / 2 - (m['low'][0] + m['high'][0]) / 2 * scale
                # against the wall itself, behind the box's front where an eave or a porch sticks out
                inset = wall_inset(model, z, z + h) * entity.get('scale', 1.0)
                off = spec.get('gap', 0.03) - m['low'][2] * scale - inset
                x, y = a[0] + t[0] * c + n[0] * off, a[1] + t[1] * c + n[1] * off
                out.append({'asset': asset, 'x': x, 'y': y, 'z': z - m['low'][1] * scale, 'yaw': entity['yaw'], 'scale': scale,
                            'role': spec['role'], **({'tint': spec['tint']} if spec.get('tint') else {})})
            elif at in ('front', 'back'):
                s = rng.uniform(w / 2 + 0.2, max(w / 2 + 0.2, width - w / 2 - 0.2))
                dist = _span(spec.get('out', [0.5, 1.2]), rng) + d / 2
                if at == 'front':
                    x, y = a[0] + t[0] * s + n[0] * dist, a[1] + t[1] * s + n[1] * dist
                    yaw = entity['yaw'] if spec.get('yaw', 'facing') == 'facing' else rng.uniform(0, 360)
                    within = walk
                else:
                    back = f['depth'] + dist
                    x, y = a[0] + t[0] * s - n[0] * back, a[1] + t[1] * s - n[1] * back
                    yaw = rng.uniform(0, 360) if spec.get('yaw', 'random') == 'random' else (entity['yaw'] + 180) % 360
                    within = yard
                foot = parcels.footprint_poly(m, x, y, yaw, scale)
                if ground_ok(foot, within):
                    out.append({'asset': asset, 'x': x, 'y': y, 'yaw': yaw, 'scale': scale, 'role': spec['role']})
            elif at == 'curb':
                if road is None:
                    continue
                s = rng.uniform(0.0, width)
                base = Point(a[0] + t[0] * s, a[1] + t[1] * s)
                curb = base.distance(road)
                dist = curb + spec.get('lane', 1.2)
                x, y = base.x + n[0] * dist, base.y + n[1] * dist
                yaw = (entity['yaw'] + rng.choice((90.0, 270.0))) % 360
                foot = parcels.footprint_poly(m, x, y, yaw, scale)
                if ground_ok(foot, road):
                    out.append({'asset': asset, 'x': x, 'y': y, 'yaw': yaw, 'scale': scale, 'role': spec['role']})
            elif at == 'word':
                if word_done:
                    continue
                # the sign over what is already on the wall under it (an awning, an air conditioner): not through it
                above = max((v1 + 0.15 for u0, u1, v0, v1 in wall_used if v1 > spec.get('height', 3.0) - 0.6), default=None)
                told = entity.get('word')
                if roof_word and not told:
                    entity = dict(entity, word=roof_word)
                got = word_on_facade(spec, f, entity, catalog, models_for, rng, panels, counts, above)
                if got:
                    out += got
                    word_done = True
                    wall_used.append((0.0, width, spec.get('height', 3.0) - 0.2, spec.get('height', 3.0) + 1.2))
            elif at == 'roof_front':
                # a sign on the roof by the front edge, facing the street, stood as its model's pose says (tilt about the
                # level's X, then its own turn): its lowest point on the roof
                if roof_front_taken:
                    continue
                pose = m.get('pose', {})
                tilt, turn = pose.get('tilt', 0.0), pose.get('turn', 0.0)
                lo, hi = m['low'], m['high']
                ang = math.radians(tilt)
                lowest = min(-zm * math.sin(ang) * scale + ym * math.cos(ang) * scale
                             for ym in (lo[1], hi[1]) for zm in (lo[2], hi[2]))   # (x, -z, y) del modelo en el nivel, girado sobre X
                inset = _span(spec.get('inset', [0.8, 1.4]), rng)
                s = width / 2 + rng.uniform(-0.15, 0.15) * width
                x, y = (f['a'][0] + t[0] * s - n[0] * inset, f['a'][1] + t[1] * s - n[1] * inset)
                out.append({'asset': asset, 'x': x, 'y': y, 'z': f['top'] - lowest, 'yaw': (entity['yaw'] + turn) % 360,
                            'tilt': tilt, 'scale': scale, 'role': spec['role']})
                roof_front_taken = True
                roof_word = next((w for k, w in ROOF_WORDS.items() if k in asset), None)
            elif at == 'roof':
                margin = max(w, d) / 2 + 0.8
                if width < 2 * margin or f['depth'] < 2 * margin:
                    continue
                s = rng.uniform(margin, width - margin)
                k = rng.uniform(margin, f['depth'] - margin)
                if any(math.hypot(s - u, k - v) < max(w, d) for u, v in roof_used):
                    continue
                roof_used.append((s, k))
                x, y = a[0] + t[0] * s - n[0] * k, a[1] + t[1] * s - n[1] * k
                yaw = (entity['yaw'] + rng.choice((0, 90, 180, 270))) % 360
                out.append({'asset': asset, 'x': x, 'y': y, 'z': f['top'], 'yaw': yaw, 'scale': scale, 'role': spec['role']})
    for e in out:
        e['x'], e['y'], e['yaw'] = round(e['x'], 3), round(e['y'], 3), round(e['yaw'] % 360, 2)
        e['scale'] = round(e['scale'], 3)
        if 'z' in e:
            e['z'] = round(e['z'], 3)
        if e.get('tilt'):
            e['tilt'] = round(e['tilt'], 2)
    return out


def word_on_facade(spec, f, entity, catalog, models_for, rng, panels, counts=None, above=None):
    """A word spelled with letter models (role "letter": one model per letter, its name ending in the letter) across
    the facade at 'height', centered; with "panel" {"color", "band", "pad", "thickness"}, a panel behind it ('panels'
    gets it, the mesh draws it). 'words': ["TACOS", {"word": "E-OZZO", "weight": 2, "max_per_block": 2, "max_per_town": 3,
    "only_on": a role, "panel": {...}}];
    'corner_words' for a corner lot. A dash is drawn by the mesh. Letters it lacks are left out
    of the choice (no word with them)."""
    letters = {}
    for key in models_for(spec.get('letters', 'letter')):
        letters.setdefault(key.rsplit('_', 1)[-1].upper(), key)
    pool = spec.get('corner_words') if entity.get('corner') and spec.get('corner_words') else spec.get('words', [])
    if entity.get('word'):
        # its own word, with the panel the spec gives that word when it has one
        named = lambda w: (w['word'] if isinstance(w, dict) else w).upper()  # noqa: E731
        pool = [w for w in spec.get('words', []) + spec.get('corner_words', []) if named(w) == entity['word'].upper()][:1]             or [entity['word']]
    # each word: "TACOS", or {"word": "OZZO", "weight": 2, "panel": {...}} (its own panel over the spec's)
    items = [w if isinstance(w, dict) else {'word': w} for w in pool]
    usable = [w for w in items if all(c in ' -' or c in letters for c in w['word'].upper())]
    # a word only on some models ("only_on": a role, as the e-Ozzo on a store kept up: the chain's stores are pristine in
    # the worst neighborhood); a role with no models leaves the word free
    def allowed(w):
        if not w.get('only_on'):
            return True
        models = models_for(w['only_on'])
        return not models or entity.get('asset') in models
    usable = [w for w in usable if allowed(w)]
    # a word may have a limit per block ("max_per_block": one e-Ozzo per block)
    counts = {} if counts is None else counts
    zone = entity.get('zone')
    usable = [w for w in usable if counts.get((zone, w['word'].upper()), 0) < w.get('max_per_block', 10 ** 9)]
    # ... and in the whole town ("max_per_town": a chain with a few branches)
    usable = [w for w in usable if counts.get(('*', w['word'].upper()), 0) < w.get('max_per_town', 10 ** 9)]
    if not usable or not letters:
        return []
    # a word the block already has is drawn less (two pozolerias side by side may happen, rarely)
    chosen = rng.choices(usable, [w.get('weight', 1.0) * (0.15 if counts.get((zone, w['word'].upper()), 0) else 1.0)
                                  for w in usable])[0]
    word = chosen['word'].upper()
    counts[(zone, word)] = counts.get((zone, word), 0) + 1
    counts[('*', word)] = counts.get(('*', word), 0) + 1
    if chosen.get('only_on'):
        entity['pristine'] = True   # (the rest of its dressing leaves the decay out)
    gap = spec.get('spacing', 0.08)
    space = spec.get('space', 0.45)
    sized = []
    for c in word:
        if c == ' ':
            sized.append((None, space))
        elif c == '-':
            sized.append(('-', spec.get('dash', 0.4)))   # a dash: no model has one, the mesh draws it
        else:
            m = catalog['models'][letters[c]]
            sized.append((letters[c], m['size'][0]))
    scale = spec.get('scale', 1.0)
    total = sum(w for _, w in sized) * scale + gap * (len(sized) - 1)
    room = f['width'] - 2 * spec.get('margin', 0.4)
    if total > room:
        scale *= room / total
        total = room
        if scale < spec.get('min_scale', 0.45):
            return []
    a, t, n = f['a'], f['t'], f['n']
    z = spec.get('height', 3.0)
    if above is not None:
        z = max(z, above)   # over what the wall already has (an awning)
    panel = chosen.get('panel', spec.get('panel'))
    lift = (panel or {}).get('thickness', 0.0)
    s = (f['width'] - total) / 2
    out = []
    tallest = max((catalog['models'][k]['size'][2] * scale for k, _ in sized if k and k != '-'), default=0.5)
    if f.get('model'):
        # on what stands at the sign's height (the wall, or an eave's band), not out at the box's front
        inset = wall_inset(f['model'], z, z + tallest) * f.get('scale', 1.0)
        a = (a[0] - n[0] * inset, a[1] - n[1] * inset)
    for key, w in sized:
        if key == '-':
            if panels is not None:
                off = 0.03 + lift + 0.03
                p0 = (a[0] + t[0] * (s + 0.05) + n[0] * off, a[1] + t[1] * (s + 0.05) + n[1] * off)
                p1 = (a[0] + t[0] * (s + w * scale - 0.05) + n[0] * off, a[1] + t[1] * (s + w * scale - 0.05) + n[1] * off)
                panels.append({'a': p0, 'b': p1, 'bottom': z + tallest * 0.42, 'top': z + tallest * 0.56, 'thickness': 0.06,
                               'color': spec.get('dash_color', 'sign_white')})
        elif key:
            m = catalog['models'][key]
            c = s + (w * scale) / 2 - (m['low'][0] + m['high'][0]) / 2 * scale
            off = 0.03 + lift - m['low'][2] * scale
            out.append({'asset': key, 'x': a[0] + t[0] * c + n[0] * off, 'y': a[1] + t[1] * c + n[1] * off,
                        'z': z - m['low'][1] * scale, 'yaw': entity['yaw'], 'scale': scale, 'role': 'letter', 'spells': word})
        s += w * scale + gap
    if panel is not None and panels is not None:
        pad = panel.get('pad', 0.25)
        s0 = (f['width'] - total) / 2 - pad
        p0 = (a[0] + t[0] * s0 + n[0] * (0.03 + lift / 2), a[1] + t[1] * s0 + n[1] * (0.03 + lift / 2))
        s1 = s0 + total + 2 * pad
        p1 = (a[0] + t[0] * s1 + n[0] * (0.03 + lift / 2), a[1] + t[1] * s1 + n[1] * (0.03 + lift / 2))
        panels.append({'a': p0, 'b': p1, 'bottom': z - pad, 'top': z + tallest + pad, 'thickness': max(lift, 0.06),
                       'color': panel.get('color', 'sign_red'), 'band': panel.get('band')})
    return out


def dress_all(buildings, lots, catalog, models_for, occupancy, regions, seed, panels=None):
    """Los anexos de todos los edificios, cada uno con su generador (determinista por edificio). 'panels' recibe los
    tableros de los letreros (los dibuja la malla)."""
    out = []
    counts = {}   # (manzana, palabra) -> cuántas van: los límites por manzana de los letreros
    for k, e in enumerate(buildings):
        recipe = lots.get(e['role'])
        if not recipe:
            continue
        rng = random.Random(seed * 7919 + k)
        for got in dress(e, recipe, catalog, models_for, occupancy, regions, rng, panels, counts):
            got['zone'] = e.get('zone')
            got['spawner'] = f'lot:{e["role"]}'
            out.append(got)
    town = {word: n for (zone, word), n in counts.items() if zone == '*'}
    print('signs:', ', '.join(f'{w} {n}' for w, n in sorted(town.items(), key=lambda kv: -kv[1])))
    return out
