"""Puts the free (CC0) asset packs the Zombies example uses where the tools expect them.

The packs are Quaternius's, dedicated to the public domain (CC0 1.0). They are not kept in the repository: download each zip
from its page (the download button there) and run this script, which finds the zips in your Downloads folder (or takes them
with --zip, or downloads them with --url), checks that each one carries its CC0 license and unpacks it into place:

    python tools/fetch_free_assets.py                      # looks in ~/Downloads
    python tools/fetch_free_assets.py --zip "D:/x/Zombie Apocalypse Kit.zip"
    python tools/fetch_free_assets.py --url zombie-kit=https://example.com/kit.zip

Packs already in place are left alone. Then: python tools/zombies_convert/build_downtown.py (see docs/ZOMBIES_DOWNTOWN.md).
"""
import argparse
import fnmatch
import shutil
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ZOMBIES = ROOT / 'examples' / 'zombies_module'

# name, page to download it from, where it goes, zip name patterns, a path that marks the pack's top folder
PACKS = [
    ('zombie-kit', 'https://quaternius.com/packs/zombieapocalypsekit.html',
     ZOMBIES / 'ZombieApocalypseKit-March-2024', ['*zombie*apocalypse*.zip'], 'Characters'),
    ('downtown-city', 'https://quaternius.com (Downtown City MegaKit, Standard version)',
     ZOMBIES / 'assets' / 'source' / 'downtown-city', ['*downtown*city*.zip'], 'Exports'),
    ('ual1', 'https://quaternius.com/packs/universalanimationlibrary.html',
     ZOMBIES / 'assets' / 'source' / 'universal-animation-library', ['*universal*animation*library*.zip'],
     'Unreal-Godot/UAL1_Standard.glb'),
    ('ual2', 'https://quaternius.com/packs/universalanimationlibrary2.html',
     ZOMBIES / 'assets' / 'source' / 'universal-animation-library-2', ['*universal*animation*library*2*.zip'],
     'Unreal-Godot/UAL2_Standard.glb'),
    ('nature', 'https://quaternius.com (Stylized Nature MegaKit, Standard version)',
     ZOMBIES / 'assets' / 'source' / 'nature-kit', ['*stylized*nature*.zip'], 'glTF/CommonTree_1.gltf'),
    ('medieval', 'https://quaternius.com (Medieval Village MegaKit, Standard version)',
     ZOMBIES / 'assets' / 'source' / 'medieval-village', ['*medieval*village*.zip'], 'glTF/Wall_Plaster_Straight.gltf'),
]


def shown(path):
    return path.relative_to(ROOT) if ROOT in path.parents else path


def installed(target):
    return any(target.glob('License*.txt'))


def safe_extract(archive, into):
    """Unpacks a zip without letting any entry escape 'into' (absolute paths, ..)."""
    with zipfile.ZipFile(archive) as zf:
        base = into.resolve()
        for member in zf.infolist():
            destination = (into / member.filename).resolve()
            if base != destination and base not in destination.parents:
                sys.exit(f'{archive}: the entry {member.filename!r} points outside the folder; not unpacked')
        zf.extractall(into)


def find_top(folder, marker):
    """The folder of the unpacked zip that holds the pack (the zip may wrap it in one or more folders)."""
    for candidate in [folder] + sorted(p for p in folder.rglob('*') if p.is_dir()):
        if (candidate / marker).exists():
            return candidate
    return None


def is_cc0(top):
    licenses = list(top.glob('License*.txt'))
    return bool(licenses) and all('CC0' in f.read_text(encoding='utf-8', errors='replace') for f in licenses)


def install(name, archive, target, marker):
    with tempfile.TemporaryDirectory() as temp:
        temp = Path(temp)
        safe_extract(archive, temp)
        top = find_top(temp, marker)
        if not top:
            print(f'  {name}: {archive.name} is not this pack (no {marker} inside)')
            return False
        if not is_cc0(top):
            print(f'  {name}: {archive.name} has no CC0 license file; not installed')
            return False
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(top, target)
    print(f'  {name}: installed from {archive.name} -> {shown(target)}')
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--downloads', type=Path, default=Path.home() / 'Downloads', help='where to look for the zips')
    parser.add_argument('--zip', type=Path, action='append', default=[], help='a pack zip (repeatable)')
    parser.add_argument('--url', action='append', default=[], help='NAME=URL of a pack zip to download (repeatable)')
    args = parser.parse_args()

    urls = dict(item.split('=', 1) for item in args.url)
    zips = list(args.zip)
    if args.downloads.is_dir():
        zips += sorted(args.downloads.glob('*.zip'))

    missing = []
    with tempfile.TemporaryDirectory() as downloads:
        for name, page, target, patterns, marker in PACKS:
            if installed(target):
                print(f'  {name}: already in {shown(target)}')
                continue
            candidates = [z for z in zips if any(fnmatch.fnmatch(z.name.lower(), p) for p in patterns)]
            if name in urls:
                fetched = Path(downloads) / f'{name}.zip'
                print(f'  {name}: downloading {urls[name]}')
                urllib.request.urlretrieve(urls[name], fetched)
                candidates.insert(0, fetched)
            if not any(install(name, archive, target, marker) for archive in candidates):
                missing.append((name, page))

    if missing:
        print('\nStill missing (download the zip from its page, then run this again):')
        for name, page in missing:
            print(f'  {name}: {page}')
        sys.exit(1)
    print('\nAll the free packs are in place.')


if __name__ == '__main__':
    main()
