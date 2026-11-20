#!/usr/bin/env python3
"""C-DAT scan: UI built or styled in code, and player-facing text in code (docs/RELEASE_CHECKLIST.md, C-DAT-1 and C-DAT-3;
docs/UI_RELOAD.md).

Screens are .rml/.rcss bound to a data model; game code fills the model and handles named events. This scan reads a game's
C++ sources and reports, as file:line, what goes against that:

  element-creation  elements made or moved in code: CreateElement, AppendChild, InsertBefore, ReplaceChild, RemoveChild,
                    SetInnerRML, and set_markup with markup written in the code
  markup-string     markup in a string literal ("<div ...>", "<span>...")
  inline-style      a style set from code: set_property / SetProperty with a color, size or position property, or a
                    style="..." attribute
  color-literal     a color written in the code: "#rrggbb", "rgb(...)"/"rgba(...)" in a string, or an 8-digit hex number
                    (0xAARRGGBB, ABGR; not a mask after '&'; a hash or another magic number is waived)
  player-text       a string literal with words passed to set_text (text the player reads belongs in the localization file)

A line is waived with a comment on it, or on the line just above it, that gives a reason:

    m_ui->set_text(kDoc, "fps", "FPS");   // cdat-ok: debug overlay, listed in DESIGN.md (C-DAT-5)

A waiver without a reason waives nothing. Comments are not scanned.

Usage:
    python tools/cdat_scan.py examples/zombies_module game_genres/topdown          # the findings, then counts; exit 1 if any
    python tools/cdat_scan.py --summary examples/zombies_module                    # counts only
    python tools/cdat_scan.py --json examples/zombies_module                       # the findings as JSON
    python tools/cdat_scan.py --report-only ...                            # exit 0 even with findings

As a library (tools/release_check.py's source checks call it): scan(paths) -> list of Finding.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable, Iterator, List, Sequence

SOURCE_SUFFIXES = {'.cpp', '.cc', '.cxx', '.h', '.hpp', '.hh', '.inl'}
# folders that are not the game's shipping code
DEFAULT_EXCLUDES = {'tests', 'test', 'external', 'thirdParty', 'third_party', '.git'}

RULES = ('element-creation', 'markup-string', 'inline-style', 'color-literal', 'player-text')

WAIVER = re.compile(r'cdat-ok\s*:\s*(\S.*)?$')

# RmlUi's element API (a game reaching past IUIManager: ADR-0017 forbids it, the scan reports it anyway)
ELEMENT_CALLS = re.compile(r'\b(CreateElement|AppendChild|InsertBefore|ReplaceChild|RemoveChild|SetInnerRML)\s*\(')
MARKUP_TAGS = r'(?:div|span|p|img|button|br|b|i|em|strong|li|ul|ol|h[1-6]|table|tr|td|th|input|select|option|template|body|handle|progress|tabset|tab|panel|textarea)'
MARKUP = re.compile(r'</?' + MARKUP_TAGS + r'\b[^>]*/?>?', re.IGNORECASE)
STYLE_ATTRIBUTE = re.compile(r'\bstyle\s*=\s*\\?["\']', re.IGNORECASE)
# style properties of color, size or position (what C-DAT-1 keeps in the .rcss)
STYLE_PROPERTIES = {
    'color', 'background', 'background-color', 'image-color', 'border-color', 'border-top-color', 'border-right-color',
    'border-bottom-color', 'border-left-color', 'outline-color', 'opacity', 'font-effect', 'decorator', 'box-shadow',
    'width', 'height', 'min-width', 'min-height', 'max-width', 'max-height', 'font-size', 'line-height', 'border-width',
    'border-radius', 'margin', 'margin-top', 'margin-right', 'margin-bottom', 'margin-left', 'padding', 'padding-top',
    'padding-right', 'padding-bottom', 'padding-left', 'left', 'top', 'right', 'bottom', 'position', 'transform',
    'transform-origin', 'z-index', 'display', 'visibility', 'flex-basis', 'gap',
}
STRING = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
HEX_COLOR = re.compile(r'^#(?:[0-9a-fA-F]{3}|[0-9a-fA-F]{4}|[0-9a-fA-F]{6}|[0-9a-fA-F]{8})$')
CSS_COLOR = re.compile(r'\b(?:rgba?|hsla?)\s*\(', re.IGNORECASE)
# an 8-digit hex number (0xAARRGGBB, ABGR), not a mask (x & 0xff000000)
HEX_NUMBER_COLOR = re.compile(r'(?<![&\w])(?<!&\s)0[xX][0-9a-fA-F]{8}(?:[uU][lL]{0,2}|[lL]{1,2}[uU]?)?\b')
WORDS = re.compile(r'[A-Za-z]{2,}')
KEY = re.compile(r'^[A-Z][A-Z0-9_]*_[A-Z0-9_]+$')


@dataclass
class Finding:
    path: str
    line: int
    rule: str
    message: str
    text: str

    def __str__(self) -> str:
        return f'{self.path}:{self.line}: [{self.rule}] {self.message} | {self.text.strip()}'


def strip_comments(lines: Sequence[str]) -> List[str]:
    """Each line with its comments blanked out (string literals kept), so that the rules only see code."""
    out: List[str] = []
    in_block = False
    for line in lines:
        code = []
        i = 0
        in_string = None
        while i < len(line):
            ch = line[i]
            nxt = line[i + 1] if i + 1 < len(line) else ''
            if in_block:
                if ch == '*' and nxt == '/':
                    in_block = False
                    i += 2
                else:
                    i += 1
                continue
            if in_string:
                code.append(ch)
                if ch == '\\' and nxt:
                    code.append(nxt)
                    i += 2
                    continue
                if ch == in_string:
                    in_string = None
                i += 1
                continue
            if ch == '/' and nxt == '/':
                break
            if ch == '/' and nxt == '*':
                in_block = True
                i += 2
                continue
            if ch in '"\'':
                # a digit separator (1'000) is not a character literal
                if ch == "'" and i > 0 and line[i - 1].isalnum() and nxt.isalnum():
                    code.append(ch)
                    i += 1
                    continue
                in_string = ch
            code.append(ch)
            i += 1
        out.append(''.join(code))
    return out


def waived(lines: Sequence[str], index: int) -> bool:
    """A cdat-ok comment with a reason on the line or alone on the line above it."""
    for at in (index, index - 1):
        if at < 0:
            continue
        text = lines[at]
        comment = text.find('//')
        if comment < 0:
            continue
        if at == index - 1 and text[:comment].strip():
            continue   # the line above is code with its own comment: that waiver is its own
        match = WAIVER.search(text[comment:])
        if match and match.group(1) and match.group(1).strip():
            return True
    return False


def call_arguments(code: Sequence[str], index: int, start: int) -> List[str]:
    """The top-level arguments of the call whose '(' is at code[index][start], read across lines (up to 20)."""
    text = ' '.join(code[index:index + 20])   # (code[index] first: 'start' indexes it)
    depth = 0
    args: List[str] = []
    current: List[str] = []
    in_string = None
    i = start
    while i < len(text):
        ch = text[i]
        if in_string:
            current.append(ch)
            if ch == '\\' and i + 1 < len(text):
                current.append(text[i + 1])
                i += 2
                continue
            if ch == in_string:
                in_string = None
        elif ch in '"\'':
            in_string = ch
            current.append(ch)
        elif ch in '([{':
            depth += 1
            if depth > 1:
                current.append(ch)
        elif ch in ')]}':
            depth -= 1
            if depth == 0:
                args.append(''.join(current))
                return args
            current.append(ch)
        elif ch == ',' and depth == 1:
            args.append(''.join(current))
            current = []
        else:
            current.append(ch)
        i += 1
    return args


def literals(text: str) -> List[str]:
    return [m.group(1) for m in STRING.finditer(text)]


def top_level(text: str) -> str:
    """The text without what is inside brackets (a call's arguments: a localization key, a format's values)."""
    out: List[str] = []
    depth = 0
    in_string = None
    i = 0
    while i < len(text):
        ch = text[i]
        if in_string:
            if depth == 0:
                out.append(ch)
            if ch == '\\' and i + 1 < len(text):
                if depth == 0:
                    out.append(text[i + 1])
                i += 2
                continue
            if ch == in_string:
                in_string = None
        elif ch in '"\'':
            in_string = ch
            if depth == 0:
                out.append(ch)
        elif ch in '([{':
            depth += 1
        elif ch in ')]}':
            depth = max(0, depth - 1)
        elif depth == 0:
            out.append(ch)
        i += 1
    return ''.join(out)


def scan_lines(lines: Sequence[str], path: str = '<text>') -> List[Finding]:
    """The findings in one file's lines."""
    code = strip_comments(lines)
    findings: List[Finding] = []

    def add(index: int, rule: str, message: str) -> None:
        if not waived(lines, index):
            findings.append(Finding(path, index + 1, rule, message, lines[index].rstrip('\n')))

    for index, line in enumerate(code):
        if not line.strip():
            continue
        seen = set()

        def once(rule: str, message: str) -> None:
            if rule not in seen:
                seen.add(rule)
                add(index, rule, message)

        for match in ELEMENT_CALLS.finditer(line):
            once('element-creation', f'{match.group(1)}: elements belong in the .rml (data-for for lists)')
        for match in re.finditer(r'\bset_markup\s*\(', line):
            args = call_arguments(code, index, match.end() - 1)
            if len(args) >= 3 and any(MARKUP.search(lit) for lit in literals(','.join(args[2:]))):
                once('element-creation', 'set_markup with markup written in the code')
        line_literals = literals(line)
        if any(MARKUP.search(lit) for lit in line_literals) and 'element-creation' not in seen:
            once('markup-string', 'markup in a string: screens are .rml files')
        for match in re.finditer(r'\b(set_property|SetProperty)\s*\(', line):
            args = call_arguments(code, index, match.end() - 1)
            names = [lit.strip().lower() for arg in args for lit in literals(arg)]
            hit = next((n for n in names if n in STYLE_PROPERTIES), None)
            if hit:
                once('inline-style', f'{match.group(1)} "{hit}": colors, sizes and positions belong in the .rcss (a class, or a data binding)')
        for match in re.finditer(r'\b(set_attribute|SetAttribute)\s*\(', line):
            args = call_arguments(code, index, match.end() - 1)
            if any(lit.strip().lower() == 'style' for arg in args for lit in literals(arg)):
                once('inline-style', f'{match.group(1)} "style": an inline style')
        if any(STYLE_ATTRIBUTE.search(lit) for lit in line_literals):
            once('inline-style', 'style="..." in a string: an inline style')
        if any(HEX_COLOR.match(lit.strip()) or CSS_COLOR.search(lit) for lit in line_literals):
            once('color-literal', 'a color in a string: colors belong in the theme (.rcss)')
        elif HEX_NUMBER_COLOR.search(STRING.sub('""', line)):
            once('color-literal', 'a 0xAARRGGBB color in the code: colors belong in the theme or the game\'s data (not a color: waive it)')
        for match in re.finditer(r'\bset_text\s*\(', line):
            args = call_arguments(code, index, match.end() - 1)
            if len(args) >= 3:
                # the literals given as the text itself (not a call's arguments), other than a localization key
                # (SOME_KEY: the documents show a key as its text)
                texts = [lit for lit in literals(top_level(','.join(args[2:]))) if WORDS.search(lit) and not KEY.match(lit)]
                if texts:
                    once('player-text', f'set_text with "{texts[0]}": player-facing text belongs in the localization file')
    return findings


