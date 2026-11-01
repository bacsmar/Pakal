"""Plazas como las de México: andadores empedrados que van de las esquinas y de los lados al centro (al kiosco), y entre
ellos jardineras (pasto con un borde bajo de piedra) llenas de árboles, arbustos y flores; bancas a la orilla de las
jardineras, mirando al andador. Por tipo:

    civic     la plaza principal: andadores en cruz y en diagonal, jardineras en todos los gajos, el kiosco al centro
    children  una plazuela para niños: una jardinera se vuelve el área de juegos (arenero con borde, columpios,
              resbaladilla), árboles en las orillas
    dance     una plazuela para baile: un piso abierto grande al centro (el kiosco o nada), jardineras solo en el anillo

El brief los pide en "plazas": ["civic", "children"] (uno por plaza, en orden; se repite el último). layout() devuelve
las jardineras, los areneros y el centro; draw() las pone en la malla. Las jardineras chocan llenas (coastal.py las
exporta como colisión rellena): nadie camina dentro ni aparece encerrado en ellas.
"""
import math

from shapely.geometry import LineString, Point
from shapely.ops import unary_union

from geom import polygons

DEFAULTS = {
    'walk': 2.4,          # el andador de la orilla (la plaza menos esto es donde van las jardineras)
    'path': 2.6,          # el ancho de los andadores al centro
    'center': 6.5,        # el radio libre alrededor del kiosco
    'dance_center': 11.0,
    'min_bed': 10.0,      # m2: lo más chico que queda como jardinera
    'curb': 0.32,         # la altura del borde de la jardinera
    'soil': 0.2,          # la altura del pasto adentro
}


def layout(poly, kind, rng, spec=None):
    s = dict(DEFAULTS, **(spec or {}))
    c = poly.centroid
    rect = poly.minimum_rotated_rectangle
    corners = list(rect.exterior.coords)[:4]
    mids = [((a[0] + b[0]) / 2, (a[1] + b[1]) / 2) for a, b in zip(corners, corners[1:] + corners[:1])]
    reach = max(math.dist((c.x, c.y), p) for p in corners) + 2
    paths = []
    targets = corners + mids if kind != 'dance' else mids
    for tx, ty in targets:
        dx, dy = tx - c.x, ty - c.y
        n = math.hypot(dx, dy) or 1.0
        paths.append(LineString([(c.x, c.y), (c.x + dx / n * reach, c.y + dy / n * reach)]))
    path_area = unary_union([p.buffer(s['path'] / 2, cap_style=2) for p in paths])
    radius = s['dance_center'] if kind == 'dance' else s['center']
    inner = poly.buffer(-s['walk'], join_style=2)
    beds_area = inner.difference(path_area).difference(Point(c.x, c.y).buffer(radius))
    beds = [b.buffer(-0.15, join_style=2) for b in polygons(beds_area) if b.area >= s['min_bed']]
    beds = [b for b in beds if not b.is_empty and b.area >= s['min_bed'] * 0.6]
    pits = []
    if kind == 'children' and beds:
        # los dos gajos más grandes son el área de juegos: areneros con su borde, un poco adentro
        for biggest in sorted(beds, key=lambda b: -b.area)[:2]:
            beds.remove(biggest)
            pits += [p for p in polygons(biggest.buffer(-0.4, join_style=2)) if p.area > 6.0]
    return {'kind': kind, 'center': (c.x, c.y), 'clear': radius, 'beds': beds, 'pits': pits, 'paths': path_area,
            'spec': s}


def draw(plan, mesh):
    """Las jardineras (pasto levantado con borde de piedra) y los areneros (arena con borde bajo)."""
    s = plan['spec']
    edge = s.get('material', 'stone')
    for bed in plan['beds']:
        mesh.flat(bed, s['soil'], 'grass')
        mesh.ring_walls(bed, 0.0, s['curb'], edge)
        mesh.flat(bed.difference(bed.buffer(-0.22, join_style=2)), s['curb'], edge)   # la corona del borde
        mesh.ring_walls(bed.buffer(-0.22, join_style=2), s['soil'], s['curb'], edge, outward=False)   # y su cara de adentro
    for pit in plan['pits']:
        mesh.flat(pit, 0.04, 'sand')
        mesh.ring_walls(pit, 0.0, 0.18, 'stone')
