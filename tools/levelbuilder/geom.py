"""Geometría para el generador por curvas (shapely 2): curvas suaves, ruido de una dimensión, desplazar una curva por su
normal, triangular polígonos y partirlos en convexos para la colisión. Todo en metros, ejes del nivel (x este, y norte).

    pip install shapely numpy pillow
"""
import math
import random

import numpy as np
import shapely
from shapely.geometry import LineString, MultiPolygon, Point, Polygon, box
from shapely.ops import unary_union


def polygons(geometry):
    """Los polígonos de una geometría (Polygon, MultiPolygon o colección), sin vacíos."""
    if geometry is None or geometry.is_empty:
        return []
    if isinstance(geometry, Polygon):
        return [geometry]
    if isinstance(geometry, MultiPolygon):
        return [g for g in geometry.geoms if not g.is_empty]
    return [g for part in getattr(geometry, 'geoms', []) for g in polygons(part)]


def lines(geometry):
    """Las líneas de una geometría (LineString, MultiLineString o colección)."""
    if geometry is None or geometry.is_empty:
        return []
    if isinstance(geometry, LineString):
        return [geometry]
    return [g for part in getattr(geometry, 'geoms', []) for g in lines(part)]


class Noise1D:
    """Ruido suave de una variable: valores al azar cada 'scale' metros unidos por una curva coseno; varias octavas."""

    def __init__(self, seed, scale, octaves=2):
        self.rng = random.Random(seed)
        self.scale = scale
        self.octaves = octaves
        self.tables = [[self.rng.uniform(-1.0, 1.0) for _ in range(4096)] for _ in range(octaves)]

    def __call__(self, x):
        total, amplitude, frequency, norm = 0.0, 1.0, 1.0 / self.scale, 0.0
        for table in self.tables:
            t = x * frequency + 1000.0
            i = math.floor(t)
            f = t - i
            f = (1 - math.cos(f * math.pi)) * 0.5
            a, b = table[i % 4096], table[(i + 1) % 4096]
            total += (a + (b - a) * f) * amplitude
            norm += amplitude
            amplitude *= 0.5
            frequency *= 2.0
        return total / norm


def fbm2d(seed, width, height, cell, scale, octaves=4):
    """Ruido fBm sobre una grilla (alto x ancho celdas de 'cell' metros), valores 0..1."""
    rng = np.random.default_rng(seed)
    out = np.zeros((height, width), dtype=np.float32)
    amplitude, norm = 1.0, 0.0
    period = max(scale / cell, 1.0)
    for _ in range(octaves):
        gw, gh = int(width / period) + 3, int(height / period) + 3
        grid = rng.random((gh, gw)).astype(np.float32)
        ys = np.arange(height) / period
        xs = np.arange(width) / period
        y0 = np.floor(ys).astype(int)
        x0 = np.floor(xs).astype(int)
        fy = (1 - np.cos((ys - y0) * np.pi)) * 0.5
        fx = (1 - np.cos((xs - x0) * np.pi)) * 0.5
        a = grid[y0][:, x0]
        b = grid[y0][:, x0 + 1]
        c = grid[y0 + 1][:, x0]
        d = grid[y0 + 1][:, x0 + 1]
        top = a + (b - a) * fx[None, :]
        bottom = c + (d - c) * fx[None, :]
        out += (top + (bottom - top) * fy[:, None]) * amplitude
        norm += amplitude
        amplitude *= 0.5
        period = max(period / 2.0, 1.0)
    return out / norm


def catmull_rom(points, step=2.0):
    """Una curva suave por los puntos (Catmull-Rom centrípeta), muestreada cada ~'step' metros."""
    pts = [np.array(p, dtype=float) for p in points]
    if len(pts) < 2:
        return [tuple(p) for p in pts]
    pts = [pts[0] * 2 - pts[1]] + pts + [pts[-1] * 2 - pts[-2]]
    out = []
    for i in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[i + 1], pts[i + 2]
        n = max(int(np.linalg.norm(p2 - p1) / step), 1)
        for k in range(n):
            t = k / n
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3)))
    out.append(tuple(pts[-2]))
    return out


