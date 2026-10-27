"""Helpers to build Zombies levels from the Quaternius glTF kits (Downtown City, the Zombie Apocalypse kit).

Models are packaged next to the level, one folder each, with their buffers and textures (the renderer reads texture
files). Positions are in meters here; the level file stores centimeters.
"""
import base64
import hashlib
import itertools
import json
import math
import os
import shutil
import subprocess
from pathlib import Path
from urllib.parse import unquote

import numpy as np

REPO = Path(__file__).resolve().parents[2]
CITY = REPO / 'examples/zombies_module/assets/source/downtown-city'
KIT = REPO / 'examples/zombies_module/ZombieApocalypseKit-March-2024'
NATURE = REPO / 'examples/zombies_module/assets/source/nature-kit'
MEDIEVAL = REPO / 'examples/zombies_module/assets/source/medieval-village'
# Models the level builder generates (buildings from modular pieces): made again from their recipes, not in the repository
GENERATED = REPO / 'examples/zombies_module/assets/generated'
# Our own pieces for what the packs lack (ground tiles), made by tools/levelbuilder/own_assets.py and kept in the repository
OWN = REPO / 'examples/zombies_module/assets/own'


def find_model(name):
    """Path of <name>.gltf in the city, the kit, the nature kit or our own pieces."""
    for root in (CITY / 'Exports', KIT, NATURE / 'glTF', MEDIEVAL / 'glTF', OWN, GENERATED):
        found = list(root.rglob(name + '.gltf'))
        if found:
            if len(found) > 1:
                found = [f for f in found if 'glTF' in str(f)] or found
            return found[0]
    raise FileNotFoundError(name)


def _local_matrix(node):
    if 'matrix' in node:
        return np.array(node['matrix']).reshape(4, 4, order='F')
    x, y, z, w = node.get('rotation', [0, 0, 0, 1])
    m = np.eye(4)
    m[:3, :3] = np.array([
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]) @ np.diag(node.get('scale', [1, 1, 1]))
    m[:3, 3] = node.get('translation', [0, 0, 0])
    return m


def model_bounds(path):
    """(low, high) corners of a glTF in meters, Y up, with the node transforms applied."""
    doc = json.loads(Path(path).read_text())
    points = []

    def visit(index, parent):
        node = doc['nodes'][index]
        world = parent @ _local_matrix(node)
        if 'mesh' in node:
            for prim in doc['meshes'][node['mesh']]['primitives']:
                a = doc['accessors'][prim['attributes']['POSITION']]
                for corner in itertools.product(*zip(a['min'], a['max'])):
                    points.append((world @ [*corner, 1])[:3])
        for child in node.get('children', []):
            visit(child, world)

    for root in doc['scenes'][doc.get('scene', 0)]['nodes']:
        visit(root, np.eye(4))
    return np.min(points, axis=0), np.max(points, axis=0)


_content_keys = {}


def _content_key(path):
    """A short key of a file's contents (cached by path, size and time)."""
    stat = path.stat()
    key = (str(path), stat.st_size, stat.st_mtime)
    if key not in _content_keys:
        _content_keys[key] = hashlib.md5(path.read_bytes()).hexdigest()[:10]
    return _content_keys[key]


