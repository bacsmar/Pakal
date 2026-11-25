"""A tree's shadow proxy: three alpha-tested quads with its silhouette, drawn only in the sun's shadow pass while the
tree itself casts nothing (docs/SHADOW_PROXIES.md; the engine's MeshComponent3D_Bgfx::set_shadow_only and
set_casts_shadow, which a level loader turns on with the rules of topdown/ShadowProxies.h).

A dense tree can be ~30k leaf cards; in the shadow pass each one costs as much as on screen. The proxy is six triangles:
one horizontal quad at the crown's height (its shadow under a high sun) and two crossed vertical quads through the
trunk (a low sun's long shadow). Each shows the tree seen from that side, its leaves' alpha kept, in one small atlas.
With the soft 1024 shadow map the proxy's shadow reads as the crown's.

    python shadow_proxy.py <tree.gltf> [...] [--suffix _shadow] [--size 256]

Writes <tree><suffix>.glb beside each tree (the pack's folder, never Git: licensed art stays out of the repository).
"""
import argparse
import io
import json
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from trim_cards import Gltf  # noqa: E402


def quat_matrix(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def node_matrix(node):
    m = np.eye(4)
    if 'matrix' in node:
        return np.array(node['matrix'], dtype=float).reshape(4, 4).T
    r = quat_matrix(node.get('rotation', [0, 0, 0, 1]))
    s = np.diag(node.get('scale', [1, 1, 1]))
    m[:3, :3] = r @ s
    m[:3, 3] = node.get('translation', [0, 0, 0])
    return m


def triangles(g):
    """Every triangle of the model in its world space (meters, Y up): (positions (n,3,3), uvs (n,3,2) or None, alpha
    texture or None, cutoff) per primitive."""
    doc = g.doc
    out = []

    def walk(index, parent):
        node = doc['nodes'][index]
        world = parent @ node_matrix(node)
        if 'mesh' in node:
            for prim in doc['meshes'][node['mesh']]['primitives']:
                pos = g.read(prim['attributes']['POSITION'])
                pos = (world @ np.c_[pos, np.ones(len(pos))].T).T[:, :3]
                idx = g.read(prim['indices']).astype(int).ravel() if 'indices' in prim else np.arange(len(pos))
                tri = idx.reshape(-1, 3)
                mat = doc['materials'][prim['material']] if 'material' in prim else {}
                alpha, uvs, cutoff = None, None, mat.get('alphaCutoff', 0.5)
                if mat.get('alphaMode') == 'MASK' and 'TEXCOORD_0' in prim['attributes']:
                    t = mat.get('pbrMetallicRoughness', {}).get('baseColorTexture', {}).get('index')
                    if t is not None:
                        uri = doc['images'][doc['textures'][t]['source']]['uri']
                        alpha = np.array(Image.open(g.path.parent / uri).convert('RGBA').getchannel('A'), dtype=np.float32) / 255.0
                        uvs = g.read(prim['attributes']['TEXCOORD_0'])[tri]
                out.append((pos[tri], uvs, alpha, cutoff))
        for child in node.get('children', []):
            walk(child, world)

    for root in doc['scenes'][doc.get('scene', 0)]['nodes']:
        walk(root, np.eye(4))
    return out


def silhouette(parts, axes, lo, hi, size):
    """The model seen along the axis left out of 'axes' (two of 0, 1, 2), over the box lo..hi of those axes: an alpha
    image, row 0 at the box's high end of the second axis (up for a side view)."""
    img = np.zeros((size, size), dtype=bool)
    span = np.maximum(np.array(hi) - np.array(lo), 1e-6)
    for tris, uvs, alpha, cutoff in parts:
        p = (tris[:, :, axes] - lo) / span * (size - 1)
        p[:, :, 1] = (size - 1) - p[:, :, 1]
        for k in range(len(p)):
            a, b, c = p[k]
            x0, y0 = np.floor(np.minimum(np.minimum(a, b), c)).astype(int)
            x1, y1 = np.ceil(np.maximum(np.maximum(a, b), c)).astype(int)
            x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, size - 1), min(y1, size - 1)
            if x1 < x0 or y1 < y0:
                continue
            xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
            if abs(d) < 1e-9:
                continue
            w0 = ((b[1] - c[1]) * (xs - c[0]) + (c[0] - b[0]) * (ys - c[1])) / d
            w1 = ((c[1] - a[1]) * (xs - c[0]) + (a[0] - c[0]) * (ys - c[1])) / d
            w2 = 1.0 - w0 - w1
            inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            if alpha is not None and inside.any():
                uv = w0[..., None] * uvs[k][0] + w1[..., None] * uvs[k][1] + w2[..., None] * uvs[k][2]
                h, w = alpha.shape
                tx = np.clip((uv[..., 0] % 1.0) * w, 0, w - 1).astype(int)
                ty = np.clip((uv[..., 1] % 1.0) * h, 0, h - 1).astype(int)
                inside &= alpha[ty, tx] >= cutoff
            img[y0:y1 + 1, x0:x1 + 1] |= inside
    return img


