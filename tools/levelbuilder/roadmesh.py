"""Las calles como mallas propias a lo largo de las curvas de la red (streetnet.py), en cuadros de 32 m.

- Asfalto: la unión de las calles, triangulada, con la textura de asfalto de Downtown City repetida cada 6 m (UV en
  coordenadas del mundo: los cuadros empatan). 10 cm abajo del resto: las guarniciones son las paredes que bajan a él.
- Banquetas y lotes: concreto a la altura 0. Malecón, plazas, callejones y el puente peatonal: el empedrado (UnevenBrick
  del Medieval Village).
- Líneas: doble amarilla en las avenidas, discontinua blanca en las calles, cortadas en los cruces; cebras a los lados de
  cada cruce. Color plano, sin textura.
- Puentes: el asfalto sigue sobre el río; pretiles de concreto a los lados.

Cada cuadro es un .gltf con sus primitivas por material y un .bin; las texturas se copian una vez a _textures/ junto a
ellos. Ejes del glTF: x este, y arriba, z sur (= -y del nivel).
"""
import json
import math
import shutil
import struct
from pathlib import Path

import numpy as np
from shapely.geometry import LineString, Point, box
from shapely.ops import unary_union

from geom import lines, polygons, triangulate

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
CITY_TEX = REPO / 'examples/zombies_module/assets/source/downtown-city/Textures'
MEDIEVAL_TEX = REPO / 'examples/zombies_module/assets/source/medieval-village/Textures'
OWN = REPO / 'examples/zombies_module/assets/own'
NATURE_TEX = REPO / 'examples/zombies_module/assets/source/nature-kit/Textures'
CELL = 32.0
ROAD_Y = -0.10      # el asfalto, bajo las banquetas
WATER_SEA = -0.9
WATER_RIVER = -1.3

