"""The 2D collision of a model as a top-down game needs it: what the model has at the height of a body, seen from above.
A gazebo is its four posts (its roof is overhead), a bench its legs and seat, a table its legs and top, an awning
nothing.

    outline(path)            -> [polygon in the model's (x, z)], or None when nothing is known (use the footprint box)
    shapes_at(path, x, y, yaw, scale) -> the same polygons placed in the level (meters, x east, y north)

Where it comes from, in order:
1. <model>.colliders.json next to the model (what an extractor wrote: e.g. a Unity prefab's colliders, in the glTF's
   axes): its boxes, capsules and spheres that cross the band; a "mesh" entry adds the model's own slice (2).
   Colliders that exist but none crossing the band: no collision at all.
2. The model's own triangles cut to the band (clipped at its bottom and top), projected and rasterized at 'cell' meters,
   and made polygons again (outer rings only).
The results are cached by the model's content under Assets/levelgen/_outlines.
"""
import base64
import hashlib
import json
import math
from pathlib import Path
from urllib.parse import unquote

import numpy as np

import citykit

BAND = (0.15, 1.7)    # meters above the floor: what a body walking runs into
CELL = 0.05
CACHE = citykit.ASSETS_ROOT / 'levelgen' / '_outlines'
VERSION = 1


def _buffers(doc, base):
    data = []
    for b in doc.get('buffers', []):
        uri = b.get('uri', '')
        if uri.startswith('data:'):
            data.append(base64.b64decode(uri.split(',', 1)[1]))
        else:
            data.append((base / unquote(uri)).read_bytes())
    return data


def _accessor(doc, buffers, index):
    a = doc['accessors'][index]
    view = doc['bufferViews'][a['bufferView']]
    kinds = {5126: np.float32, 5125: np.uint32, 5123: np.uint16, 5121: np.uint8}
    width = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
    dtype = np.dtype(kinds[a['componentType']])
    stride = view.get('byteStride') or dtype.itemsize * width
    start = view.get('byteOffset', 0) + a.get('byteOffset', 0)
    raw = buffers[view['buffer']]
    if stride == dtype.itemsize * width:
        return np.frombuffer(raw, dtype, a['count'] * width, start).reshape(a['count'], width)
    rows = [np.frombuffer(raw, dtype, width, start + i * stride) for i in range(a['count'])]
    return np.array(rows)


def triangles(path):
    """The model's triangles (N x 3 x 3), with its node transforms applied (meters, Y up)."""
    path = Path(path)
    doc = json.loads(path.read_text(encoding='utf-8'))
    buffers = _buffers(doc, path.parent)
    out = []

    def visit(index, parent):
        node = doc['nodes'][index]
        world = parent @ citykit._local_matrix(node)
        if 'mesh' in node:
            for prim in doc['meshes'][node['mesh']]['primitives']:
                if prim.get('mode', 4) != 4:
                    continue
                pos = _accessor(doc, buffers, prim['attributes']['POSITION']).astype(np.float64)
                pos = (np.c_[pos, np.ones(len(pos))] @ world.T)[:, :3]
                idx = _accessor(doc, buffers, prim['indices']).reshape(-1) if 'indices' in prim else np.arange(len(pos))
                out.append(pos[idx.reshape(-1, 3)])
        for child in node.get('children', []):
            visit(child, world)

    for root in doc['scenes'][doc.get('scene', 0)]['nodes']:
        visit(root, np.eye(4))
    return np.concatenate(out) if out else np.zeros((0, 3, 3))


def _clip(poly, y, keep_above):
    """A polygon (list of 3D points) clipped against the plane Y = y."""
    out = []
    for i, p in enumerate(poly):
        q = poly[(i + 1) % len(poly)]
        pin = p[1] >= y if keep_above else p[1] <= y
        qin = q[1] >= y if keep_above else q[1] <= y
        if pin:
            out.append(p)
        if pin != qin:
            t = (y - p[1]) / (q[1] - p[1])
            out.append(p + (q - p) * t)
    return out


def slice_polygons(tris, band=BAND, cell=CELL):
    """The parts of the triangles inside the band, seen from above: polygons in (x, z)."""
    from PIL import Image, ImageDraw
    from shapely.geometry import Polygon, box
    from shapely.ops import unary_union
    lo, hi = band
    pieces = []
    for tri in tris:
        ys = tri[:, 1]
        if ys.max() < lo or ys.min() > hi:
            continue
        poly = _clip(_clip(list(tri), lo, True), hi, False)
        if len(poly) >= 3:
            pieces.append([(p[0], p[2]) for p in poly])
    if not pieces:
        return []
    xs = [x for p in pieces for x, _ in p]
    zs = [z for p in pieces for _, z in p]
    x0, z0 = min(xs) - cell, min(zs) - cell
    w = int(math.ceil((max(xs) - x0) / cell)) + 2
    h = int(math.ceil((max(zs) - z0) / cell)) + 2
    if w * h > 4_000_000:
        return None   # too big to rasterize here (a building): the caller keeps its box
    image = Image.new('1', (w, h), 0)
    draw = ImageDraw.Draw(image)
    for p in pieces:
        pts = [((x - x0) / cell, (z - z0) / cell) for x, z in p]
        draw.polygon(pts, fill=1, outline=1)   # a vertical face projects to a line: the outline keeps it
    grid = np.array(image, dtype=bool)
    rects = []
    for j in range(h):
        row = grid[j]
        i = 0
        while i < w:
            if row[i]:
                k = i
                while k < w and row[k]:
                    k += 1
                rects.append(box(x0 + i * cell, z0 + j * cell, x0 + k * cell, z0 + (j + 1) * cell))
                i = k
            else:
                i += 1
    merged = unary_union(rects).buffer(cell * 0.5).buffer(-cell * 0.25)
    polys = [merged] if merged.geom_type == 'Polygon' else list(getattr(merged, 'geoms', []))
    out = []
    for p in polys:
        outer = Polygon(p.exterior).simplify(cell, preserve_topology=True)
        if outer.area >= cell * cell * 4:
            out.append([(round(x, 3), round(z, 3)) for x, z in list(outer.exterior.coords)[:-1]])
    return out


