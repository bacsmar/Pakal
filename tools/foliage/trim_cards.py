"""Trims a model's alpha-masked cards to their leaves: each masked triangle becomes the small convex polygon around the
texels its texture shows (the outline of the leaves, simplified, cut by the triangle and triangulated), so the GPU
shades and discards far fewer empty pixels.

On a phone's tile GPU a masked card pays its shader on every pixel it covers, the transparent ones too, and a discard
also stops the GPU from hiding what is behind it early. Measured on a mid-range phone in a dense marsh (High): the
masked grass cards cost 12 ms of a 31 ms frame, two thirds of it their area.

    python trim_cards.py <model.gltf> [...] [--suffix _trim] [--threshold 0.15] [--pad 8] [--tolerance 6]

Writes <model><suffix>.gltf and .bin beside each model (the same folder, so its textures resolve). Licensed packs stay
out of the repository: the output goes where the pack is (the build's Assets), never into Git. Solid primitives are
copied as they are. Every attribute is interpolated at the new corners (normals renormalized); all are written as
floats.

- threshold: the alpha (0..1) a texel counts as leaf at. Low, with 'pad', because the cutout shader lifts alpha with
  the mip level (pakal_cutout.sh): far away a leaf grows into its blurred edge, and the polygon must still hold it.
- pad: texels of margin around the leaves (at the texture's full size).
- tolerance: how far (in 1/256ths of the texture) the outline may stray outward to save corners. On a licensed
  marsh grass tuft: 4 keeps 45% of the card's area in 64 triangles, 6 keeps 48% in 48, 10 keeps 54% in 34 (the untrimmed
  card: 100% in 2); no leaf texel is lost at any of them.
"""
import argparse
import copy
import json
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter
import shapely
import shapely.affinity
from shapely.geometry import Polygon, box
from shapely.ops import unary_union

COMPONENT = {5120: ('b', 1), 5121: ('B', 1), 5122: ('h', 2), 5123: ('H', 2), 5125: ('I', 4), 5126: ('f', 4)}
WIDTH = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}
NORMALIZE = {5120: 127.0, 5121: 255.0, 5122: 32767.0, 5123: 65535.0}


class Gltf:
    def __init__(self, path):
        self.path = Path(path)
        self.doc = json.loads(self.path.read_text())
        self.buffers = [(self.path.parent / b['uri']).read_bytes() for b in self.doc['buffers']]

    def read(self, index):
        """An accessor as a float array (count, width), normalized integers scaled to 0..1."""
        a = self.doc['accessors'][index]
        view = self.doc['bufferViews'][a['bufferView']]
        code, size = COMPONENT[a['componentType']]
        width = WIDTH[a['type']]
        stride = view.get('byteStride', size * width)
        start = view.get('byteOffset', 0) + a.get('byteOffset', 0)
        data = self.buffers[view['buffer']]
        out = np.array([struct.unpack_from(f'<{width}{code}', data, start + k * stride) for k in range(a['count'])],
                       dtype=np.float64).reshape(a['count'], width)
        if a.get('normalized') and a['componentType'] in NORMALIZE:
            out /= NORMALIZE[a['componentType']]
        return out


GRID = 256  # the outline is traced on the texture scaled to GRID x GRID


def leaf_outline(texture, threshold, pad, tolerance):
    """The leaves' outline in uv (0..1): the texels with alpha at or over 'threshold', grown by 'pad' texels, traced on
    a GRID x GRID copy, simplified by 'tolerance' grid cells and grown back by as much (so no leaf is cut)."""
    alpha = Image.open(texture).convert('RGBA').getchannel('A')
    mask = alpha.point(lambda v: 255 if v >= threshold * 255 else 0)
    if pad > 0:
        mask = mask.filter(ImageFilter.MaxFilter(2 * pad + 1))
    grid = np.array(mask.resize((GRID, GRID), Image.BOX)) > 0
    runs = []
    for r in range(GRID):
        edges = np.diff(np.r_[0, grid[r].astype(int), 0])
        runs += [box(a, r, b, r + 1) for a, b in zip(np.nonzero(edges == 1)[0], np.nonzero(edges == -1)[0])]
    shape = unary_union(runs)
    shape = shape.simplify(tolerance).buffer(tolerance * 0.7, join_style=2).simplify(tolerance * 0.3)
    shape = shape.intersection(box(0, 0, GRID, GRID))
    return shapely.affinity.scale(shape, 1.0 / GRID, 1.0 / GRID, origin=(0, 0))


def trim_triangle(uv, outline):
    """The triangles (each three uv corners, the source's winding) that cover the leaf inside one triangle: [] when it
    shows nothing, the triangle itself when it leaves the texture (wrapping uv)."""
    uv = np.array(uv)
    if uv.min() < -1e-4 or uv.max() > 1.0 + 1e-4:
        return [list(uv)]
    triangle = Polygon(uv)
    piece = outline.intersection(triangle)
    if piece.is_empty or piece.area <= 0.0:
        return []
    if piece.area > 0.9 * triangle.area:
        return [list(uv)]
    sign = triangle_sign(uv) > 0
    out = []
    for part in getattr(piece, 'geoms', [piece]):
        if part.geom_type != 'Polygon' or part.area <= 0.0:
            continue
        for t in shapely.constrained_delaunay_triangles(part).geoms:
            corners = [np.array(c) for c in list(t.exterior.coords)[:3]]
            if (triangle_sign(corners) > 0) != sign:
                corners = [corners[0], corners[2], corners[1]]
            out.append(corners)
    return out


