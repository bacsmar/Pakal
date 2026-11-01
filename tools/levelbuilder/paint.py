"""Buildings in colors of their own: a paint mask per pack atlas, laid over it, that marks which texels are walls (red)
and which are roof (green). The engine paints them per building (MeshComponent3D_Bgfx::set_paint, the level object's
"paint"): one model and one atlas for every color.

The mask comes from the pack's own buildings, not from guessing on the atlas: every face of every building model that
samples the atlas is drawn into it by its UVs, as a wall when it stands up (its normal level with the ground) and as a
roof when it faces the sky in the upper part of the model. Of the wall texels only the light, muted ones are kept (the
plaster and the siding; not the signs, doors or windows drawn on the same faces); the roof texels keep what the roof
faces sample.

Packs whose buildings take their colors from the atlas's swatches (flat squares of one color: PolygonCity) have walls
the light, muted test does not find, and the same swatches color doors and frames. There each model's wall is the swatch
most of its standing area samples (and its roof the one most of its roof samples), when that is over 40% of it; its
patch of one color is marked, and the blue channel keeps that color's brightness, so the shader paints it flat (the
paint at its own brightness) instead of shading it against plaster's.

    paint.mask(atlas_png, [building.gltf, ...]) -> <atlas>_paint.png (made once)
    paint.atlas_of(model_gltf, ['PolygonTown']) -> the atlas file the model samples, of those packs, or None
"""
import json
import math
import re
import struct
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ATLAS = re.compile(r'^(?P<pack>Polygon[A-Za-z]+)_(?:Texture_)?0\d_[A-Z]\.png$')
SIZE = 512   # the mask's side (its texels are walls or roofs: it needs less detail than the atlas)
COMPONENTS = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}
TYPES = {5126: ('f', 4), 5125: ('I', 4), 5123: ('H', 2), 5121: ('B', 1)}


def atlas_of(model, packs):
    """The pack atlas the model samples (an absolute path), of the packs named, or None."""
    model = Path(model)
    doc = json.loads(model.read_text(encoding='utf-8'))
    for image in doc.get('images', []):
        match = ATLAS.match(Path(image.get('uri', '')).name)
        if match and match.group('pack') in packs:
            path = (model.parent / image['uri']).resolve()
            if path.is_file():
                return path
    return None


def _accessor(doc, blob, index):
    accessor = doc['accessors'][index]
    view = doc['bufferViews'][accessor['bufferView']]
    fmt, size = TYPES[accessor['componentType']]
    width = COMPONENTS[accessor['type']]
    start = view.get('byteOffset', 0) + accessor.get('byteOffset', 0)
    stride = view.get('byteStride', size * width)
    count = accessor['count']
    if stride == size * width:
        values = np.frombuffer(blob, dtype=np.dtype(fmt).newbyteorder('<'), count=count * width, offset=start)
    else:
        values = np.array([struct.unpack_from('<' + fmt * width, blob, start + i * stride) for i in range(count)])
    return values.reshape(count, width).astype(np.float64)


def _node_matrix(node):
    if 'matrix' in node:
        return np.array(node['matrix'], np.float64).reshape(4, 4).T
    t = np.array(node.get('translation', [0, 0, 0]), np.float64)
    x, y, z, w = node.get('rotation', [0, 0, 0, 1])
    s = np.array(node.get('scale', [1, 1, 1]), np.float64)
    r = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    m = np.eye(4)
    m[:3, :3] = r * s
    m[:3, 3] = t
    return m


