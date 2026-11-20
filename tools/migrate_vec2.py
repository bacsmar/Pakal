#!/usr/bin/env python3
"""Moves a game's or a genre's code from Box2D's names to the engine's (docs/adr ADR-0017, phase B of the 2D physics).

    python tools/migrate_vec2.py <folder or file> [...] [--dry-run]

In each .h/.hpp/.inl/.cpp/.cc file, outside comments and strings:
- b2Vec2 -> Pakal::Vec2, b2AABB -> Pakal::Aabb2; b2Dot, b2Cross, b2Distance, b2DistanceSquared -> Pakal::dot, cross, distance, distance_squared
  (since phase C they are the engine's own types, with the same members and the same arithmetic as Box2D 2.4's, so
  nothing computes differently);
- X.native() given to a CharacterController2D -> X (the constructor that takes the PhysicsWorld2D; since phase C the
  only one, and PhysicsWorld2D has no native());
- an #include of a Box2D header (<box2d/...>) is replaced by "math/Vec2.h" when nothing of Box2D is left in the file.
What it cannot move it lists: other Box2D names (b2Body, b2Shape, b2Mul, ... and native()), which want the engine's API
(PhysicsWorld2D::for_each_shape, the PhysicsWorld2D constructors) by hand. Files are written with their own line endings.
"""
import re
import sys
from pathlib import Path

RENAMES = {
    'b2Vec2': 'Pakal::Vec2',
    'b2AABB': 'Pakal::Aabb2',
    'b2DistanceSquared': 'Pakal::distance_squared',
    'b2Distance': 'Pakal::distance',
    'b2Dot': 'Pakal::dot',
    'b2Cross': 'Pakal::cross',
}
EXTENSIONS = {'.h', '.hpp', '.inl', '.cpp', '.cc'}
BOX2D_NAME = re.compile(r'\bb2[A-Z_][A-Za-z0-9_]*|\bb2_[a-z][A-Za-z0-9_]*')
BOX2D_INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*<box2d/[^>]+>[ \t]*\r?\n', re.MULTILINE)
CONTROLLER_NATIVE = re.compile(r'(CharacterController2D\s*(?:>\s*)?\(\s*)([A-Za-z_][\w.\->()]*?)\s*\.\s*native\s*\(\s*\)')
CONTROLLER_NATIVE_PTR = re.compile(r'(CharacterController2D\s*(?:>\s*)?\(\s*)([A-Za-z_][\w.\->()]*?)\s*->\s*native\s*\(\s*\)')
# comments and string/char literals, kept as they are
TOKENS = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.DOTALL)


def code_parts(text):
    """Yields (is_code, piece) over the text: comments and literals are not code."""
    pos = 0
    for match in TOKENS.finditer(text):
        if match.start() > pos:
            yield True, text[pos:match.start()]
        yield False, match.group(0)
        pos = match.end()
    if pos < len(text):
        yield True, text[pos:]


def migrate(text):
    out = []
    for is_code, piece in code_parts(text):
        if is_code:
            piece = re.sub(r'\b(' + '|'.join(sorted(RENAMES, key=len, reverse=True)) + r')\b', lambda m: RENAMES[m.group(1)], piece)
            piece = re.sub(r'\bPakal::Pakal::', 'Pakal::', piece)
            piece = CONTROLLER_NATIVE.sub(r'\1\2', piece)
            piece = CONTROLLER_NATIVE_PTR.sub(r'\1*\2', piece)
        out.append(piece)
    text = ''.join(out)
    without_includes = BOX2D_INCLUDE.sub('', text)
    left = sorted({m.group(0) for is_code, piece in code_parts(without_includes) if is_code for m in BOX2D_NAME.finditer(piece)})
    native = any(is_code and re.search(r'\bnative\s*\(\s*\)', piece) for is_code, piece in code_parts(text))
    if not left and BOX2D_INCLUDE.search(text):
        newline = '\r\n' if '\r\n' in text else '\n'
        first = True

        def replace(match):
            nonlocal first
            keep = first and '#include "math/Vec2.h"' not in text
            first = False
            return f'#include "math/Vec2.h"{newline}' if keep else ''
        text = BOX2D_INCLUDE.sub(replace, text)
    # a file that spells the engine's names includes them itself (it may have had b2Vec2 through another header)
    uses = any(is_code and re.search(r'\bPakal::(Vec2|Aabb2|dot|cross|distance|distance_squared)\b', piece) for is_code, piece in code_parts(text))
    if uses and '#include "math/Vec2.h"' not in text and not BOX2D_INCLUDE.search(text):
        newline = '\r\n' if '\r\n' in text else '\n'
        includes = list(re.finditer(r'^[ \t]*#[ \t]*include[^\n]*\n', text, re.MULTILINE))
        anchor = includes[-1] if includes else re.search(r'^[ \t]*#[ \t]*pragma once[^\n]*\n', text, re.MULTILINE)
        at = anchor.end() if anchor else 0
        text = text[:at] + f'#include "math/Vec2.h"{newline}' + text[at:]
    return text, left, native


def files_of(paths):
    for path in paths:
        path = Path(path)
        if path.is_file():
            yield path
        else:
            yield from sorted(f for f in path.rglob('*') if f.suffix in EXTENSIONS and f.is_file())


def main(argv):
    dry = '--dry-run' in argv
    paths = [a for a in argv if a != '--dry-run']
    if not paths:
        print(__doc__)
        return 2
    changed = 0
    for file in files_of(paths):
        raw = file.read_bytes()
        text = raw.decode('utf-8')
        new, left, native = migrate(text)
        if new != text:
            changed += 1
            print(f'{file}: {"would change" if dry else "changed"}')
            if not dry:
                file.write_bytes(new.encode('utf-8'))
        if left or native:
            print(f'{file}: by hand: {", ".join(left + (["native()"] if native else []))}')
    print(f'{changed} files {"would change" if dry else "changed"}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
