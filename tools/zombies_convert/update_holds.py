"""Gives the levels already built the weapons in the hands and the hero's clips of build_downtown.py, without building them
again: in every <assets>/*/level.json whose hero is one of the kit's (its look reaches with KIT_REACH), the melee weapons'
bone, offset, rotation and scale, and the hero's clips standing, walking and running. Nothing else changes.

Usage: update_holds.py [--assets build-win64/bin/Assets/zombies]
"""
import argparse
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(__file__))
import build_downtown  # noqa: E402


def update(level_path):
    """True if the level was changed."""
    level = json.loads(level_path.read_text(encoding='utf-8'))
    player = level.get('visuals', {}).get('player')
    if not player or player.get('reach') != build_downtown.KIT_REACH:
        return False
    changed = False
    for consumable, (_, offset, euler, scale) in build_downtown.KIT_MELEE_WEAPONS.items():
        weapon = player.get('weapons', {}).get(str(consumable))
        if not weapon:
            continue
        hold = {'bone': build_downtown.KIT_WEAPON_BONE, 'offset': offset, 'euler': euler, 'scale': scale}
        if any(weapon.get(key) != value for key, value in hold.items()):
            weapon.update(hold)
            changed = True
    clips = player.setdefault('clips', {})
    for name, names in build_downtown.KIT_HERO_CLIPS.items():
        if clips.get(name) != names:
            clips[name] = names
            changed = True
    if changed:
        level_path.write_text(json.dumps(level, indent=1), encoding='utf-8')
    return changed


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--assets', default='build-win64/bin/Assets/zombies')
    args = parser.parse_args()
    changed = [path.parent.name for path in sorted(Path(args.assets).glob('*/level.json')) if update(path)]
    print(f'{len(changed)} levels updated: {", ".join(changed)}' if changed else 'every level already has them')


if __name__ == '__main__':
    main()
