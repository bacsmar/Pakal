"""Keeps the brawler's moves in step with their clips: a move's clip is stretched over the move (windup + active + recovery),
so its hitbox should come out when the clip reaches its "impact" mark. For each move whose clip (the look's action) has that
mark, the windup becomes the same share of the move as the mark is of the clip; the recovery takes the difference, so the
move lasts as long as before. Clip lengths come from the models (glTF binary files).

Usage: brawler_timing.py [--data examples/brawler_proto/data/brawler.json] [--assets build-win64/bin] [--write]
Without --write it only prints what it would change.
"""
import argparse
import json
import struct
import sys
from pathlib import Path


def clip_lengths(glb):
    """Seconds of each animation of a .glb, by name."""
    data = Path(glb).read_bytes()
    size = struct.unpack('<I', data[12:16])[0]
    gltf = json.loads(data[20:20 + size])
    lengths = {}
    for animation in gltf.get('animations', []):
        lengths[animation['name']] = max(gltf['accessors'][s['input']]['max'][0] for s in animation['samplers'])
    return lengths


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--data', default='examples/brawler_proto/data/brawler.json')
    parser.add_argument('--assets', default='build-win64/bin')
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    path = Path(args.data)
    data = json.loads(path.read_text(encoding='utf-8'))
    changes = 0
    for name, fighter in data.get('fighters', {}).items():
        look = fighter.get('look', {})
        model = Path(args.assets) / look.get('model', '')
        if not model.is_file():
            print(f'{name}: no model {model}')
            continue
        lengths = clip_lengths(model)
        actions = look.get('clips', {}).get('actions', {})
        marks = look.get('marks', {})
        for move in fighter.get('moves', []):
            clips = [c for c in actions.get(move['name'], []) if c in lengths]
            if not clips or 'impact' not in marks.get(clips[0], {}):
                continue
            clip = clips[0]
            total = move.get('windup', 0.0) + move.get('active', 0.1) + move.get('recovery', 0.2)
            windup = round(total * marks[clip]['impact'] / lengths[clip], 3)
            recovery = round(total - windup - move.get('active', 0.1), 3)
            if recovery < 0.0:
                print(f'{name}.{move["name"]}: the mark comes too late in {clip} for a move this short')
                continue
            if abs(windup - move.get('windup', 0.0)) > 0.0005:
                print(f'{name}.{move["name"]}: {clip} ({lengths[clip]:.2f} s, impact at {marks[clip]["impact"]:.2f} s): '
                      f'windup {move.get("windup", 0.0):.3f} -> {windup:.3f}, recovery {move.get("recovery", 0.2):.3f} -> {recovery:.3f}')
                move['windup'], move['recovery'] = windup, recovery
                changes += 1
    if args.write and changes:
        path.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    print(f'{changes} moves {"written" if args.write else "to change (use --write)"}' if changes else 'every move is in step with its clip')
    return 0


if __name__ == '__main__':
    sys.exit(main())
