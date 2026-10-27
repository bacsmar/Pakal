"""Compresses the textures of a staged asset folder for phones: each PNG/TGA becomes a KTX next to it (ASTC 4x4 with mips,
named <name>.png.ktx) and the original is removed. The engine loads <file>.ktx in place of the file a model names (MeshComponent3D_Bgfx).

ASTC is read by every GPU the game targets (Mali-T880 and later, Adreno 5xx and later); where it is not, bgfx decodes it.
Normal maps and the ORM textures of glTF materials are encoded as linear data (the materials say which ones they are).
Results are cached by the source's contents, so only new or changed textures are encoded again.
"""
import hashlib
import json
import os
import shutil
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from urllib.parse import unquote

FORMAT = 'ASTC4x4'  # blocks that divide the power-of-two sizes (6x6 pads 1024 to 1026 and the image comes out sheared)
MAX_SIZE = 1024  # the game draws at 720 px tall from far above: the 2048 level is almost never sampled
LIGHTMAP_MAX_SIZE = 2048
SOURCES = ('.png', '.tga')
ENCODING = 2  # bump when the way textures are encoded changes: the cache is keyed by it


def texture_kinds(stage):
    """{texture path: 'normal' | 'linear'} for the textures glTF materials use as normal maps or data (ORM)."""
    kinds = {}
    for gltf in stage.rglob('*.gltf'):
        doc = json.loads(gltf.read_text(encoding='utf-8'))
        images = doc.get('images', [])
        textures = doc.get('textures', [])

        def path_of(info):
            if not info or info.get('index') is None or info['index'] >= len(textures):
                return None
            source = textures[info['index']].get('source')
            if source is None or source >= len(images) or 'uri' not in images[source]:
                return None
            return (gltf.parent / unquote(images[source]['uri'])).resolve()

        for material in doc.get('materials', []):
            normal = path_of(material.get('normalTexture'))
            if normal:
                kinds[normal] = 'normal'
            pbr = material.get('pbrMetallicRoughness', {})
            for data in (path_of(pbr.get('metallicRoughnessTexture')), path_of(material.get('occlusionTexture'))):
                if data and kinds.get(data) != 'normal':
                    kinds[data] = 'linear'
    # The baked light of the level builder (Assets/zombies/lightmaps): light and shadow amounts, not colors
    for atlas in stage.rglob('lightmaps/*.png'):
        kinds[atlas.resolve()] = 'lightmap'
    return kinds


def encode(texturec, source, target, kind):
    # Lightmap atlases keep their size: halving them leaves the margin around each object's square under a texel
    size = LIGHTMAP_MAX_SIZE if kind == 'lightmap' else MAX_SIZE
    command = [str(texturec), '-f', str(source), '-o', str(target), '-t', FORMAT, '-m', '--max', str(size)]
    # Normal maps go as plain linear data: texturec's -n stores them in another encoding (z dropped) that the shaders do
    # not read. Mips of a normal map averaged as colors are close enough at the distance they are used.
    if kind in ('normal', 'linear', 'lightmap'):
        command.append('--linear')
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0 or not target.exists():
        raise RuntimeError(f'texturec failed on {source}: {result.stdout}{result.stderr}')


def compress_textures(stage, cache, texturec):
    """Replaces every PNG/TGA under stage by its KTX. Returns (count, bytes before, bytes after)."""
    stage, cache = Path(stage), Path(cache)
    cache.mkdir(parents=True, exist_ok=True)
    kinds = texture_kinds(stage)
    # The interface's images stay PNG: RmlUi decodes them itself and knows nothing of the .ktx copies
    sources = [f for f in stage.rglob('*') if f.is_file() and f.suffix.lower() in SOURCES
               and 'ui' not in f.relative_to(stage).parts[:2]]

    # The kit repeats textures in every model's folder: each different one (contents and kind) is encoded once
    keys = {}
    for source in sources:
        kind = kinds.get(source.resolve(), 'color')
        key = hashlib.md5(source.read_bytes() + f'{FORMAT}/{MAX_SIZE}/{LIGHTMAP_MAX_SIZE}/{kind}/{ENCODING}'.encode()).hexdigest()
        keys[source] = key
    first = {}
    for source, key in keys.items():
        first.setdefault(key, source)

    def encode_once(item):
        key, source = item
        cached = cache / f'{key}.ktx'
        if not cached.exists():
            temporary = cache / f'{key}.tmp.ktx'
            encode(texturec, source, temporary, kinds.get(source.resolve(), 'color'))
            os.replace(temporary, cached)

    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
        list(pool.map(encode_once, first.items()))

    before = after = 0
    for source, key in keys.items():
        target = source.with_name(source.name + '.ktx')
        shutil.copyfile(cache / f'{key}.ktx', target)
        before += source.stat().st_size
        after += target.stat().st_size
        source.unlink()
    return len(keys), before, after
