"""Index of every file of the free packs (tools/fetch_free_assets.py installs them): what each one is and what uses it.

    python tools/assets_index.py

For each pack: every model (family, triangles, size in meters, materials, the textures it reads, skinned or animated) and
every texture (resolution, kind: base color, normal, ORM, roughness, palette, HDR; the models that use it), plus the other
files (FBX/OBJ/Blend exports, licenses, previews). Writes tools/levelbuilder/assets_index.json (all of it, generated) and
docs/ASSET_INDEX.md (the summary to read). Run it again after installing a pack.
"""
import collections
import json
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / 'zombies_convert'))
import citykit  # noqa: E402

REPO = HERE.parent
SOURCE = REPO / 'examples/zombies_module/assets/source'
PACKS = {
    'downtown-city': (SOURCE / 'downtown-city', 'Downtown City MegaKit: streets, sidewalks, three whole buildings and the modular '
                      'pieces to build more (brick and metal walls, windows, columns, cornices, roofs, doors, stairs), urban props'),
    'medieval-village': (SOURCE / 'medieval-village', 'Medieval Village MegaKit: modular house pieces (plaster, brick and wood walls, '
                         'doors, windows and shutters, corners, floors, tiled and brick roofs, balconies, stairs), props'),
    'nature-kit': (SOURCE / 'nature-kit', 'Stylized Nature MegaKit: trees (common, pine, dead, twisted), bushes, grass, flowers, '
                   'ferns, mushrooms, rocks, stone paths'),
    'zombie-kit': (REPO / 'examples/zombies_module/ZombieApocalypseKit-March-2024', 'Zombie Apocalypse Kit: survivors, zombies, '
                   'dogs, weapons, vehicles, street props (one shared color atlas)'),
    'ual1': (SOURCE / 'universal-animation-library', 'Universal Animation Library: a mannequin with hundreds of clips (retargeted '
             'onto the kit characters)'),
    'ual2': (SOURCE / 'universal-animation-library-2', 'Universal Animation Library 2: more clips, female mannequin'),
}

# What some textures are for, when the name does not say it
NOTES = {
    'T_Dirt': 'tileable dirt ground (PBR: base color, normal, ORM)',
    'T_Concrete': 'tileable concrete (sidewalks, curbs)',
    'T_Concrete_Asphalt': 'tileable asphalt',
    'T_MarbleFloor': 'tileable light marble floor',
    'T_MetalConcrete': 'metal and concrete facade material',
    'T_RedBrick': 'tileable red brick walls',
    'T_RoofSlate': 'tileable slate roof',
    'T_Trim': 'trims and frames atlas',
    'T_Ornaments': 'facade ornaments atlas',
    'T_Street_Decals': 'road markings atlas (arrows, lines, crosswalks)',
    'T_lit_interior': 'fake lit interior behind windows',
    'T_dark_interior': 'fake dark interior behind windows',
    'CM_': 'interior cube map (HDR) for windows',
    'T_Blinds': 'window blinds overlay',
    'T_Curtains': 'window curtains overlay',
    'T_CornerDamage': 'damaged corners normal detail',
    'T_Noise_Drips': 'dripping grime mask',
    'T_Brick': 'tileable brick (medieval walls)',
    'T_Plaster': 'tileable plaster walls',
    'T_RockTrim': 'stone trims and foundations',
    'T_RoundTiles': 'tileable clay roof tiles',
    'T_UnevenBrick': 'tileable cobblestone / uneven brick (walls, floors, paths)',
    'T_WoodTrim': 'wooden beams and trims',
    'T_MetalOrnaments': 'iron ornaments',
    'T_VineLeaf': 'vine leaves (alpha)',
    'T_BottomWear': 'grime mask for the bottom of walls',
    'T_TopWear': 'wear mask for the top of walls',
    'T_BrushedNoise': 'brushed noise mask',
    'T_Noise_Terrain': 'grayscale terrain noise (blend mask)',
    'T_WindowGradient': 'window glass gradient',
    'Grass': 'color palette strip for the grass and plant meshes (not a ground texture)',
    'Flowers': 'flower petals atlas',
    'Leaves': 'leaves atlas (alpha)',
    'Leaf_Pine': 'pine needles (alpha)',
    'Bark_': 'tree bark',
    'Mushrooms': 'mushrooms atlas',
    'PathRocks_Diffuse': 'top-down cobbles on grass (stone paths)',
    'Rocks_Desert_Diffuse': 'sandy rock surface (desert rocks; usable as sand)',
    'Rocks_Diffuse': 'gray rock surface',
    'Zombie_Atlas': 'the zombie kit color atlas (every kit model)',
}


def png_size(path):
    with open(path, 'rb') as f:
        head = f.read(24)
    if head[:8] == b'\x89PNG\r\n\x1a\n':
        return struct.unpack('>II', head[16:24])
    return None


def texture_kind(name):
    lower = name.lower()
    if lower.endswith('.hdr'):
        return 'hdr'
    for key, kind in (('normal', 'normal'), ('_orm', 'orm'), ('roughness', 'roughness'), ('basecolor', 'base_color'),
                      ('diffuse', 'base_color'), ('preview', 'preview'), ('setup', 'preview')):
        if key in lower:
            return kind
    if lower.startswith(('grass', 'zombie_atlas')):
        return 'palette'
    return 'color'