def _quat_rotate(q, v):
    x, y, z, w = q
    ux, uy, uz = v
    # v + 2 w (q x v) + 2 q x (q x v)
    cx, cy, cz = y * uz - z * uy, z * ux - x * uz, x * uy - y * ux
    cx2, cy2, cz2 = y * cz - z * cy, z * cx - x * cz, x * cy - y * cx
    return (ux + 2 * (w * cx + cx2), uy + 2 * (w * cy + cy2), uz + 2 * (w * cz + cz2))


def _collider_polygons(shapes, band):
    """The prefab colliders that cross the band, seen from above. Returns (polygons, has_mesh)."""
    from shapely.geometry import MultiPoint, Point
    lo, hi = band
    polys, mesh = [], False
    for s in shapes:
        kind = s['type']
        if kind == 'mesh':
            mesh = True
            continue
        c = s.get('center', [0, 0, 0])
        if kind == 'box':
            hx, hy, hz = (v / 2 for v in s['size'])
            q = s.get('rot', [0, 0, 0, 1])
            corners = [_quat_rotate(q, (sx * hx, sy * hy, sz * hz)) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
            corners = [(c[0] + a, c[1] + b, c[2] + d) for a, b, d in corners]
            ys = [p[1] for p in corners]
            if max(ys) < lo or min(ys) > hi:
                continue
            hull = MultiPoint([(p[0], p[2]) for p in corners]).convex_hull
        elif kind in ('capsule', 'sphere'):
            r = s['radius']
            half = max(0.0, s.get('height', 2 * r) / 2 - r) if kind == 'capsule' else 0.0
            axis = s.get('axis', 1)
            d = [0.0, 0.0, 0.0]
            d[axis] = half
            q = s.get('rot', [0, 0, 0, 1])
            a = _quat_rotate(q, d)
            p0 = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
            p1 = (c[0] + a[0], c[1] + a[1], c[2] + a[2])
            if max(p0[1], p1[1]) + r < lo or min(p0[1], p1[1]) - r > hi:
                continue
            hull = MultiPoint([(p0[0], p0[2]), (p1[0], p1[2])]).buffer(r, quad_segs=3) if half > 0 else Point(c[0], c[2]).buffer(r, quad_segs=3)
        else:
            continue
        if hull.area > 1e-4:
            polys.append([(round(x, 3), round(z, 3)) for x, z in list(hull.exterior.coords)[:-1]])
    return polys, mesh


def outline(path, band=BAND, cell=CELL):
    """The model's collision at body height, in its own (x, z): a list of polygons ([] = it collides with nothing), or
    None when nothing can be said (the caller keeps the footprint box)."""
    path = Path(path)
    sidecar = path.with_name(path.stem + '.colliders.json')
    key = hashlib.md5((path.read_bytes() + (sidecar.read_bytes() if sidecar.is_file() else b'')
                       + json.dumps([band, cell, VERSION]).encode())).hexdigest()[:16]
    cached = CACHE / f'{path.stem}_{key}.json'
    if cached.is_file():
        return json.loads(cached.read_text(encoding='utf-8'))['polygons']
    polys = None
    if sidecar.is_file():
        shapes = json.loads(sidecar.read_text(encoding='utf-8')).get('shapes', [])
        if shapes:
            polys, mesh = _collider_polygons(shapes, band)
            if mesh:
                sliced = slice_polygons(triangles(path), band, cell)
                if sliced is None:
                    polys = None
                else:
                    polys += sliced
    if polys is None and not (sidecar.is_file() and json.loads(sidecar.read_text(encoding='utf-8')).get('shapes')):
        polys = slice_polygons(triangles(path), band, cell)
    CACHE.mkdir(parents=True, exist_ok=True)
    cached.write_text(json.dumps({'model': str(path), 'polygons': polys}), encoding='utf-8')
    return polys


def place(polygons, x, y, yaw_degrees, scale=1.0):
    """Model-space polygons (x, z) placed in the level: model X east, model Z level -Y, turned by the yaw (as
    citykit.footprint)."""
    a = math.radians(yaw_degrees)
    c, s = math.cos(a), math.sin(a)
    out = []
    for poly in polygons:
        placed = []
        for mx, mz in poly:
            lx, ly = mx * scale, -mz * scale
            placed.append((x + lx * c - ly * s, y + lx * s + ly * c))
        out.append(placed)
    return out


def shapes_at(path, x, y, yaw_degrees, scale=1.0, band=BAND):
    polys = outline(path, band)
    return None if polys is None else place(polys, x, y, yaw_degrees, scale)