class Curve:
    """Una polilínea con longitud de arco: punto, tangente y normal derecha en cualquier s."""

    def __init__(self, points):
        self.points = np.array(points, dtype=float)
        seg = np.diff(self.points, axis=0)
        self.lengths = np.hypot(seg[:, 0], seg[:, 1])
        self.s = np.concatenate([[0.0], np.cumsum(self.lengths)])
        self.length = float(self.s[-1])
        self.line = LineString(self.points)

    def at(self, s):
        s = min(max(s, 0.0), self.length)
        i = int(np.searchsorted(self.s, s, side='right') - 1)
        i = min(max(i, 0), len(self.lengths) - 1)
        f = (s - self.s[i]) / self.lengths[i] if self.lengths[i] > 0 else 0.0
        return self.points[i] + (self.points[i + 1] - self.points[i]) * f

    def tangent(self, s, window=6.0):
        a, b = self.at(s - window), self.at(s + window)
        d = b - a
        return d / max(np.hypot(*d), 1e-9)

    def normal(self, s):
        """La normal a la derecha de la dirección de avance."""
        t = self.tangent(s)
        return np.array([t[1], -t[0]])

    def offset(self, distance, step=3.0):
        """La curva desplazada por su normal derecha; 'distance' es un número o una función de s."""
        far = distance if callable(distance) else (lambda s: distance)
        count = max(int(self.length / step), 2)
        return [tuple(self.at(s) + self.normal(s) * far(s)) for s in np.linspace(0.0, self.length, count + 1)]

    def project(self, point):
        return self.line.project(Point(point))


def ray(origin, direction, length):
    o = np.array(origin, dtype=float)
    d = np.array(direction, dtype=float)
    return LineString([tuple(o), tuple(o + d * length)])


def rotate(v, degrees):
    a = math.radians(degrees)
    c, s = math.cos(a), math.sin(a)
    return np.array([v[0] * c - v[1] * s, v[0] * s + v[1] * c])


def triangulate(polygon):
    """Triángulos (tuplas de 3 puntos 2D, en sentido antihorario) de un polígono con agujeros, respetando sus bordes."""
    out = []
    for poly in polygons(polygon):
        poly = poly.buffer(0)
        for p in polygons(poly):
            tris = shapely.constrained_delaunay_triangles(p)
            for t in getattr(tris, 'geoms', []):
                coords = list(t.exterior.coords)[:3]
                (x0, y0), (x1, y1), (x2, y2) = coords
                area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
                if abs(area) < 1e-6:
                    continue
                out.append(coords if area > 0 else [coords[0], coords[2], coords[1]])
    return out


def convex_pieces(polygon, max_corners=8):
    """Piezas convexas de hasta 'max_corners' esquinas que cubren el polígono (para la colisión llena del juego):
    los triángulos juntados de a poco mientras la unión siga convexa."""
    pieces = []
    for tri in triangulate(polygon):
        merged = False
        t = Polygon(tri)
        for i, piece in enumerate(pieces):
            if not piece.touches(t) and not piece.intersects(t):
                continue
            union = piece.union(t)
            if not isinstance(union, Polygon):
                continue
            hull = union.convex_hull
            if abs(hull.area - union.area) < 1e-4 and len(hull.exterior.coords) - 1 <= max_corners:
                pieces[i] = hull
                merged = True
                break
        if not merged:
            pieces.append(t)
    return [list(p.exterior.coords)[:-1] for p in pieces]


def oriented_box(width, depth, center, front, setback=0.0):
    """Rectángulo de 'width' a lo largo y 'depth' de fondo, con su frente en 'center' mirando hacia 'front'."""
    f = np.array(front, dtype=float)
    f /= max(np.hypot(*f), 1e-9)
    t = np.array([-f[1], f[0]])
    c = np.array(center, dtype=float) - f * setback
    a = c + t * width / 2
    b = c - t * width / 2
    return Polygon([tuple(a), tuple(b), tuple(b - f * depth), tuple(a - f * depth)])


__all__ = ['polygons', 'lines', 'Noise1D', 'fbm2d', 'catmull_rom', 'Curve', 'ray', 'rotate', 'triangulate', 'convex_pieces',
           'oriented_box', 'LineString', 'Polygon', 'MultiPolygon', 'Point', 'box', 'unary_union']