def glb(positions, uvs, indices, png):
    """A binary glTF: one mesh, one double-sided MASK material with the embedded PNG."""
    blob = bytearray()

    def view(data, target=None):
        while len(blob) % 4:
            blob.append(0)
        v = {'buffer': 0, 'byteOffset': len(blob), 'byteLength': len(data)}
        if target:
            v['target'] = target
        blob.extend(data)
        return v

    pos = np.asarray(positions, dtype='<f4')
    views = [view(pos.tobytes(), 34962), view(np.asarray(uvs, dtype='<f4').tobytes(), 34962),
             view(np.asarray(indices, dtype='<u4').tobytes(), 34963), view(png)]
    normals = np.tile(np.array([0, 1, 0], dtype='<f4'), (len(pos), 1))
    views.append(view(normals.tobytes(), 34962))
    doc = {
        'asset': {'version': '2.0', 'generator': 'Pakal tools/foliage/shadow_proxy.py'},
        'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0, 'name': 'shadow_proxy'}],
        'meshes': [{'primitives': [{'attributes': {'POSITION': 0, 'TEXCOORD_0': 1, 'NORMAL': 3}, 'indices': 2, 'material': 0}]}],
        'materials': [{'name': 'shadow_proxy', 'alphaMode': 'MASK', 'alphaCutoff': 0.5, 'doubleSided': True,
                       'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}, 'metallicFactor': 0, 'roughnessFactor': 1}}],
        'textures': [{'source': 0, 'sampler': 0}], 'samplers': [{'magFilter': 9728, 'minFilter': 9728}],
        'images': [{'bufferView': 3, 'mimeType': 'image/png'}],
        'accessors': [
            {'bufferView': 0, 'componentType': 5126, 'count': len(pos), 'type': 'VEC3', 'min': pos.min(0).tolist(), 'max': pos.max(0).tolist()},
            {'bufferView': 1, 'componentType': 5126, 'count': len(pos), 'type': 'VEC2'},
            {'bufferView': 2, 'componentType': 5125, 'count': len(indices), 'type': 'SCALAR'},
            {'bufferView': 4, 'componentType': 5126, 'count': len(pos), 'type': 'VEC3'}],
        'bufferViews': views, 'buffers': [{'byteLength': 0}],
    }
    while len(blob) % 4:
        blob.append(0)
    doc['buffers'][0]['byteLength'] = len(blob)
    text = json.dumps(doc).encode()
    text += b' ' * ((4 - len(text) % 4) % 4)
    total = 12 + 8 + len(text) + 8 + len(blob)
    return (struct.pack('<III', 0x46546C67, 2, total) + struct.pack('<II', len(text), 0x4E4F534A) + text
            + struct.pack('<II', len(blob), 0x004E4942) + bytes(blob))


def make_proxy(path, suffix='_shadow', size=256):
    g = Gltf(path)
    parts = triangles(g)
    allp = np.concatenate([p[0].reshape(-1, 3) for p in parts])
    lo, hi = allp.min(0), allp.max(0)
    center = 0.5 * (lo + hi)
    # the crown's height: the middle of the upper half of the tree, where the horizontal quad lies
    crown = lo[1] + 0.75 * (hi[1] - lo[1])
    top = silhouette(parts, [0, 2], [lo[0], lo[2]], [hi[0], hi[2]], size)       # seen from above: x, z
    front = silhouette(parts, [0, 1], [lo[0], lo[1]], [hi[0], hi[1]], size)     # seen along z: x, y
    side = silhouette(parts, [2, 1], [lo[2], lo[1]], [hi[2], hi[1]], size)      # seen along x: z, y
    # (the top view's rows run toward -z from the box's high z: its row 0 is z = hi; flipped to match the quad below)
    atlas = np.zeros((size, size * 3), dtype=np.uint8)
    for k, img in enumerate((top, front, side)):
        atlas[:, k * size:(k + 1) * size] = img * 255
    rgba = Image.merge('RGBA', [Image.new('L', (size * 3, size), 255)] * 3 + [Image.fromarray(atlas)])
    png = io.BytesIO()
    rgba.save(png, format='PNG')
    t = 1.0 / 3.0
    P, U, I = [], [], []

    def quad(corners, u0):
        # corners: (low-a, low-b), (high-a, low-b), (high-a, high-b), (low-a, high-b) in the image's axes; v runs down
        base = len(P)
        P.extend(corners)
        U.extend([(u0, 1.0), (u0 + t, 1.0), (u0 + t, 0.0), (u0, 0.0)])
        I.extend([base, base + 1, base + 2, base, base + 2, base + 3])

    quad([(lo[0], crown, lo[2]), (hi[0], crown, lo[2]), (hi[0], crown, hi[2]), (lo[0], crown, hi[2])], 0.0)
    quad([(lo[0], lo[1], center[2]), (hi[0], lo[1], center[2]), (hi[0], hi[1], center[2]), (lo[0], hi[1], center[2])], t)
    quad([(center[0], lo[1], lo[2]), (center[0], lo[1], hi[2]), (center[0], hi[1], hi[2]), (center[0], hi[1], lo[2])], 2 * t)
    out = g.path.with_name(g.path.stem + suffix + '.glb')
    out.write_bytes(glb(P, U, I, png.getvalue()))
    leaves = int(sum(len(p[0]) for p in parts))
    print(f'{g.path.name} -> {out.name}: {leaves} triangles -> 6; height {hi[1] - lo[1]:.1f} m, crown quad at {crown:.1f} m; '
          f'cover top {top.mean():.0%}, front {front.mean():.0%}, side {side.mean():.0%}')
    return out


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('models', nargs='+')
    parser.add_argument('--suffix', default='_shadow')
    parser.add_argument('--size', type=int, default=256)
    args = parser.parse_args(argv)
    for model in args.models:
        make_proxy(model, args.suffix, args.size)


if __name__ == '__main__':
    sys.exit(main())