# material -> texturas (archivo, en el paquete), cuántos metros cubre una repetición, rugosidad o color plano
MATERIALS = {
    'asphalt': {'detile': (0.6, 0.12), 'color': CITY_TEX / 'T_Concrete_Asphalt_BaseColor.png', 'normal': CITY_TEX / 'T_Concrete_Normal.png',
                'orm': CITY_TEX / 'T_Concrete_ORM.png', 'repeat': 6.0},
    'concrete': {'detile': (0.5, 0.1), 'color': CITY_TEX / 'T_Concrete_BaseColor.png', 'normal': CITY_TEX / 'T_Concrete_Normal.png',
                 'orm': CITY_TEX / 'T_Concrete_ORM.png', 'repeat': 3.0},
    'cobble': {'detile': (0.5, 0.12), 'color': MEDIEVAL_TEX / 'T_UnevenBrick_BaseColor.png', 'normal': MEDIEVAL_TEX / 'T_UnevenBrick_Normal.png',
               'repeat': 2.5, 'roughness': 0.9},
    'dirt': {'detile': (0.7, 0.15), 'color': CITY_TEX / 'T_Dirt_BaseColor.png', 'normal': CITY_TEX / 'T_Dirt_Normal.png',
             'orm': CITY_TEX / 'T_Dirt_ORM.png', 'repeat': 5.0},
    # a seawall promenade's paving: pale concrete slabs with dark joints (make_malecon_paving, 4 by 4 slabs a repeat)
    'malecon': {'detile': (0.4, 0.08), 'color': REPO / 'build-win64/bin/Assets/levelgen/_decals/malecon_paving.png',
                'repeat': 4.0, 'roughness': 0.85},
    # a well-off plaza's tiles (POLYGON_Shops' ground: its texture covers 10 m)
    # (a little darker than the texture, which glared under the sun, and glazed: a soft highlight)
    'plaza_tiles': {'color': REPO / 'build-win64/bin/Assets/levelgen/_decals/plaza_tiles.png', 'repeat': 4.0,
                    'factor': [0.82, 0.8, 0.77, 1.0], 'roughness': 0.45},
    # (the ground's relief from the dirt's normal map, as the cobblestone has its own: flat, a planter's lawn read as paint)
    'grass': {'detile': (0.7, 0.18), 'color': OWN / 'Ground_Grass_9x9.png', 'normal': CITY_TEX / 'T_Dirt_Normal.png',
              'repeat': 3.0, 'roughness': 0.95},
    'stone': {'color': MEDIEVAL_TEX / 'T_RockTrim_BaseColor.png', 'normal': MEDIEVAL_TEX / 'T_RockTrim_Normal.png',
              'orm': MEDIEVAL_TEX / 'T_RockTrim_ORM.png', 'repeat': 3.0},
    # (its color a little under the texture's: under the afternoon sun the bare texture burned to white)
    'sand': {'detile': (0.6, 0.12), 'color': REPO / 'build-win64/bin/Assets/levelgen/_decals/beach_sand.png', 'repeat': 4.0, 'roughness': 1.0,
             'factor': [0.76, 0.69, 0.56, 1.0]},
    # la arena que se derrama sobre el adoquín donde la playa llega a él: la misma arena, desvanecida a lo ancho de la
    # franja con un borde irregular (terrain.sand_fringe la hace la primera vez que hace falta)
    'sand_fringe': {'color': REPO / 'build-win64/bin/Assets/levelgen/_decals/beach_sand_fringe.png', 'alpha': 'BLEND',
                    'repeat': 4.0, 'roughness': 1.0, 'factor': [0.76, 0.69, 0.56, 1.0]},
    # the wet sand where the sea meets the beach: a foam line at the water, a dark shiny band fading into the dry sand
    # (terrain.shore_wet lays it along the waterline; make_wet_sand paints it the first time)
    'wet_sand': {'color': REPO / 'build-win64/bin/Assets/levelgen/_decals/wet_sand.png', 'alpha': 'BLEND',
                 'repeat': 8.0, 'roughness': 0.6},
    # the shallows: the rippled sand seen through the water near the beach (over the water, fading out to sea)
    'shallows': {'color': REPO / 'build-win64/bin/Assets/levelgen/_decals/shallows.png', 'alpha': 'BLEND',
                 'repeat': 6.0, 'roughness': 0.25},
    'white': {'factor': [0.92, 0.92, 0.9, 1.0], 'roughness': 0.5},
    'thatch': {'factor': [0.48, 0.37, 0.21, 1.0], 'roughness': 1.0, 'double': True},   # a palapa's palm roof
    'wood_post': {'factor': [0.42, 0.30, 0.18, 1.0], 'roughness': 0.9},
    'sail': {'factor': [0.85, 0.88, 0.9, 1.0], 'roughness': 0.4, 'double': True},
    'haze_near': {'factor': [0.30, 0.38, 0.33, 1.0], 'roughness': 1.0},
    'haze_far': {'factor': [0.52, 0.52, 0.60, 1.0], 'roughness': 1.0},
    # el agua: su color y unas ondas (un mapa de normales que se repite) que se deslizan (scroll: anchos de textura por
    # segundo, en <modelo>.materials.xml): los brillos del sol se mueven; el mar despacio y en diagonal, el río corre
    'water': {'factor': [0.06, 0.25, 0.40, 1.0], 'roughness': 0.35, 'repeat': 7.0, 'scroll': (0.018, 0.011), 'water': 0.45,
              'normal': REPO / 'build-win64/bin/Assets/levelgen/_decals/water_ripples_normal.png'},
    # the waves on the beach: foam crests across a band along the waterline, rolling in toward the sand (scroll in v)
    'shore_foam': {'color': REPO / 'build-win64/bin/Assets/levelgen/_decals/shore_foam.png', 'alpha': 'BLEND',
                   'repeat': 6.0, 'roughness': 0.6, 'scroll': (0.0, -0.11)},
    'river': {'factor': [0.18, 0.33, 0.30, 1.0], 'roughness': 0.38, 'repeat': 5.0, 'scroll': (0.0, 0.05), 'water': 0.35,
              'normal': REPO / 'build-win64/bin/Assets/levelgen/_decals/water_ripples_normal.png'},
    # el atlas de desgaste (decals.py): se hace la primera vez que hace falta
    'decals': {'color': REPO / 'build-win64/bin/Assets/levelgen/_decals/wear_atlas.png', 'alpha': 'BLEND', 'roughness': 0.9},
    # road paint worn by traffic (make_worn_paint), the crosswalks more than the lines: in Mexico little paint is left
    'paint_white': {'factor': [0.86, 0.86, 0.82, 1.0], 'roughness': 0.7, 'alpha': 'BLEND', 'repeat': 3.0,
                    'color': REPO / 'build-win64/bin/Assets/levelgen/_decals/worn_paint.png'},
    'paint_yellow': {'factor': [0.88, 0.70, 0.18, 1.0], 'roughness': 0.7, 'alpha': 'BLEND', 'repeat': 3.0,
                     'color': REPO / 'build-win64/bin/Assets/levelgen/_decals/worn_paint.png'},
    'paint_crossing': {'factor': [0.86, 0.86, 0.82, 1.0], 'roughness': 0.7, 'alpha': 'BLEND', 'repeat': 2.5,
                       'color': REPO / 'build-win64/bin/Assets/levelgen/_decals/worn_paint_heavy.png'},
    'cable': {'factor': [0.06, 0.06, 0.07, 1.0], 'roughness': 0.6},
    'plaster': {'factor': [0.86, 0.80, 0.70, 1.0], 'roughness': 0.9},   # las bardas: aplanado
    'sign_red': {'factor': [0.80, 0.08, 0.06, 1.0], 'roughness': 0.5},   # tableros de letreros
    'sign_yellow': {'factor': [0.98, 0.78, 0.10, 1.0], 'roughness': 0.5},
    'sign_white': {'factor': [0.92, 0.92, 0.90, 1.0], 'roughness': 0.5},
    'sign_green': {'factor': [0.10, 0.50, 0.25, 1.0], 'roughness': 0.5},
    # el papel picado: banderitas de colores vivos, de las dos caras
    'flag_pink': {'factor': [0.93, 0.20, 0.55, 1.0], 'roughness': 0.8, 'double': True},
    'flag_orange': {'factor': [0.98, 0.52, 0.10, 1.0], 'roughness': 0.8, 'double': True},
    'flag_yellow': {'factor': [0.98, 0.84, 0.15, 1.0], 'roughness': 0.8, 'double': True},
    'flag_green': {'factor': [0.20, 0.72, 0.30, 1.0], 'roughness': 0.8, 'double': True},
    'flag_blue': {'factor': [0.15, 0.50, 0.92, 1.0], 'roughness': 0.8, 'double': True},
    'flag_purple': {'factor': [0.55, 0.25, 0.80, 1.0], 'roughness': 0.8, 'double': True},
}
FLAGS = ['flag_pink', 'flag_orange', 'flag_yellow', 'flag_green', 'flag_blue', 'flag_purple']


