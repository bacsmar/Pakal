"""Build a reversible city-art variant of an already converted Survival level.

Only visual objects change. Physics, scripts, spawners and role models are shared.
Static replacement bounds are fitted to explicit original render-space bounds.
"""
import argparse
import base64
import copy
import itertools
import json
import shutil
from pathlib import Path
from urllib.parse import unquote

import numpy as np


def bounds(doc):
    points = []
    def visit(index, parent):
        n = doc['nodes'][index]
        if 'matrix' in n:
            local = np.array(n['matrix']).reshape(4, 4, order='F')
        else:
            x, y, z, w = n.get('rotation', [0, 0, 0, 1])
            local = np.eye(4)
            local[:3, :3] = np.array([
                [1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]]) @ np.diag(n.get('scale', [1, 1, 1]))
            local[:3, 3] = n.get('translation', [0, 0, 0])
        world = parent @ local
        if 'mesh' in n:
            for prim in doc['meshes'][n['mesh']]['primitives']:
                a = doc['accessors'][prim['attributes']['POSITION']]
                for corner in itertools.product(*zip(a['min'], a['max'])):
                    points.append((world @ [*corner, 1])[:3])
        for child in n.get('children', []):
            visit(child, world)
    for root in doc['scenes'][doc.get('scene', 0)]['nodes']:
        visit(root, np.eye(4))
    return np.min(points, axis=0), np.max(points, axis=0)