def export_model(name, level_dir):
    """Copies a model with its buffers and textures into <level>/meshes/<name>/ . Returns the path relative to the level."""
    source = find_model(name)
    target_dir = Path(level_dir) / 'meshes' / name
    relative = f'meshes/{name}/{name}.gltf'
    if (target_dir / f'{name}.gltf').is_file():
        return relative
    doc = json.loads(source.read_text())
    target_dir.mkdir(parents=True, exist_ok=True)
    for index, image in enumerate(doc.get('images', [])):
        if 'bufferView' not in image:
            continue
        view = doc['bufferViews'][image.pop('bufferView')]
        uri = doc['buffers'][view['buffer']]['uri']
        raw = base64.b64decode(uri.split(',', 1)[1]) if uri.startswith('data:') else (source.parent / unquote(uri)).read_bytes()
        start = view.get('byteOffset', 0)
        suffix = '.png' if image['mimeType'] == 'image/png' else '.jpg'
        file = f'embedded_{index}{suffix}'
        (target_dir / file).write_bytes(raw[start:start + view['byteLength']])
        image['uri'] = file
    textures = CITY / 'Textures'
    shared = Path(level_dir) / 'meshes' / '_shared'
    images = {id(entry) for entry in doc.get('images', [])}
    for entry in doc.get('buffers', []) + doc.get('images', []):
        uri = entry.get('uri', '')
        if not uri or uri.startswith('data:') or uri.startswith('embedded_'):
            continue
        path = source.parent / unquote(uri)
        if not path.is_file():
            matches = list(textures.rglob(Path(unquote(uri)).name))
            if len(matches) != 1:
                raise ValueError(f'Missing or ambiguous dependency {uri} of {name}')
            path = matches[0]
        if id(entry) in images:
            # textures go to one folder shared by every model, once per content: the generated buildings all name the
            # pack's textures, and a copy in each model's folder came to gigabytes
            shared.mkdir(parents=True, exist_ok=True)
            file = f'{_content_key(path)}_{path.name}'
            if not (shared / file).is_file():
                shutil.copy2(path, shared / file)
            entry['uri'] = f'../_shared/{file}'
        else:
            shutil.copy2(path, target_dir / path.name)
            entry['uri'] = path.name
    (target_dir / f'{name}.gltf').write_text(json.dumps(doc), encoding='utf-8')
    return relative


UAL1 = REPO / 'examples/zombies_module/assets/source/universal-animation-library/Unreal-Godot/UAL1_Standard.glb'
UAL2 = REPO / 'examples/zombies_module/assets/source/universal-animation-library-2/Unreal-Godot/UAL2_Standard.glb'
BLENDER = Path(os.environ.get('PAKAL_BLENDER', r'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe'))


def export_retargeted(name, clips, level_dir, source=UAL1, strip=()):
    """A kit character with clips of the Universal Animation Library copied onto it (tools/blender/retarget_ual.py), as
    <level>/meshes/<name>_UAL/<name>_UAL.glb. Made again only when the character, the clips or the script change. Returns
    the path relative to the level. 'strip': objects of the character left out (the weapons it holds)."""
    target = find_model(name)
    script = REPO / 'tools/blender/retarget_ual.py'
    out_dir = Path(level_dir) / 'meshes' / f'{name}_UAL'
    out = out_dir / f'{name}_UAL.glb'
    relative = f'meshes/{name}_UAL/{name}_UAL.glb'
    key = hashlib.sha1(target.read_bytes() + script.read_bytes() + json.dumps([sorted(clips), str(source), sorted(strip)]).encode()).hexdigest()
    stamp = out_dir / 'retarget.key'
    if out.is_file() and stamp.is_file() and stamp.read_text() == key:
        return relative
    out_dir.mkdir(parents=True, exist_ok=True)
    out.unlink(missing_ok=True)
    subprocess.check_call([str(BLENDER), '--background', '--python', str(script), '--', '--target', str(target),
                           '--source', str(source), '--clips', ','.join(clips), '--out', str(out), '--strip', ','.join(strip)],
                          stdout=subprocess.DEVNULL)
    # Blender exits with 0 even when the script fails: the model must be there
    if not out.is_file():
        raise RuntimeError(f'retargeting {name} wrote no {out} (run tools/blender/retarget_ual.py by hand to see why)')
    stamp.write_text(key)
    return relative


def footprint(low, high, x, y, yaw_degrees, scale=1.0, shrink=0.0):
    """Corners (meters, level axes x east / y north) of the floor rectangle of a model placed at (x, y) with a yaw.

    The model's own axes: X east and Z south when unrotated (a glTF front faces +Z, i.e. level south).
    'shrink' pulls the rectangle in on every side (a collider a little tighter than the picture).
    """
    x0, x1 = low[0] * scale + shrink, high[0] * scale - shrink
    z0, z1 = low[2] * scale + shrink, high[2] * scale - shrink
    corners = [(x0, z0), (x1, z0), (x1, z1), (x0, z1)]
    a = math.radians(yaw_degrees)
    c, s = math.cos(a), math.sin(a)
    out = []
    for mx, mz in corners:
        lx, ly = mx, -mz              # model Z is level -Y
        out.append((x + lx * c - ly * s, y + lx * s + ly * c))
    return out