def triangle_sign(p):
    return (p[1][0] - p[0][0]) * (p[2][1] - p[0][1]) - (p[1][1] - p[0][1]) * (p[2][0] - p[0][0])


def barycentric(uv, p):
    a, b, c = (np.array(x) for x in uv)
    m = np.array([[b[0] - a[0], c[0] - a[0]], [b[1] - a[1], c[1] - a[1]]])
    v, w = np.linalg.solve(m, np.array(p) - a)
    return np.array([1.0 - v - w, v, w])


def trim(path, suffix, threshold, pad, tolerance):
    g = Gltf(path)
    doc = copy.deepcopy(g.doc)
    blob = bytearray()
    views, accessors = [], []

    def put(array, kind, target, with_bounds=False):
        nonlocal blob
        while len(blob) % 4:
            blob += b'\0'
        if kind == 'SCALAR':
            data = np.asarray(array, dtype='<u4').tobytes()
            component = 5125
        else:
            data = np.asarray(array, dtype='<f4').tobytes()
            component = 5126
        views.append({'buffer': 0, 'byteOffset': len(blob), 'byteLength': len(data), 'target': target})
        blob += data
        acc = {'bufferView': len(views) - 1, 'componentType': component, 'count': len(array), 'type': kind}
        if with_bounds:
            acc['min'] = np.asarray(array).min(axis=0).tolist()
            acc['max'] = np.asarray(array).max(axis=0).tolist()
        accessors.append(acc)
        return len(accessors) - 1

    masks = {}
    report = []
    for mesh in doc['meshes']:
        for prim in mesh['primitives']:
            attrs = {name: g.read(i) for name, i in prim['attributes'].items()}
            count = len(attrs['POSITION'])
            index = g.read(prim['indices']).astype(int).ravel() if 'indices' in prim else np.arange(count)
            tris = index.reshape(-1, 3)
            material = doc['materials'][prim['material']] if 'material' in prim else {}
            texture = None
            if material.get('alphaMode') == 'MASK' and 'TEXCOORD_0' in attrs:
                t = material.get('pbrMetallicRoughness', {}).get('baseColorTexture', {}).get('index')
                if t is not None:
                    texture = g.path.parent / doc['images'][doc['textures'][t]['source']]['uri']
            if texture is None:
                out = {name: a for name, a in attrs.items()}
                out_index = index
            else:
                if texture not in masks:
                    masks[texture] = leaf_outline(texture, threshold, pad, tolerance)
                outline = masks[texture]
                out = {name: [] for name in attrs}
                out_index = []
                before = after = 0.0
                for tri in tris:
                    uv = [attrs['TEXCOORD_0'][k] for k in tri]
                    before += abs(triangle_sign(uv)) * 0.5
                    for corners in trim_triangle(uv, outline):
                        after += abs(triangle_sign(corners)) * 0.5
                        first = len(out['POSITION'])
                        for p in corners:
                            weights = barycentric(uv, p)
                            for name, a in attrs.items():
                                value = weights @ a[tri]
                                if name == 'NORMAL':
                                    value = value / max(np.linalg.norm(value), 1e-9)
                                out[name].append(value)
                        out_index += [first, first + 1, first + 2]
                report.append(f'{len(tris)} triangles, area {100 * after / max(before, 1e-12):.0f}% kept, '
                              f'{len(out_index) // 3} triangles after')
                out = {name: np.array(v) for name, v in out.items()}
            new_attrs = {}
            for name, a in out.items():
                kind = {2: 'VEC2', 3: 'VEC3', 4: 'VEC4'}[a.shape[1]]
                new_attrs[name] = put(a, kind, 34962, with_bounds=(name == 'POSITION'))
            prim['attributes'] = new_attrs
            prim['indices'] = put(np.asarray(out_index, dtype=np.uint32), 'SCALAR', 34963)
    out_path = g.path.with_name(g.path.stem + suffix + '.gltf')
    doc['buffers'] = [{'byteLength': len(blob), 'uri': out_path.stem + '.bin'}]
    doc['bufferViews'] = views
    doc['accessors'] = accessors
    out_path.with_suffix('.bin').write_bytes(bytes(blob))
    out_path.write_text(json.dumps(doc, indent=1))
    print(f'{g.path.name} -> {out_path.name}: ' + ('; '.join(report) or 'no masked cards'))
    return out_path


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('models', nargs='+')
    parser.add_argument('--suffix', default='_trim')
    parser.add_argument('--threshold', type=float, default=0.15)
    parser.add_argument('--pad', type=int, default=8)
    parser.add_argument('--tolerance', type=float, default=6.0)
    args = parser.parse_args(argv)
    for model in args.models:
        trim(model, args.suffix, args.threshold, args.pad, args.tolerance)


if __name__ == '__main__':
    sys.exit(main())
