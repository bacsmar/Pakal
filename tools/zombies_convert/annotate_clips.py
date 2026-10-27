"""Adds the length of each animation clip to the rule objects of a converted level, for the gameplay timing.

The rules name their clips (a weapon's FireAnimation, an enemy's attack and damage clips) but not their lengths, and the
timing depends on them: a gun lets out its shots at TriggerTimes of its fire clip, a zombie's attack lasts its clip. The
lengths come from a folder of rig exports with one '<rig>/validation.txt' per character, whose lines start with
'<clip>\\t<seconds>s'. Each '--rig ID=NAME' says which rig a rule object uses.

    python tools/zombies_convert/annotate_clips.py --level <level.json> --rigs <rig exports> --rig 35001=player_35001 ...

It writes "ClipLengthsMs": {clip: milliseconds} into each object named. Levels built from this one (build_downtown.py) take
the annotated rules with them.
"""
import argparse
import json
import sys
from pathlib import Path


def clip_lengths(validation):
    lengths = {}
    for line in validation.read_text(encoding='utf-8', errors='replace').splitlines():
        parts = line.split('\t')
        if len(parts) >= 2 and parts[1].endswith('s'):
            try:
                lengths[parts[0].strip()] = round(float(parts[1][:-1]) * 1000.0, 1)
            except ValueError:
                pass
    return lengths


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--level', type=Path, required=True)
    parser.add_argument('--rigs', type=Path, required=True, help='folder with <rig>/validation.txt')
    parser.add_argument('--rig', action='append', default=[], help='OBJECT_ID=RIG_NAME (repeatable)')
    args = parser.parse_args()

    level = json.loads(args.level.read_text(encoding='utf-8'))
    objects = level.get('rules', {}).get('objects', {})
    for item in args.rig:
        object_id, rig = item.split('=', 1)
        if object_id not in objects:
            print(f'  {object_id}: not in the level rules, skipped')
            continue
        validation = args.rigs / rig / 'validation.txt'
        if not validation.is_file():
            sys.exit(f'{validation} not found')
        lengths = clip_lengths(validation)
        objects[object_id]['ClipLengthsMs'] = lengths
        print(f'  {object_id} ({objects[object_id].get("name", "?")}): {len(lengths)} clips from {rig}')
    args.level.write_text(json.dumps(level, indent=1), encoding='utf-8')


if __name__ == '__main__':
    main()