def source_files(paths: Iterable[os.PathLike | str], excludes: Iterable[str] = DEFAULT_EXCLUDES) -> Iterator[Path]:
    skip = set(excludes)
    for given in paths:
        root = Path(given)
        if root.is_file():
            if root.suffix.lower() in SOURCE_SUFFIXES:
                yield root
            continue
        for folder, dirs, files in os.walk(root):
            dirs[:] = sorted(d for d in dirs if d not in skip and not d.startswith('build'))
            for name in sorted(files):
                if Path(name).suffix.lower() in SOURCE_SUFFIXES:
                    yield Path(folder) / name


def scan(paths: Iterable[os.PathLike | str], excludes: Iterable[str] = DEFAULT_EXCLUDES) -> List[Finding]:
    """The C-DAT findings in the C++ sources under 'paths' (files or folders), file by file and line by line."""
    findings: List[Finding] = []
    for file in source_files(paths, excludes):
        try:
            lines = file.read_text(encoding='utf-8', errors='replace').splitlines()
        except OSError as error:
            print(f'cdat_scan: {file}: {error}', file=sys.stderr)
            continue
        findings.extend(scan_lines(lines, file.as_posix()))
    return findings


def counts(findings: Sequence[Finding]) -> dict:
    by_rule = {rule: 0 for rule in RULES}
    for finding in findings:
        by_rule[finding.rule] = by_rule.get(finding.rule, 0) + 1
    return by_rule


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description='C-DAT scan: UI built or styled in code, player-facing text in code')
    parser.add_argument('paths', nargs='+', help="a game's source folders or files")
    parser.add_argument('--summary', action='store_true', help='the counts only')
    parser.add_argument('--json', action='store_true', help='the findings as JSON')
    parser.add_argument('--report-only', action='store_true', help='exit 0 even with findings')
    parser.add_argument('--exclude', action='append', default=[], help='a folder name to leave out (besides tests, external...)')
    args = parser.parse_args(argv)

    findings = scan(args.paths, DEFAULT_EXCLUDES | set(args.exclude))
    if args.json:
        print(json.dumps({'findings': [asdict(f) for f in findings], 'counts': counts(findings)}, indent=2))
    else:
        if not args.summary:
            for finding in findings:
                print(finding)
        by_rule = counts(findings)
        print(f'C-DAT scan: {len(findings)} finding(s) in {", ".join(str(p) for p in args.paths)}')
        for rule in RULES:
            print(f'  {rule:17} {by_rule.get(rule, 0)}')
    return 0 if args.report_only or not findings else 1


if __name__ == '__main__':
    sys.exit(main())
