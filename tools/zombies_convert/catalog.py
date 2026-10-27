"""Lays kit models out in a grid as a level, to see their size and orientation in the game.

Usage: catalog.py --out <dir> --base <converted level dir> name [name ...]
Each model sits at the center of a 30 m cell; the player starts at the first cell's side.
"""
import argparse
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(__file__))
import citykit  # noqa: E402


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--base', type=Path, default=Path('build-win64/bin/Assets/zombies/hollywood_s01t01'))
    parser.add_argument('--columns', type=int, default=4)
    parser.add_argument('--cell', type=float, default=30.0)
    parser.add_argument('names', nargs='+')
    args = parser.parse_args()

    base = json.loads((args.base / 'level.json').read_text())
    rel = Path(os.path.relpath(args.base, args.out)).as_posix()
    level = {
        'format': 'pakal-level', 'version': 2, 'name': 'catalog', 'mode': 'Survival', 'units_per_meter': 100,
        'roles': {k: f'{rel}/{v}' for k, v in base['roles'].items()},
        'spawn': [-1500.0, -1500.0], 'walls': [], 'enemies': [], 'pickups': [], 'spawners': [],
        'collision': {'segments': []}, 'script': {'order': [], 'points': {}, 'anchors': {}, 'objectives': {}},
        'rules': base['rules'], 'destructibles': [], 'objects': [],
    }
    args.out.mkdir(parents=True, exist_ok=True)
    for index, name in enumerate(args.names):
        mesh = citykit.export_model(name, args.out)
        low, high = citykit.model_bounds(citykit.find_model(name))
        col, row = index % args.columns, index // args.columns
        x, y = col * args.cell, row * args.cell
        print(f'{name}: cell ({x:.0f}, {y:.0f}) size {high - low}')
        level['objects'].append({'name': name, 'mesh': mesh, 'x': x * 100, 'y': y * 100, 'z': 0, 'rot': [0, 0, 0], 'scale': 1.0})
    (args.out / 'level.json').write_text(json.dumps(level, indent=1), encoding='utf-8')


if __name__ == '__main__':
    main()