def _faces(model, atlas):
    """The model's triangles that sample 'atlas': world positions (N x 3 x 3) and UVs (N x 3 x 2)."""
    doc = json.loads(model.read_text(encoding='utf-8'))
    blob = (model.parent / doc['buffers'][0]['uri']).read_bytes()
    wanted = set()
    for i, material in enumerate(doc.get('materials', [])):
        t = material.get('pbrMetallicRoughness', {}).get('baseColorTexture')
        if t is not None:
            uri = doc['images'][doc['textures'][t['index']]['source']]['uri']
            if (model.parent / uri).resolve() == atlas:
                wanted.add(i)
    positions, uvs = [], []

    def visit(index, parent):
        node = doc['nodes'][index]
        m = parent @ _node_matrix(node)
        if 'mesh' in node:
            for p in doc['meshes'][node['mesh']]['primitives']:
                if p.get('material') not in wanted or 'TEXCOORD_0' not in p['attributes']:
                    continue
                pos = _accessor(doc, blob, p['attributes']['POSITION'])
                uv = _accessor(doc, blob, p['attributes']['TEXCOORD_0'])
                idx = _accessor(doc, blob, p['indices']).astype(np.int64).reshape(-1, 3) if 'indices' in p \
                    else np.arange(len(pos)).reshape(-1, 3)
                world = (np.c_[pos, np.ones(len(pos))] @ m.T)[:, :3]
                positions.append(world[idx])
                uvs.append(uv[idx])
        for child in node.get('children', []):
            visit(child, m)

    scene = doc['scenes'][doc.get('scene', 0)]
    for root in scene['nodes']:
        visit(root, np.eye(4))
    if not positions:
        return np.zeros((0, 3, 3)), np.zeros((0, 3, 2))
    return np.concatenate(positions), np.concatenate(uvs)