class Mesh:
    """Lo que va en los cuadros: por cuadro y material, posiciones (x, altura, -y), normales, UV e índices."""

    def __init__(self, cell=CELL):
        self.cell = cell
        self.chunks = {}

    def _prim(self, x, y, material):
        key = (math.floor(x / self.cell), math.floor(y / self.cell))
        chunk = self.chunks.setdefault(key, {})
        return chunk.setdefault(material, {'pos': [], 'nrm': [], 'uv': [], 'idx': []})

    def flat(self, geometry, height, material, clip=None):
        """Un polígono plano a una altura, partido por cuadros."""
        repeat = MATERIALS[material].get('repeat', 4.0)
        for poly in polygons(geometry if clip is None else geometry.intersection(clip)):
            minx, miny, maxx, maxy = poly.bounds
            for cx in range(math.floor(minx / self.cell), math.floor(maxx / self.cell) + 1):
                for cy in range(math.floor(miny / self.cell), math.floor(maxy / self.cell) + 1):
                    cell_box = box(cx * self.cell, cy * self.cell, (cx + 1) * self.cell, (cy + 1) * self.cell)
                    part = poly.intersection(cell_box)
                    if part.is_empty or part.area < 1e-4:
                        continue
                    prim = self._prim(cx * self.cell + 1e-3, cy * self.cell + 1e-3, material)
                    for tri in triangulate(part):
                        base = len(prim['pos'])
                        for x, y in tri:
                            prim['pos'].append((x, height, -y))
                            prim['nrm'].append((0.0, 1.0, 0.0))
                            prim['uv'].append((x / repeat, -y / repeat))
                        # antihorario visto desde arriba en (x, y) sigue antihorario visto desde +y del glTF
                        prim['idx'] += [base, base + 1, base + 2]

    def heightfield(self, geometry, height_of, material, step=4.0, uv1_of=None):
        """Un terreno sobre el polígono: una grilla de 'step' metros con la altura de height_of(x, y). Con 'uv1_of', cada
        vértice lleva además uv1_of(x, y) como segundas coordenadas (el mar: qué tan bajo es)."""
        repeat = MATERIALS[material].get('repeat', 4.0)
        for poly in polygons(geometry):
            minx, miny, maxx, maxy = poly.bounds
            xs = np.arange(math.floor(minx / step) * step, maxx + step, step)
            ys = np.arange(math.floor(miny / step) * step, maxy + step, step)
            for x in xs:
                for y in ys:
                    quad = box(x, y, x + step, y + step)
                    if not poly.intersects(quad):
                        continue
                    part = poly.intersection(quad)
                    if part.area < 0.05:
                        continue
                    prim = self._prim(x + step / 2, y + step / 2, material)
                    for tri in triangulate(part):
                        pts = [(px, py, height_of(px, py)) for px, py in tri]
                        a, b, c = (np.array([p[0], p[2], -p[1]]) for p in pts)
                        n = np.cross(c - a, b - a)
                        n = n / max(np.linalg.norm(n), 1e-9)
                        if n[1] < 0:
                            n = -n
                        base = len(prim['pos'])
                        for px, py, h in pts:
                            prim['pos'].append((px, h, -py))
                            prim['nrm'].append(tuple(n))
                            prim['uv'].append((px / repeat, -py / repeat))
                            if uv1_of is not None:
                                prim.setdefault('uv1', []).append(uv1_of(px, py))
                        prim['idx'] += [base, base + 1, base + 2]

    def wall(self, a, b, bottom, top, material, facing=None):
        """Una pared vertical de a a b (puntos 2D), de 'bottom' a 'top'. Su normal mira a la derecha de a->b, o hacia
        'facing' (un punto) si se da."""
        (ax, ay), (bx, by) = a, b
        length = math.hypot(bx - ax, by - ay)
        if length < 1e-3:
            return
        nx, ny = (by - ay) / length, -(bx - ax) / length
        if facing is not None:
            mx, my = (ax + bx) / 2, (ay + by) / 2
            if (facing[0] - mx) * nx + (facing[1] - my) * ny < 0:
                (ax, ay), (bx, by) = (bx, by), (ax, ay)
                nx, ny = -nx, -ny
        repeat = MATERIALS[material].get('repeat', 4.0)
        prim = self._prim((ax + bx) / 2, (ay + by) / 2, material)
        base = len(prim['pos'])
        u0 = (ax * 0.7 + ay * 0.3) / repeat
        u1 = u0 + length / repeat
        for (x, y, h, u, v) in ((ax, ay, bottom, u0, bottom / repeat), (bx, by, bottom, u1, bottom / repeat),
                                (bx, by, top, u1, top / repeat), (ax, ay, top, u0, top / repeat)):
            prim['pos'].append((x, h, -y))
            prim['nrm'].append((nx, 0.0, -ny))
            prim['uv'].append((u, -v))
        prim['idx'] += [base, base + 1, base + 2, base, base + 2, base + 3]

    def ring_walls(self, geometry, bottom, top, material, outward=True):
        """Paredes a lo largo de todos los bordes de un polígono (afuera, o adentro con outward=False)."""
        for poly in polygons(geometry):
            for k, ring in enumerate([poly.exterior] + list(poly.interiors)):
                coords = list(ring.coords)
                ccw = ring.is_ccw
                # en un anillo antihorario lo que encierra queda a la izquierda de a->b: en el exterior, el polígono; en
                # un agujero, el agujero
                right_is_outside = ccw if k == 0 else not ccw
                for a, b in zip(coords, coords[1:]):
                    if right_is_outside == outward:
                        self.wall(a, b, bottom, top, material)
                    else:
                        self.wall(b, a, bottom, top, material)

    def box_along(self, line, width, bottom, top, material):
        """Una barda (pretil, barandal) de 'width' de grueso a lo largo de una línea."""
        for poly in polygons(line.buffer(width / 2, cap_style=2, join_style=2)):
            self.flat(poly, top, material)
            self.ring_walls(poly, bottom, top, material)

    def decal(self, center, size, angle, cell, height, stretch=1.0, mirror=False):
        """Una calcomanía plana (celda 'cell' del atlas de decals.py) de 'size' metros, girada 'angle' grados; 'stretch'
        la alarga a lo largo, 'mirror' la voltea (una misma celda se ve distinta)."""
        from decals import cell_uv
        u0, v0, u1, v1 = cell_uv(cell)
        if mirror:
            u0, u1 = u1, u0
        a = math.radians(angle)
        ca, sa = math.cos(a), math.sin(a)
        hx, hy = size / 2 * stretch, size / 2
        x, y = center
        corners = [(x + ca * dx - sa * dy, y + sa * dx + ca * dy) for dx, dy in ((-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy))]
        prim = self._prim(x, y, 'decals')
        base = len(prim['pos'])
        # dos calcomanías encimadas a la misma altura se pelean la profundidad (z-fighting): cada una un escalón más
        # arriba que la anterior (un ciclo de 8, a 3 mm)
        self._decals = getattr(self, '_decals', 0) + 1
        height += 0.003 * (self._decals % 8)
        for (px, py), uv in zip(corners, ((u0, v1), (u1, v1), (u1, v0), (u0, v0))):
            prim['pos'].append((px, height, -py))
            prim['nrm'].append((0.0, 1.0, 0.0))
            prim['uv'].append(uv)
        prim['idx'] += [base, base + 1, base + 2, base, base + 2, base + 3]

    def strip(self, side_a, side_b, height, material):
        """Una franja plana entre dos filas de puntos (x, y) a la par: v = 0 en 'side_a' y 1 en 'side_b'; u a lo largo,
        una repetición de la textura cada 'repeat' metros del material."""
        repeat = MATERIALS[material].get('repeat', 4.0)
        run = 0.0
        for k in range(len(side_a) - 1):
            a0, a1, b0, b1 = side_a[k], side_a[k + 1], side_b[k], side_b[k + 1]
            step = math.hypot(a1[0] - a0[0], a1[1] - a0[1])
            u0, u1 = run / repeat, (run + step) / repeat
            run += step
            prim = self._prim((a0[0] + b1[0]) / 2, (a0[1] + b1[1]) / 2, material)
            base = len(prim['pos'])
            for (x, y), uv in ((a0, (u0, 0.0)), (a1, (u1, 0.0)), (b1, (u1, 1.0)), (b0, (u0, 1.0))):
                prim['pos'].append((x, height, -y))
                prim['nrm'].append((0.0, 1.0, 0.0))
                prim['uv'].append(uv)
            # antihorario visto desde arriba (en x, y): si no, al revés
            cross = (a1[0] - a0[0]) * (b0[1] - a0[1]) - (a1[1] - a0[1]) * (b0[0] - a0[0])
            prim['idx'] += [base, base + 1, base + 2, base, base + 2, base + 3] if cross > 0 else                 [base, base + 2, base + 1, base, base + 3, base + 2]

    def ribbon(self, rows, heights, material):
        """A band through rows of points (x, y), each row as long as the others and at its own height: v = 0 on the first
        row, 1 on the last; u along, a repeat of the texture every 'repeat' meters of the material (along the first row)."""
        repeat = MATERIALS[material].get('repeat', 4.0)
        first = rows[0]
        runs = [0.0]
        for k in range(1, len(first)):
            runs.append(runs[-1] + math.hypot(first[k][0] - first[k - 1][0], first[k][1] - first[k - 1][1]))
        last = len(rows) - 1
        for r in range(last):
            a_row, b_row = rows[r], rows[r + 1]
            va, vb = r / last, (r + 1) / last
            for k in range(len(first) - 1):
                a0, a1, b0, b1 = a_row[k], a_row[k + 1], b_row[k], b_row[k + 1]
                u0, u1 = runs[k] / repeat, runs[k + 1] / repeat
                prim = self._prim((a0[0] + b1[0]) / 2, (a0[1] + b1[1]) / 2, material)
                base = len(prim['pos'])
                for (x, y), h, uv in ((a0, heights[r], (u0, va)), (a1, heights[r], (u1, va)), (b1, heights[r + 1], (u1, vb)),
                                      (b0, heights[r + 1], (u0, vb))):
                    prim['pos'].append((x, h, -y))
                    prim['nrm'].append((0.0, 1.0, 0.0))
                    prim['uv'].append(uv)
                cross = (a1[0] - a0[0]) * (b0[1] - a0[1]) - (a1[1] - a0[1]) * (b0[0] - a0[0])
                prim['idx'] += [base, base + 1, base + 2, base, base + 2, base + 3] if cross > 0 else                     [base, base + 2, base + 1, base, base + 3, base + 2]

    def facade_decal(self, a, b, bottom, top, cell, out=0.04):
        """Una calcomanía parada sobre una fachada: de a a b (su base, en el plano de la fachada), mirando a la derecha
        de a->b, despegada 'out' metros."""
        from decals import cell_uv
        u0, v0, u1, v1 = cell_uv(cell)
        (ax, ay), (bx, by) = a, b
        length = math.hypot(bx - ax, by - ay)
        if length < 1e-3:
            return
        nx, ny = (by - ay) / length, -(bx - ax) / length
        ax, ay, bx, by = ax + nx * out, ay + ny * out, bx + nx * out, by + ny * out
        prim = self._prim((ax + bx) / 2, (ay + by) / 2, 'decals')
        base = len(prim['pos'])
        # la parte de arriba de la celda (v0) arriba: los escurrimientos bajan
        for (x, y, h, u, v) in ((ax, ay, bottom, u0, v1), (bx, by, bottom, u1, v1), (bx, by, top, u1, v0), (ax, ay, top, u0, v0)):
            prim['pos'].append((x, h, -y))
            prim['nrm'].append((nx, 0.0, -ny))
            prim['uv'].append((u, v))
        prim['idx'] += [base, base + 1, base + 2, base, base + 2, base + 3]

    def beam(self, p0, p1, thickness, material):
        """Una viga de sección cuadrada entre dos puntos 3D (x, y, altura) del nivel."""
        a, b = np.array(p0, float), np.array(p1, float)
        d = b - a
        length = np.linalg.norm(d)
        if length < 1e-4:
            return
        d /= length
        up = np.array([0.0, 0.0, 1.0]) if abs(d[2]) < 0.9 else np.array([1.0, 0.0, 0.0])
        u = np.cross(d, up)
        u /= np.linalg.norm(u)
        v = np.cross(d, u)
        h = thickness / 2
        ring = [u * h + v * h, -u * h + v * h, -u * h - v * h, u * h - v * h]
        prim = self._prim(a[0], a[1], material)
        for k in range(4):
            c0, c1 = ring[k], ring[(k + 1) % 4]
            n = (c0 + c1) / np.linalg.norm(c0 + c1)
            base = len(prim['pos'])
            quad = (a + c0, a + c1, b + c1, b + c0)
            for q in quad:
                prim['pos'].append((q[0], q[2], -q[1]))
                prim['nrm'].append((n[0], n[2], -n[1]))
                prim['uv'].append((0.0, 0.0))
            # (x, y, altura) -> (x, altura, -y) es una rotación: el sentido de las caras se conserva
            if np.dot(np.cross(quad[1] - quad[0], quad[2] - quad[0]), n) > 0:
                prim['idx'] += [base, base + 1, base + 2, base, base + 2, base + 3]
            else:
                prim['idx'] += [base, base + 2, base + 1, base, base + 3, base + 2]

    def triangle(self, p0, p1, p2, material):
        """Un triángulo suelto (puntos 3D del nivel); con un material de dos caras se ve de ambos lados."""
        pts = [np.array(p, float) for p in (p0, p1, p2)]
        n = np.cross(pts[1] - pts[0], pts[2] - pts[0])
        n /= max(np.linalg.norm(n), 1e-9)
        prim = self._prim(pts[0][0], pts[0][1], material)
        base = len(prim['pos'])
        for q in pts:
            prim['pos'].append((q[0], q[2], -q[1]))
            prim['nrm'].append((n[0], n[2], -n[1]))
            prim['uv'].append((0.0, 0.0))
        prim['idx'] += [base, base + 1, base + 2]

    @staticmethod
    def sag_points(p0, p1, sag, segments):
        """Los puntos de un cable colgado entre dos puntos 3D: una parábola que baja 'sag' metros a la mitad."""
        a, b = np.array(p0, float), np.array(p1, float)
        out = []
        for k in range(segments + 1):
            u = k / segments
            q = a + (b - a) * u
            q[2] -= sag * 4.0 * u * (1.0 - u)
            out.append(q)
        return out

    def cable(self, p0, p1, sag=0.4, thickness=0.04, material='cable', segments=6):
        """Un cable de luz colgado entre dos puntos 3D (x, y, altura)."""
        pts = self.sag_points(p0, p1, sag, segments)
        for q0, q1 in zip(pts, pts[1:]):
            self.beam(q0, q1, thickness, material)

    def bunting(self, p0, p1, rng, sag=0.5, spacing=0.55, size=(0.36, 0.42), materials=None):
        """Papel picado: un cordón colgado entre dos puntos 3D con banderitas de colores colgando de él, una cada
        'spacing' metros (ancho, alto: 'size')."""
        materials = materials or FLAGS
        length = float(np.linalg.norm(np.array(p1, float)[:2] - np.array(p0, float)[:2]))
        segments = max(4, int(length / 1.5))
        pts = self.sag_points(p0, p1, sag, segments)
        for q0, q1 in zip(pts, pts[1:]):
            self.beam(q0, q1, 0.02, 'cable')
        count = int(length / spacing)
        a, b = np.array(p0, float), np.array(p1, float)
        d = (b - a)
        d[2] = 0.0
        d /= max(np.linalg.norm(d), 1e-9)
        half = size[0] / 2
        for k in range(1, count):
            u = k / count
            q = a + (b - a) * u
            q[2] -= sag * 4.0 * u * (1.0 - u)
            top_l, top_r = q - d * half, q + d * half
            drop = np.array([0.0, 0.0, -size[1] * rng.uniform(0.9, 1.1)])
            # un poco ladeada, como si el viento la moviera
            sway = np.cross(d, [0.0, 0.0, 1.0]) * rng.uniform(-0.08, 0.08)
            material = materials[k % len(materials)]
            bl, br = top_l + drop + sway, top_r + drop + sway
            self.triangle(top_l, top_r, br, material)
            self.triangle(top_l, br, bl, material)

    def write(self, out_dir, prefix):
        """Escribe un .gltf + .bin por cuadro en out_dir; copia las texturas a out_dir/_textures. Devuelve los nombres."""
        out_dir = Path(out_dir)
        out_dir.mkdir(parents=True, exist_ok=True)
        tex_dir = out_dir / '_textures'
        written = []
        for old in out_dir.glob(f'{prefix}_*.gltf'):
            old.unlink()
        for old in out_dir.glob(f'{prefix}_*.bin'):
            old.unlink()
        for (cx, cy), materials in sorted(self.chunks.items()):
            name = f'{prefix}_{cx}_{cy}'.replace('-', 'm')
            gltf, data = to_gltf(name, materials, tex_dir)
            if gltf is None:
                continue
            (out_dir / f'{name}.bin').write_bytes(data)
            gltf['buffers'] = [{'byteLength': len(data), 'uri': f'{name}.bin'}]
            (out_dir / f'{name}.gltf').write_text(json.dumps(gltf), encoding='utf-8')
            # the materials that flow (water): their speed for the engine, next to the model
            # (and the ones that blend: the engine blends what its sidecar says, whatever the glTF's alpha mode)
            flowing = [m for m in sorted(materials) if materials[m]['idx'] and (MATERIALS[m].get('scroll') or MATERIALS[m].get('detile')
                                                                                or MATERIALS[m].get('alpha') == 'BLEND')]
            overrides = out_dir / f'{name}.gltf.materials.xml'
            if flowing:
                def row(m):
                    spec = MATERIALS[m]
                    out = f'  <material name="{m}" blend="{"alpha" if spec.get("alpha") == "BLEND" else "opaque"}"'
                    if spec.get('scroll'):
                        out += f' scroll="{spec["scroll"][0]} {spec["scroll"][1]}"'
                    if spec.get('water'):
                        out += f' water="{spec["water"]}"'
                    if spec.get('detile'):
                        out += f' detile="{spec["detile"][0]} {spec["detile"][1]}"'
                    return out + '/>\n'
                rows = ''.join(row(m) for m in flowing)
                overrides.write_text(f'<materials>\n{rows}</materials>\n', encoding='utf-8')
            elif overrides.exists():
                overrides.unlink()
            written.append(name)
        return written

    def triangles(self):
        return sum(len(p['idx']) // 3 for chunk in self.chunks.values() for p in chunk.values())


def make_shore_foam(path, size=256):
    """The beach's waves: two foam crests per tile (white, broken by noise) across v, wavy along u; the texture repeats
    in both (it scrolls in v: the crests roll in). Transparent between them."""
    from PIL import Image
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    u, v = np.meshgrid(np.linspace(0, 1, size, endpoint=False), np.linspace(0, 1, size, endpoint=False))
    rng = np.random.default_rng(5)
    wave = 0.05 * np.sin(2 * np.pi * (3 * u + 0.2)) + 0.03 * np.sin(2 * np.pi * (7 * u + 0.6))
    phase = (2 * (v + wave)) % 1.0
    # a crest: a sharp front and a long thinning tail behind it
    crest = np.where(phase < 0.08, phase / 0.08, np.clip(1.0 - (phase - 0.08) / 0.35, 0.0, 1.0) ** 2)
    noise = np.zeros_like(u)
    for _ in range(6):
        kx, ky = rng.integers(4, 20), rng.integers(2, 12)
        noise += np.sin(2 * np.pi * (kx * u + ky * v) + rng.uniform(0, 2 * np.pi))
    broken = np.clip(0.55 + noise * 0.12, 0.0, 1.0)
    alpha = np.clip(crest * broken * 1.3, 0.0, 0.9)
    rgb = np.ones((size, size, 3)) * np.array([0.95, 0.97, 0.98])
    Image.fromarray(np.dstack([rgb * 255.0, alpha * 255.0]).astype(np.uint8), 'RGBA').save(path)


def make_wet_sand(path, size=256, foam_v=0.111):
    """The wet band of the beach (v = 0 under the water, 1 on the dry sand): the foam line at 'foam_v' (where the water
    meets the sand: white, broken), then the wet sand, dark, fading toward the dry sand with an edge that wavers along u.
    It repeats along u."""
    from PIL import Image
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    u, v = np.meshgrid(np.linspace(0, 1, size, endpoint=False), np.linspace(0, 1, size))
    rng = np.random.default_rng(11)
    noise = np.zeros_like(u)
    for _ in range(6):
        kx, ky = rng.integers(3, 18), rng.integers(1, 9)
        noise += np.sin(2 * np.pi * (kx * u + ky * v) + rng.uniform(0, 2 * np.pi))
    noise /= 6.0
    # the dry sand's edge waves along the shore only (a noise across it drew spikes): two slow swells
    edge = 0.58 + 0.05 * np.sin(2 * np.pi * (u + 0.3)) + 0.03 * np.sin(2 * np.pi * (3 * u + 0.8))
    wet = np.clip((edge - v) / 0.35, 0.0, 1.0)
    wet = wet * wet * (3.0 - 2.0 * wet)
    line = foam_v + 0.012 * np.sin(2 * np.pi * (3 * u)) + 0.008 * noise
    foam = np.clip(1.0 - np.abs(v - line) / 0.022, 0.0, 1.0) * np.clip(0.75 + 0.6 * noise, 0.0, 1.0)
    dark = np.array([0.42, 0.35, 0.25]) * (0.92 + 0.08 * noise[..., None])
    white = np.array([0.96, 0.97, 0.97])
    rgb = dark * (1.0 - foam[..., None]) + white * foam[..., None]
    alpha = np.clip(np.maximum(0.55 * wet, foam * 0.95), 0.0, 1.0)
    Image.fromarray(np.dstack([rgb * 255.0, alpha * 255.0]).astype(np.uint8), 'RGBA').save(path)


def make_shallows(path, size=256):
    """The sand under shallow water (v = 0 at the waterline, 1 out at sea): ridges along u (the ripples the waves leave,
    wavy and broken), tinted by the water, fading out toward the deep water. It repeats along u."""
    from PIL import Image
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    u, v = np.meshgrid(np.linspace(0, 1, size, endpoint=False), np.linspace(0, 1, size))
    rng = np.random.default_rng(17)
    noise = np.zeros_like(u)
    for _ in range(6):
        kx, ky = rng.integers(2, 12), rng.integers(1, 6)
        noise += np.sin(2 * np.pi * (kx * u + ky * v) + rng.uniform(0, 2 * np.pi))
    noise /= 6.0
    ridges = 0.5 + 0.5 * np.sin(2 * np.pi * (9 * v + 0.35 * np.sin(2 * np.pi * (2 * u)) + 0.25 * noise))
    shade = 0.82 + 0.22 * ridges ** 2
    sand = np.array([0.66, 0.58, 0.42])
    water = np.array([0.20, 0.42, 0.46])
    depth = np.clip(v, 0.0, 1.0)[..., None]
    rgb = (sand * shade[..., None]) * (1.0 - 0.55 * depth) + water * 0.55 * depth
    alpha = np.clip(0.85 * (1.0 - v) ** 1.4, 0.0, 1.0)
    Image.fromarray(np.dstack([np.clip(rgb, 0, 1) * 255.0, alpha * 255.0]).astype(np.uint8), 'RGBA').save(path)


def make_malecon_paving(path, size=512, slabs=4):
    """Pale concrete slabs, 'slabs' by 'slabs' a repeat: each its own tone, a fine grain, thin dark joints."""
    from PIL import Image
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(23)
    u, v = np.meshgrid(np.arange(size), np.arange(size))
    cell = size // slabs
    tone = rng.uniform(0.94, 1.04, (slabs, slabs))[v // cell, u // cell]
    grain = rng.normal(0.0, 0.025, (size, size))
    joint = np.minimum(np.minimum(u % cell, cell - 1 - u % cell), np.minimum(v % cell, cell - 1 - v % cell))
    seam = np.clip(joint / 3.0, 0.0, 1.0)
    base = np.array([0.70, 0.67, 0.62])
    shade = (tone + grain) * (0.62 + 0.38 * seam)
    Image.fromarray((np.clip(base * shade[..., None], 0, 1) * 255.0).astype(np.uint8), 'RGB').save(path)


def make_plaza_tiles(path, size=512, tiles=4):
    """A well-off plaza's tiles (as a shopping plaza's): pale square tiles, 'tiles' by 'tiles' a repeat, each a
    little different, a cut corner at each joint (the small square between four tiles darker), thin grout."""
    from PIL import Image
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(31)
    u, v = np.meshgrid(np.arange(size), np.arange(size))
    cell = size // tiles
    tone = rng.uniform(0.95, 1.03, (tiles, tiles))[v // cell, u // cell]
    du, dv = u % cell, v % cell
    edge = np.minimum(np.minimum(du, cell - 1 - du), np.minimum(dv, cell - 1 - dv))
    grout = np.clip(edge / 2.0, 0.0, 1.0)
    corner = np.minimum(du, cell - 1 - du) + np.minimum(dv, cell - 1 - dv) < cell * 0.16
    base = np.array([0.72, 0.71, 0.68])
    shade = (tone + rng.normal(0.0, 0.012, (size, size))) * (0.72 + 0.28 * grout) * np.where(corner, 0.78, 1.0)
    Image.fromarray((np.clip(base * shade[..., None], 0, 1) * 255.0).astype(np.uint8), 'RGB').save(path)


def make_worn_paint(path, size=512, heavy=False):
    """Road paint worn by traffic (it repeats without a seam): white with grime, its alpha eaten by noise at several
    scales and by small chips. 'heavy': a crosswalk as it usually is in Mexico, only patches of paint left."""
    from PIL import Image
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(29 if heavy else 23)

    def tile_noise(cells):
        small = rng.random((cells, cells))
        small = np.pad(small, 2, mode='wrap')
        step = size // cells
        big = Image.fromarray((small * 255).astype(np.uint8)).resize(((cells + 4) * step,) * 2, Image.BICUBIC)
        return np.asarray(big, np.float32)[2 * step:2 * step + size, 2 * step:2 * step + size] / 255.0

    wear = 0.5 * tile_noise(4) + 0.3 * tile_noise(16) + 0.2 * tile_noise(64)
    chips = tile_noise(128)
    cut = 0.55 if heavy else 0.36   # how much of the noise eats the paint
    alpha = np.clip((wear - cut) / 0.12 + 0.6, 0.0, 1.0)
    alpha *= np.clip((chips - 0.25) / 0.1, 0.0, 1.0) if not heavy else np.clip((chips - 0.35) / 0.1, 0.0, 1.0)
    alpha *= 0.55 + 0.4 * tile_noise(32) if heavy else 0.8 + 0.2 * tile_noise(32)
    grime = 0.78 + 0.22 * tile_noise(32)
    rgb = np.stack([grime * 255.0] * 3, -1)
    Image.fromarray(np.dstack([rgb, np.clip(alpha, 0, 1) * 255.0]).astype(np.uint8), 'RGBA').save(path)


def make_water_normal(path, size=256):
    """Las ondas del agua: un mapa de normales que se repite sin costura (senos de periodos enteros), suaves."""
    from PIL import Image
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    u, v = np.meshgrid(np.linspace(0, 1, size, endpoint=False), np.linspace(0, 1, size, endpoint=False))
    h = np.zeros_like(u)
    rng = np.random.default_rng(3)
    # many short waves, gentle: a shimmer, not blotches (the low sun makes every slope a glint)
    for _ in range(14):
        kx, ky = rng.integers(-14, 15), rng.integers(4, 15)
        h += np.sin(2 * np.pi * (kx * u + ky * v) + rng.uniform(0, 2 * np.pi)) / np.hypot(kx, ky)
    gy, gx = np.gradient(h)
    strength = 2.2
    n = np.dstack([-gx * strength, -gy * strength, np.ones_like(h)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    Image.fromarray(((n * 0.5 + 0.5) * 255).astype(np.uint8), 'RGB').save(path)


def to_gltf(name, materials, tex_dir):
    data = bytearray()
    views, accessors, meshes_prims, mats, images, textures = [], [], [], [], [], []
    image_index = {}

    def add_view(raw, target=None):
        while len(data) % 4:
            data.append(0)
        view = {'buffer': 0, 'byteOffset': len(data), 'byteLength': len(raw)}
        if target:
            view['target'] = target
        data.extend(raw)
        views.append(view)
        return len(views) - 1

    def image(path):
        if path not in image_index:
            if not path.is_file() and path.name in ('wear_atlas.png', 'beach_sand.png'):
                # nuestras texturas (decals.py), hechas la primera vez
                import decals
                path.parent.mkdir(parents=True, exist_ok=True)
                (decals.atlas() if path.name == 'wear_atlas.png' else decals.sand_texture()).save(path)
            if not path.is_file() and path.name == 'water_ripples_normal.png':
                make_water_normal(path)
            if not path.is_file() and path.name == 'shore_foam.png':
                make_shore_foam(path)
            if not path.is_file() and path.name == 'wet_sand.png':
                make_wet_sand(path)
            if not path.is_file() and path.name == 'plaza_tiles.png':
                make_plaza_tiles(path)
            if not path.is_file() and path.name == 'malecon_paving.png':
                make_malecon_paving(path)
            if not path.is_file() and path.name == 'shallows.png':
                make_shallows(path)
            if not path.is_file() and path.name in ('worn_paint.png', 'worn_paint_heavy.png'):
                make_worn_paint(path, heavy=path.name == 'worn_paint_heavy.png')
            tex_dir.mkdir(parents=True, exist_ok=True)
            target = tex_dir / path.name
            if not target.is_file():
                shutil.copy2(path, target)
            images.append({'uri': f'_textures/{path.name}'})
            textures.append({'source': len(images) - 1, 'sampler': 0})
            image_index[path] = len(textures) - 1
        return image_index[path]

    for material, prim in sorted(materials.items()):
        if not prim['idx']:
            continue
        spec = MATERIALS[material]
        pos = np.array(prim['pos'], dtype=np.float32)
        nrm = np.array(prim['nrm'], dtype=np.float32)
        uv = np.array(prim['uv'], dtype=np.float32)
        idx = np.array(prim['idx'], dtype=np.uint32)
        p = add_view(pos.tobytes(), 34962)
        accessors.append({'bufferView': p, 'componentType': 5126, 'count': len(pos), 'type': 'VEC3',
                          'min': pos.min(axis=0).tolist(), 'max': pos.max(axis=0).tolist()})
        n = add_view(nrm.tobytes(), 34962)
        accessors.append({'bufferView': n, 'componentType': 5126, 'count': len(nrm), 'type': 'VEC3'})
        t = add_view(uv.tobytes(), 34962)
        accessors.append({'bufferView': t, 'componentType': 5126, 'count': len(uv), 'type': 'VEC2'})
        i = add_view(idx.tobytes(), 34963)
        accessors.append({'bufferView': i, 'componentType': 5125, 'count': len(idx), 'type': 'SCALAR'})
        # a second set of coordinates when the primitive brings one (the sea: how shallow each vertex is)
        uv1_accessor = None
        if len(prim.get('uv1', [])) == len(pos):
            u1 = add_view(np.array(prim['uv1'], dtype=np.float32).tobytes(), 34962)
            accessors.append({'bufferView': u1, 'componentType': 5126, 'count': len(pos), 'type': 'VEC2'})
            uv1_accessor = len(accessors) - 1
        m = {'name': material, 'pbrMetallicRoughness': {'metallicFactor': 0.0, 'roughnessFactor': spec.get('roughness', 1.0)}}
        if 'color' in spec:
            m['pbrMetallicRoughness']['baseColorTexture'] = {'index': image(spec['color'])}
        if 'factor' in spec:
            m['pbrMetallicRoughness']['baseColorFactor'] = spec['factor']
        if 'normal' in spec:
            m['normalTexture'] = {'index': image(spec['normal'])}
        if 'orm' in spec:
            m['pbrMetallicRoughness']['metallicRoughnessTexture'] = {'index': image(spec['orm'])}
        if spec.get('alpha'):
            m['alphaMode'] = spec['alpha']
        if spec.get('double'):
            m['doubleSided'] = True
        mats.append(m)
        base = len(accessors) - (5 if uv1_accessor is not None else 4)
        attributes = {'POSITION': base, 'NORMAL': base + 1, 'TEXCOORD_0': base + 2}
        if uv1_accessor is not None:
            attributes['TEXCOORD_1'] = uv1_accessor
        meshes_prims.append({'attributes': attributes, 'indices': base + 3, 'material': len(mats) - 1})
    if not meshes_prims:
        return None, None
    gltf = {
        'asset': {'version': '2.0', 'generator': 'Pakal levelbuilder roadmesh.py'},
        'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0, 'name': name}],
        'meshes': [{'name': name, 'primitives': meshes_prims}],
        'materials': mats, 'bufferViews': views, 'accessors': accessors,
        'samplers': [{'magFilter': 9729, 'minFilter': 9987, 'wrapS': 10497, 'wrapT': 10497}],
    }
    if images:
        gltf['images'] = images
        gltf['textures'] = textures
    return gltf, bytes(data)


# --- las calles ---

def junctions(net):
    """Los puntos donde dos ejes con asfalto se cruzan o se tocan: (punto, ejes que pasan por ahí)."""
    asphalt = [e for e in net.edges if e['kind'] != 'alley']
    found = []
    for i, a in enumerate(asphalt):
        for b in asphalt[i + 1:]:
            hit = a['line'].intersection(b['line'].buffer(0.3))
            if hit.is_empty:
                continue
            for part in lines(hit) or []:
                c = part.interpolate(0.5, normalized=True)
                found.append((c, a, b))
            if hit.geom_type == 'Point':
                found.append((hit, a, b))
    return found


def dashes(line, dash, gap, start_skip=0.0):
    """Tramos de 'dash' metros cada dash+gap a lo largo de la línea."""
    out, d = [], start_skip
    while d + dash <= line.length:
        a, b = line.interpolate(d), line.interpolate(d + dash)
        out.append(LineString([a, b]))
        d += dash + gap
    return out


def road_surfaces(net, mesh, clip):
    """Asfalto, guarniciones, banquetas, empedrado, líneas, cebras y pretiles de la red, dentro de 'clip'."""
    a = net.areas
    road = a['road'].intersection(clip)
    mesh.flat(road, ROAD_Y, 'asphalt')
    # guarniciones: el borde del asfalto sube a la banqueta (salvo donde da al agua: ahí no hay borde)
    mesh.ring_walls(road, ROAD_Y - 0.02, 0.0, 'concrete', outward=False)
    mesh.flat(a['sidewalk'].intersection(clip), 0.0, 'concrete')
    paving = unary_union([a['promenade'], a['alley']] + [b.get('ground_poly', b['poly']) for b in net.blocks if b['kind'] == 'plaza'])
    malecon = getattr(net, 'malecon', None)
    if malecon:
        # a seawall promenade: concrete slabs along the sea, the steps down to it (terrain.malecon_surfaces), cobblestone
        # for the rest
        steps = unary_union([ring for ring, _ in malecon['steps']])
        walk = a['malecon'].difference(steps)
        mesh.flat(paving.difference(walk).difference(steps).intersection(clip), 0.0, 'cobble')
        # the esplanade's top in its own floor (the brief's malecon "esplanade_floor": the promenade's slabs by default)
        top = walk.intersection(malecon['esplanade'])
        mesh.flat(walk.difference(top).intersection(clip), 0.0, 'malecon')
        mesh.flat(top.intersection(clip), 0.0, malecon.get('esplanade_floor', 'malecon'))
    else:
        mesh.flat(paving.intersection(clip), 0.0, 'cobble')

    # líneas: fuera de los cruces (lejos de la calle que cruza) y de los puentes no
    crossings = unary_union([p.buffer(max(ea['width'], eb['width']) / 2 + 3.0) for p, ea, eb in junctions(net)]) \
        if net.edges else None
    for e in net.edges:
        if e['kind'] == 'alley':
            continue
        line = e['line'].intersection(clip)
        if crossings is not None:
            line = line.difference(crossings)
        for piece in lines(line):
            if piece.length < 3.0:
                continue
            if e['kind'] == 'avenue':
                for side in (-0.12, 0.12):
                    shifted = piece.offset_curve(side)
                    for part in lines(shifted):
                        mesh.flat(part.buffer(0.06, cap_style=2), ROAD_Y + 0.008, 'paint_yellow')
            else:
                for d in dashes(piece, 2.0, 3.0, 1.0):
                    mesh.flat(d.buffer(0.07, cap_style=2), ROAD_Y + 0.008, 'paint_white')
    # cebras: en cada calle, a los lados de cada cruce
    for point, ea, eb in junctions(net):
        for e, other in ((ea, eb), (eb, ea)):
            line = e['line']
            at = line.project(point)
            for sign in (-1, 1):
                d = at + sign * (other['width'] / 2 + 2.6)
                if d < 1.0 or d > line.length - 1.0:
                    continue
                c = line.interpolate(d)
                c2 = line.interpolate(min(max(d + 0.5, 0), line.length))
                c1 = line.interpolate(min(max(d - 0.5, 0), line.length))
                tx, ty = c2.x - c1.x, c2.y - c1.y
                norm = math.hypot(tx, ty) or 1.0
                tx, ty = tx / norm, ty / norm
                nx, ny = -ty, tx
                half = e['width'] / 2 - 0.6
                k = -half
                while k + 0.45 <= half:
                    m = k + 0.225
                    stripe = LineString([(c.x + nx * m - tx * 1.4, c.y + ny * m - ty * 1.4),
                                         (c.x + nx * m + tx * 1.4, c.y + ny * m + ty * 1.4)]).buffer(0.225, cap_style=2)
                    if clip.contains(stripe.centroid):
                        mesh.flat(stripe, ROAD_Y + 0.009, 'paint_crossing')
                    k += 0.9
    # pretiles de los puentes
    for e in net.edges:
        if not e.get('bridge'):
            continue
        for side in (-1, 1):
            rail = e['line'].offset_curve(side * (e['width'] / 2 + net.brief['sidewalk'] + 0.2))
            for part in lines(rail.intersection(clip)):
                mesh.box_along(part, 0.35, ROAD_Y, 0.9, 'concrete')
    return paving