def note_for(name):
    for key in sorted(NOTES, key=len, reverse=True):
        if name.startswith(key):
            return NOTES[key]
    return ''


def model_info(path):
    doc = json.loads(path.read_text(encoding='utf-8'))
    tris = 0
    for mesh in doc.get('meshes', []):
        for prim in mesh['primitives']:
            if 'indices' in prim:
                tris += doc['accessors'][prim['indices']]['count'] // 3
            else:
                tris += doc['accessors'][prim['attributes']['POSITION']]['count'] // 3
    textures = []
    for image in doc.get('images', []):
        if 'uri' in image and not image['uri'].startswith('data:'):
            textures.append(Path(image['uri']).name)
        elif 'bufferView' in image or image.get('uri', '').startswith('data:'):
            textures.append('(embedded)')
    info = {'triangles': tris, 'materials': [m.get('name', '') for m in doc.get('materials', [])], 'textures': sorted(set(textures)),
            'skinned': bool(doc.get('skins')), 'animations': len(doc.get('animations', []))}
    try:
        low, high = citykit.model_bounds(path)
        info['size'] = [round(float(high[0] - low[0]), 2), round(float(high[2] - low[2]), 2), round(float(high[1] - low[1]), 2)]
    except Exception:  # noqa: BLE001 - a model without plain meshes (only a skeleton) has no bounds
        info['size'] = None
    return info


def family(name):
    return name.split('_')[0]


def index_pack(key, folder):
    files = [p for p in folder.rglob('*') if p.is_file()]
    models, textures, other = {}, {}, collections.Counter()
    for path in files:
        suffix = path.suffix.lower()
        if suffix == '.gltf':
            if path.stem in models:
                continue   # the same model in another folder (the kit repeats some)
            info = model_info(path)
            info['path'] = path.relative_to(REPO).as_posix()
            info['family'] = family(path.stem)
            models[path.stem] = info
        elif suffix in ('.png', '.jpg', '.hdr'):
            if path.name in textures:
                textures[path.name]['copies'] += 1
                continue
            size = png_size(path) if suffix == '.png' else None
            textures[path.name] = {'path': path.relative_to(REPO).as_posix(), 'kind': texture_kind(path.name),
                                   'size': list(size) if size else None, 'bytes': path.stat().st_size, 'copies': 1,
                                   'note': note_for(path.stem), 'used_by': []}
        else:
            other[suffix or path.name] += 1
    for name, info in models.items():
        for texture in info['textures']:
            if texture in textures:
                textures[texture]['used_by'].append(name)
    return {'folder': folder.relative_to(REPO).as_posix(), 'models': models, 'textures': textures, 'other_files': dict(other)}


def markdown(index):
    lines = ['# Asset index', '',
             'Every file of the free packs (Quaternius, CC0) installed by `tools/fetch_free_assets.py`, made by `tools/assets_index.py`',
             '(the full data, per file, is in `tools/levelbuilder/assets_index.json`). The level builder\'s catalog',
             '(`tools/levelbuilder/catalog.py`) takes its models from these packs; the textures listed here can be used for new',
             'pieces of our own (`tools/levelbuilder/own_assets.py`).', '']
    for key, pack in index.items():
        lines += [f'## {key}', '', PACKS[key][1] + '.', '', f'Folder: `{pack["folder"]}`. Other files: '
                  + ', '.join(f'{n} {k}' for k, n in sorted(pack['other_files'].items())) + '.', '']
        if pack['models']:
            lines += ['| Family | Models | Triangles (min-max) | Examples |', '|---|---|---|---|']
            families = collections.defaultdict(list)
            for name, info in pack['models'].items():
                families[info['family']].append((name, info))
            for fam, items in sorted(families.items()):
                tris = [i['triangles'] for _, i in items]
                names = sorted(n for n, _ in items)
                lines.append(f'| {fam} | {len(items)} | {min(tris)}-{max(tris)} | {", ".join(names[:6])}{" ..." if len(names) > 6 else ""} |')
            lines.append('')
        real = {n: t for n, t in pack['textures'].items() if t['kind'] != 'preview'}
        if real:
            lines += ['| Texture | Kind | Size | Used by | What it is |', '|---|---|---|---|---|']
            for name, t in sorted(real.items()):
                size = f'{t["size"][0]}x{t["size"][1]}' if t['size'] else ''
                used = f'{len(t["used_by"])} models' if t['used_by'] else '-'
                lines.append(f'| {name} | {t["kind"]} | {size} | {used} | {t["note"]} |')
            lines.append('')
    return '\n'.join(lines)


def main():
    index = {}
    for key, (folder, _) in PACKS.items():
        if not folder.is_dir():
            print(f'{key}: not installed ({folder}); see tools/fetch_free_assets.py')
            continue
        index[key] = index_pack(key, folder)
        print(f'{key}: {len(index[key]["models"])} models, {len(index[key]["textures"])} textures')
    (HERE / 'levelbuilder' / 'assets_index.json').write_text(json.dumps(index, indent=1), encoding='utf-8')
    (REPO / 'docs' / 'ASSET_INDEX.md').write_text(markdown(index) + '\n', encoding='utf-8')
    print('written: tools/levelbuilder/assets_index.json, docs/ASSET_INDEX.md')


if __name__ == '__main__':
    main()