def mask(atlas, models):
    """The atlas's paint mask from these building models: <atlas>_paint.png, or None when none samples it. Made again when
    a model is newer than it (a building extracted later: its walls and roofs were not in it, left in the atlas's color)."""
    atlas = Path(atlas).resolve()
    out = atlas.with_name(f'{atlas.stem}_paint.png')
    if out.is_file() and all(Path(m).stat().st_mtime <= out.stat().st_mtime for m in models):
        return out
    walls = Image.new('L', (SIZE, SIZE), 0)
    roofs = Image.new('L', (SIZE, SIZE), 0)
    draw_wall, draw_roof = ImageDraw.Draw(walls), ImageDraw.Draw(roofs)
    used = 0
    for model in models:
        tris, uvs = _faces(Path(model), atlas)
        if not len(tris):
            continue
        used += 1
        heights = tris[:, :, 1]
        low, high = heights.min(), heights.max()
        normals = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
        lengths = np.linalg.norm(normals, axis=1)
        for tri, uv, normal, length in zip(tris, uvs, normals, lengths):
            if length < 1e-9:
                continue
            up = normal[1] / length
            points = [(u * SIZE, v * SIZE) for u, v in uv]
            if abs(up) < 0.3:
                draw_wall.polygon(points, fill=255)
            elif up > 0.45 and tri[:, 1].mean() > low + 0.55 * (high - low):
                draw_roof.polygon(points, fill=255)
    if not used:
        return None
    swatch_walls, swatch_roofs, reference = _swatches(atlas, models)
    # the walls: only the light, muted texels of those faces (plaster, siding), not the signs or doors on them
    rgb = np.asarray(Image.open(atlas).convert('RGB').resize((SIZE, SIZE), Image.BILINEAR), np.float32) / 255.0
    light = rgb.mean(axis=2)
    muted = rgb.max(axis=2) - rgb.min(axis=2)
    wall = (np.asarray(walls, np.float32) / 255.0) * (light > 0.42) * (muted < 0.22)
    roof = np.asarray(roofs, np.float32) / 255.0
    wall = wall * (1.0 - roof)   # a texel both a roof and a wall: the roof
    channels = [np.asarray(Image.fromarray((np.clip(c, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3)))
                for c in (wall, roof)]
    # the swatches as they are: neither the light test nor the grown edge (the next swatch is another color)
    roof_channel = np.maximum(channels[1], swatch_roofs * 255) * (swatch_walls == 0)
    wall_channel = np.maximum(channels[0], swatch_walls * 255) * (roof_channel < 128)
    Image.merge('RGB', [Image.fromarray(c.astype(np.uint8)) for c in (wall_channel, roof_channel, reference)]).save(out)
    return out


def _swatches(atlas, models):
    """Each model's dominant wall and roof swatch (see the module's notes): the wall texels, the roof texels (0/1 arrays)
    and the blue channel, the brightness of the swatches marked (0..255, 0 elsewhere)."""
    rgb = np.asarray(Image.open(atlas).convert('RGB').resize((SIZE, SIZE), Image.NEAREST), np.float32) / 255.0
    light = rgb @ np.array([0.299, 0.587, 0.114], np.float32)
    reference = np.zeros((SIZE, SIZE), np.float32)
    marks = {'wall': np.zeros((SIZE, SIZE), bool), 'roof': np.zeros((SIZE, SIZE), bool)}
    # a swatch may be one model's wall and another's roof: it goes to what covers more of them, all models together
    votes = {}
    for model in models:
        tris, uvs = _faces(Path(model), atlas)
        if not len(tris):
            continue
        heights = tris[:, :, 1]
        low, high = heights.min(), heights.max()
        normals = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
        lengths = np.linalg.norm(normals, axis=1)
        areas = {'wall': {}, 'roof': {}}
        totals = {'wall': 0.0, 'roof': 0.0}
        for tri, uv, normal, length in zip(tris, uvs, normals, lengths):
            if length < 1e-9:
                continue
            up = normal[1] / length
            kind = 'wall' if abs(up) < 0.3 else 'roof' if up > 0.45 and tri[:, 1].mean() > low + 0.55 * (high - low) else None
            if not kind:
                continue
            points = np.asarray(uv) * SIZE
            totals[kind] += length / 2
            if np.ptp(points, axis=0).max() > 3:   # a face spread over the atlas: a painted panel, not a swatch
                continue
            texel = tuple(np.clip(points.mean(axis=0).astype(int), 0, SIZE - 1))
            areas[kind][texel] = areas[kind].get(texel, 0.0) + length / 2
        for kind in ('wall', 'roof'):
            if not areas[kind]:
                continue
            texel, area = max(areas[kind].items(), key=lambda item: item[1])
            if area > 0.4 * totals[kind]:
                vote = votes.setdefault(texel, {'wall': 0.0, 'roof': 0.0})
                vote[kind] += area
    taken = np.zeros((SIZE, SIZE), bool)
    for texel, vote in sorted(votes.items(), key=lambda item: -max(item[1].values())):
        if taken[texel[1], texel[0]]:
            continue
        patch = _patch(rgb, texel)
        marks['wall' if vote['wall'] >= vote['roof'] else 'roof'] |= patch & ~taken
        taken |= patch
    for kind in ('wall', 'roof'):
        reference[marks[kind]] = light[marks[kind]]
    return marks['wall'].astype(np.int32), marks['roof'].astype(np.int32), (np.clip(reference, 0, 1) * 255).astype(np.int32)


def _patch(rgb, texel, tolerance=0.03, reach=12):
    """The texels of one color around 'texel' (a swatch), at most 'reach' texels away."""
    x, y = texel
    x0, x1 = max(0, x - reach), min(SIZE, x + reach + 1)
    y0, y1 = max(0, y - reach), min(SIZE, y + reach + 1)
    window = rgb[y0:y1, x0:x1]
    same = np.abs(window - rgb[y, x]).max(axis=2) <= tolerance
    # only what joins the texel (a flood fill), not every square of a similar color nearby
    region = np.zeros_like(same)
    stack = [(y - y0, x - x0)]
    while stack:
        cy, cx = stack.pop()
        if cy < 0 or cx < 0 or cy >= same.shape[0] or cx >= same.shape[1] or region[cy, cx] or not same[cy, cx]:
            continue
        region[cy, cx] = True
        stack += [(cy + 1, cx), (cy - 1, cx), (cy, cx + 1), (cy, cx - 1)]
    out = np.zeros((SIZE, SIZE), bool)
    out[y0:y1, x0:x1] = region
    return out


def building_models(atlas, pattern='*.gltf'):
    """The pack's building models (models/<pattern> beside the atlas's textures folder: the brief's paint
    "building_models", by the pack's naming) that sample it."""
    folder = Path(atlas).resolve().parent.parent / 'models'
    return [m for m in sorted(folder.glob(pattern)) if '__' not in m.name and atlas_of(m, [ATLAS.match(Path(atlas).name).group('pack')])]
