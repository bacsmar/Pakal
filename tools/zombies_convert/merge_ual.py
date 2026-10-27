"""Makes one mannequin with the clips of both Universal Animation Library packs (UAL1 and UAL2 share a skeleton).

The output is a .glb with the Mannequin mesh and every clip of both packs (A_TPose once). The materials get a white main
color and a grey joint color: the level tints the mannequin per role (player, zombies) at run time.

Usage: merge_ual.py --out <mannequin.glb> [--rm]
  --rm  use the root motion versions (not wanted for the game: movement comes from gameplay code)
"""
import argparse
import json
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / 'examples/zombies_module/assets/source'
UAL1 = SOURCE / 'universal-animation-library/Unreal-Godot/UAL1_Standard.glb'
UAL2 = SOURCE / 'universal-animation-library-2/Unreal-Godot/UAL2_Standard.glb'


def read_glb(path):
    data = path.read_bytes()
    json_length = struct.unpack_from('<I', data, 12)[0]
    doc = json.loads(data[20:20 + json_length])
    bin_start = 20 + json_length + 8
    return doc, data[bin_start:]


def write_glb(path, doc, binary):
    text = json.dumps(doc, separators=(',', ':')).encode('utf-8')
    text += b' ' * (-len(text) % 4)
    binary += b'\x00' * (-len(binary) % 4)
    total = 12 + 8 + len(text) + 8 + len(binary)
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', 0x46546C67, 2, total))
        f.write(struct.pack('<II', len(text), 0x4E4F534A) + text)
        f.write(struct.pack('<II', len(binary), 0x004E4942) + binary)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--rm', action='store_true')
    args = parser.parse_args()

    first, second = UAL1, UAL2
    if args.rm:
        first = first.with_name('UAL1_Standard_RM.glb')
        second = second.with_name('UAL2_Standard_RM.glb')
    doc1, bin1 = read_glb(first)
    doc2, bin2 = read_glb(second)

    names1 = [n.get('name') for n in doc1['nodes']]
    names2 = [n.get('name') for n in doc2['nodes']]
    if names1 != names2:
        raise SystemExit('the two packs do not share the node order: retargeting by name is needed')

    # The second pack's binary goes after the first; only what its new clips use is copied
    offset = len(bin1) + (-len(bin1) % 4)
    combined = bin1 + b'\x00' * (-len(bin1) % 4) + bin2
    have = {a['name'] for a in doc1['animations']}
    views = {}
    accessors = {}

    def copy_accessor(index):
        if index in accessors:
            return accessors[index]
        accessor = dict(doc2['accessors'][index])
        view_index = accessor['bufferView']
        if view_index not in views:
            view = dict(doc2['bufferViews'][view_index])
            view['byteOffset'] = view.get('byteOffset', 0) + offset
            doc1['bufferViews'].append(view)
            views[view_index] = len(doc1['bufferViews']) - 1
        accessor['bufferView'] = views[view_index]
        doc1['accessors'].append(accessor)
        accessors[index] = len(doc1['accessors']) - 1
        return accessors[index]

    added = []
    for animation in doc2['animations']:
        if animation['name'] in have:
            continue
        samplers = []
        for sampler in animation['samplers']:
            samplers.append({'input': copy_accessor(sampler['input']), 'output': copy_accessor(sampler['output']),
                             'interpolation': sampler.get('interpolation', 'LINEAR')})
        doc1['animations'].append({'name': animation['name'], 'samplers': samplers, 'channels': animation['channels']})
        added.append(animation['name'])

    doc1['buffers'] = [{'byteLength': len(combined)}]
    # Colors: white body (tinted by the level), mid grey joints
    for material in doc1['materials']:
        factor = [1.0, 1.0, 1.0, 1.0] if material['name'] == 'M_Main' else [0.35, 0.35, 0.38, 1.0]
        material['pbrMetallicRoughness']['baseColorFactor'] = factor
    args.out.parent.mkdir(parents=True, exist_ok=True)
    write_glb(args.out, doc1, combined)
    print('mannequin: %d clips (%d from the second pack), %.1f MB -> %s' % (len(doc1['animations']), len(added), args.out.stat().st_size / 1048576, args.out))
    return 0


if __name__ == '__main__':
    sys.exit(main())
