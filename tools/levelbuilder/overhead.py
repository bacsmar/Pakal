"""Lo que cuelga sobre las calles: los postes de luz con sus cables a lo largo de una banqueta y, de vez en cuando, un
cable que cruza a la fachada de enfrente; y el papel picado, cordones de banderitas de un lado al otro de la calle (cerca
de las plazas y del malecón). Los cables y las banderitas son mallas nuestras (roadmesh.cable, roadmesh.bunting); los
postes, modelos del tema (rol "power_pole").

El preset lo trae en "overhead":

    {"overhead": {
        "power": {"edges": ["street", "avenue", "hill_street"], "side": 1, "inset": 0.5, "spacing": 24.0,
                  "attach": 0.9, "across": 0.35, "across_height": 5.2},
        "papel_picado": {"edges": ["street", "avenue"], "near": ["plaza", "promenade"], "distance": 45.0,
                         "spacing": 6.0, "height": [4.6, 5.6], "lines": [1, 3], "chance": 0.7}}}

'power': un poste cada 'spacing' metros en un lado de cada calle ('side': 1 o -1), a 'inset' del paño de las fachadas;
el cable va de poste a poste a 'attach' de su altura; 'across': la probabilidad de un cable a la fachada de enfrente.
'papel_picado': en las calles a menos de 'distance' metros de las regiones 'near', grupos de 'lines' cordones cada
'spacing' metros, de fachada a fachada (solo donde hay edificio de los dos lados).
"""
import math
import random

from shapely.geometry import Point

from geom import lines
import parcels
import roadmesh


def _along(line, spacing, rng, start=None):
    d = rng.uniform(0, spacing) if start is None else start
    while d < line.length:
        p = line.interpolate(d)
        q = line.interpolate(min(d + 0.5, line.length))
        r = line.interpolate(max(d - 0.5, 0.0))
        yield p, math.atan2(q.y - r.y, q.x - r.x)
        d += spacing * rng.uniform(0.9, 1.1)


def _clear_of_crossings(net, edge, point, clear):
    return not any(o is not edge and o['kind'] != 'alley' and o['line'].distance(point) < o['width'] / 2 + clear
                   for o in net.edges)


def power_lines(net, spec, mesh, catalog, models_for, occupancy, seed):
    """Los postes (entidades) y sus cables (en 'mesh')."""
    rng = random.Random(seed * 31 + 7)
    models = models_for('power_pole')
    if not models:
        return []
    sidewalk = net.brief['sidewalk']
    out = []
    for e in net.edges:
        if e['kind'] not in spec.get('edges', ['street', 'avenue']) or e.get('bridge'):
            continue
        side = spec.get('side', 1)
        reach = e['width'] / 2 + sidewalk - spec.get('inset', 0.5)
        for piece in lines(e['line'].offset_curve(side * reach)):
            prev = None
            for p, angle in _along(piece, spec.get('spacing', 24.0), rng):
                if not _clear_of_crossings(net, e, p, 5.0):
                    prev = None
                    continue
                asset = rng.choice(models)
                m = catalog['models'][asset]
                yaw = (math.degrees(angle) + 90.0) % 360
                foot = Point(p.x, p.y).buffer(0.3)
                if occupancy.hits(foot):
                    prev = None
                    continue
                occupancy.add(foot)
                top = m['high'][1] * spec.get('attach', 0.9)
                out.append({'asset': asset, 'x': round(p.x, 3), 'y': round(p.y, 3), 'yaw': round(yaw, 2), 'scale': 1.0,
                            'role': 'power_pole', 'spawner': 'power_lines'})
                here = (p.x, p.y, top)
                if prev is not None:
                    mesh.cable(prev, here, sag=0.5)
                    mesh.cable((prev[0], prev[1], prev[2] - 0.35), (here[0], here[1], here[2] - 0.35), sag=0.6)
                prev = here
                # un cable a la fachada de enfrente (la acometida de una casa)
                if rng.random() < spec.get('across', 0.35):
                    nx, ny = math.cos(angle + math.pi / 2), math.sin(angle + math.pi / 2)
                    far = 2 * reach * -side
                    q = (p.x + nx * far, p.y + ny * far, spec.get('across_height', 5.2))
                    mesh.cable(here, q, sag=0.8, thickness=0.03)
    return out


def papel_picado(net, spec, mesh, regions, seed):
    """Cordones de papel picado de fachada a fachada en las calles cerca de 'near'. Devuelve cuántos."""
    rng = random.Random(seed * 37 + 11)
    near = [regions.get(r) for r in spec.get('near', ['plaza', 'promenade'])]
    near = [g for g in near if g is not None and not g.is_empty]
    buildings = regions.get('buildings')
    if buildings is None or buildings.is_empty:
        return 0
    facades = buildings.buffer(1.2)
    sidewalk = net.brief['sidewalk']
    count = 0
    for e in net.edges:
        if e['kind'] not in spec.get('edges', ['street', 'avenue']) or e.get('bridge'):
            continue
        if near and min(g.distance(e['line']) for g in near) > spec.get('distance', 45.0):
            continue
        if rng.random() > spec.get('chance', 0.7):
            continue
        reach = e['width'] / 2 + sidewalk + 0.3
        for p, angle in _along(e['line'], spec.get('spacing', 6.0), rng):
            if not _clear_of_crossings(net, e, p, 3.0):
                continue
            nx, ny = math.cos(angle + math.pi / 2), math.sin(angle + math.pi / 2)
            for k in range(rng.randint(*spec.get('lines', [1, 3]))):
                shift = (k - 1) * 1.2
                tx, ty = math.cos(angle) * shift, math.sin(angle) * shift
                a = (p.x + tx + nx * reach, p.y + ty + ny * reach)
                b = (p.x + tx - nx * reach, p.y + ty - ny * reach)
                if not (facades.contains(Point(a)) and facades.contains(Point(b))):
                    continue
                h0, h1 = rng.uniform(*spec.get('height', [4.6, 5.6])), rng.uniform(*spec.get('height', [4.6, 5.6]))
                # los colores de un cordón, de la paleta revuelta
                colors = roadmesh.FLAGS[:]
                rng.shuffle(colors)
                mesh.bunting((a[0], a[1], h0), (b[0], b[1], h1), rng, sag=rng.uniform(0.4, 0.8), materials=colors)
                count += 1
    return count