def package(source, target, lo, hi, textures):
    doc = json.loads(source.read_text())
    lower, upper = bounds(doc)
    scale = (np.array(hi)-lo) / (upper-lower)
    offset = np.array(lo)-lower*scale
    roots = doc['scenes'][doc.get('scene', 0)]['nodes']
    wrapper = len(doc['nodes'])
    doc['nodes'].append({'name': 'FitOriginalFootprint', 'children': roots,
                         'scale': scale.tolist(), 'translation': offset.tolist()})
    doc['scenes'][doc.get('scene', 0)]['nodes'] = [wrapper]
    target.parent.mkdir(parents=True, exist_ok=True)
    # Assimp's current renderer resolves texture files, not embedded *0 images.
    # Externalize the kit's compressed PNG payloads while keeping UVs/materials.
    for index, image in enumerate(doc.get('images', [])):
        if 'bufferView' not in image:
            continue
        view = doc['bufferViews'][image.pop('bufferView')]
        uri = doc['buffers'][view['buffer']]['uri']
        raw = base64.b64decode(uri.split(',', 1)[1]) if uri.startswith('data:') else (source.parent/unquote(uri)).read_bytes()
        start = view.get('byteOffset', 0)
        suffix = '.png' if image['mimeType'] == 'image/png' else '.jpg'
        name = f'embedded_{index}{suffix}'
        (target.parent/name).write_bytes(raw[start:start+view['byteLength']])
        image['uri'] = name
    for entry in doc.get('buffers', []) + doc.get('images', []):
        uri = entry.get('uri', '')
        if not uri or uri.startswith('data:'):
            continue
        path = source.parent / unquote(uri)
        if (target.parent / uri).is_file() and uri.startswith('embedded_'):
            continue
        if not path.is_file():
            matches = list(textures.rglob(Path(unquote(uri)).name))
            if len(matches) != 1:
                raise ValueError(f'Missing or ambiguous dependency: {uri}')
            path = matches[0]
        destination = target.parent / path.name
        shutil.copy2(path, destination)
        entry['uri'] = destination.name
    target.write_text(json.dumps(doc), encoding='utf-8')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--base', type=Path, default=Path('build-win64/bin/Assets/zombies/hollywood_s01t01'))
    parser.add_argument('--out', type=Path, default=Path('build-win64/bin/Assets/zombies/hollywood_city_preview'))
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    city = repo/'examples/zombies_module/assets/source/downtown-city'
    kit = repo/'examples/zombies_module/ZombieApocalypseKit-March-2024'
    # Original bounds from the renderer after Assimp unit/up-axis conversion (meters).
    replacements = {
        'pr_safetycone_02': ('TrafficCone_1', [-.4, 0, -.5], [.5, 1, .5]),
        'pr_firehydrant_03': ('FireHydrant', [-.6, 0, -.6], [.6, 1.7, .6]),
        'pr_barrier_waterbarrier1': ('PlasticBarrier', [-1.5, 0, -.4], [1.5, 1.1, .4]),
        'pr_barrier_pedestrian': ('TrafficBarrier_1', [-1.4, 0, -.4], [1.4, 1.7, .4]),
        'pr_barrier_fence': ('TrafficBarrier_2', [-1.4, 0, -.3], [1.4, 2.7, .1]),
        'pr_cars_gazelleblue': ('Vehicle_Pickup', [-1.4, 0, -2.5], [1.4, 1.9, 2.3]),
        'pr_mustan_blue': ('Vehicle_Sports', [-1.7, 0, -2.5], [1.5, 1.8, 2.7]),
        'pr_copcar': ('Vehicle_Pickup_Armored', [-1.6, 0, -2.4], [2, 2.2, 2.8]),
        'pr_streetlight': ('StreetLights', [-3.4, 0, -.3], [.1, 8.6, .3]),
        'pr_silvergarbagecan_03': ('Barrel', [-.6, 0, -.9], [.6, 1.4, .6]),
        'lv_hw_dinerzone_buildings': ('Building_Small_1', [-2, 0, -48.7], [26.5, 9.9, -38.2]),
        'lv_hw_trans_dinerzone': ('Building_Medium_2_001', [35.4, 0, -52.2], [51, 12.4, -36.9]),
        'lv_hw_trans_usedcardealer': ('Building_Small_1', [-.9, 0, -65.4], [13.7, 8, -54.8]),
        'lv_hw_grauman_wall1': ('Building_Small_1', [-67.4, 0, -55.6], [-55.9, 10.5, -39.3]),
        'lv_hw_grauman_wall2': ('Building_Small_1', [-46.8, 0, -64], [-38.6, 10.5, -39.3]),
        'lv_hw_grauman_theatre': ('Building_Large_2', [-66.8, 0, -64], [-41.9, 15.4, -52.7]),
        'lv_hw_whoagoodbld': ('Building_Medium_2_001', [-4.3, 0, -11.3], [48.7, 11, 1.8]),
    }
    args.out.mkdir(parents=True, exist_ok=True)
    level = json.loads((args.base/'level.json').read_text())
    original = copy.deepcopy(level)
    import os
    base_relative = Path(os.path.relpath(args.base, args.out)).as_posix()
    for role, path in level['roles'].items():
        level['roles'][role] = f'{base_relative}/{path}'
    count = 0
    for obj in level['objects']:
        stem = Path(obj['mesh']).stem
        if stem in replacements:
            model, lo, hi = replacements[stem]
            target = args.out/'meshes'/stem/f'{model}.gltf'
            candidates = list((city/'Exports').rglob(model+'.gltf')) or list(kit.rglob(model+'.gltf'))
            if len(candidates) != 1:
                raise ValueError(f'Ambiguous model {model}: {candidates}')
            package(candidates[0], target, lo, hi, city/'Textures')
            obj['mesh'] = target.relative_to(args.out).as_posix()
            count += 1
        else:
            obj['mesh'] = f'{base_relative}/{obj["mesh"]}'
    # Original window overlays belong to the replaced facades.
    level['objects'] = [o for o in level['objects'] if Path(o['mesh']).stem not in
                        {'lv_hw_dinerzone_windows', 'lv_hw_grauman_windows'}]
    for field in ('walls', 'collision', 'script', 'enemies', 'spawners', 'pickups'):
        assert level.get(field) == original.get(field), field
    (args.out/'level.json').write_text(json.dumps(level, indent=2), encoding='utf-8')
    print(f'Visual preview: {count} replaced instances, {len(replacements)} model mappings; gameplay data preserved')


if __name__ == '__main__':
    main()
