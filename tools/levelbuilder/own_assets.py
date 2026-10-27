"""Piezas propias para lo que los paquetes no traen como modelo: losetas de piso planas (2 triángulos) para zonas.

- Ground_Grass_9x9: pasto; ningún paquete trae una textura de piso de pasto (Grass.png de la naturaleza es la paleta de
  colores de las briznas), así que su textura se hace aquí con ruido y va en el repositorio.
- Ground_Dirt_9x9: tierra con la textura PBR T_Dirt de Downtown (color, normal, ORM).
- Ground_Sand_9x9: arena con Rocks_Desert_Diffuse de la naturaleza.
- Ground_Path_2x2: camino de piedras sobre pasto con PathRocks_Diffuse de la naturaleza: un cuadro plano en lugar de las
  piedras modeladas (RockPath_*, ~2000 triángulos cada una).
- Street_4Lane_Short: un tramo de calle de Downtown acortado a lo que los pasos de cebra del cruce sobresalen de su asfalto
  (3.3 m): sin él, entre las franjas se ve el cielo. Es el mismo modelo con una escala a lo largo (lee su .bin y sus
  texturas del paquete).

Las de textura de paquete la leen de su paquete instalado (ruta relativa a examples/zombies_module/assets/source/); al
exportar un nivel se copia junto al modelo. Mismo pivote que Street_Asphalt_9x9: x y z de -tamaño a 0, a 0.15 m bajo el suelo.

    python tools/levelbuilder/own_assets.py
"""
import base64
import json
import os
import random
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'zombies_convert'))
import citykit  # noqa: E402

OUT = citykit.OWN
SIZE = 512


def noise(seed, scale):
    """Ruido suave que se repite (para que la loseta empate con la de al lado)."""
    rng = np.random.default_rng(seed)
    small = rng.random((SIZE // scale, SIZE // scale))
    image = Image.fromarray((small * 255).astype(np.uint8)).resize((SIZE, SIZE), Image.BICUBIC)
    a = np.asarray(image, dtype=np.float32) / 255.0
    b = np.roll(np.roll(a, SIZE // 2, 0), SIZE // 2, 1)
    ramp = np.abs(np.linspace(-1, 1, SIZE))
    w = np.maximum.outer(ramp, ramp)
    return a * (1 - w) + b * w


def grass_texture(seed=11):
    # verde oliva apagado: el sol, el cielo y el post-proceso del juego lo aclaran (el anterior se veía demasiado brillante)
    low, mid, high = (np.array(c, dtype=np.float32) for c in ((44, 60, 30), (58, 76, 38), (74, 90, 48)))
    n1, n2 = noise(seed, 16), noise(seed + 1, 4)
    t = np.clip(n1 * 0.7 + n2 * 0.3, 0, 1)[..., None]
    rgb = np.where(t < 0.5, low + (mid - low) * (t / 0.5), mid + (high - mid) * ((t - 0.5) / 0.5))
    rgb *= 0.88 + 0.24 * noise(seed + 2, 1)[..., None]
    image = Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8))
    rng = random.Random(seed)
    pixels = image.load()
    for _ in range(SIZE * SIZE // 18):
        x, y = rng.randrange(SIZE), rng.randrange(SIZE)
        shade = rng.uniform(0.75, 1.3)
        for k in range(rng.randint(2, 5)):
            px, py = (x + rng.choice((-1, 0, 0, 1))) % SIZE, (y - k) % SIZE
            r, g, b = pixels[px, py]
            pixels[px, py] = (min(255, int(r * shade)), min(255, int(g * shade)), min(255, int(b * shade)))
    return image.filter(ImageFilter.SMOOTH)


def tile_gltf(name, size, repeat, color, normal=None, orm=None, roughness=0.95):
    """Un cuadro plano de 'size' metros cara arriba; la textura se repite 'repeat' veces por lado."""
    positions = [(-size, -0.15, -size), (0.0, -0.15, -size), (0.0, -0.15, 0.0), (-size, -0.15, 0.0)]
    normals = [(0.0, 1.0, 0.0)] * 4
    tangents = [(1.0, 0.0, 0.0, 1.0)] * 4
    uvs = [(0.0, 0.0), (repeat, 0.0), (repeat, repeat), (0.0, repeat)]
    indices = [0, 2, 1, 0, 3, 2]
    data = b''.join(struct.pack('<3f', *p) for p in positions)
    data += b''.join(struct.pack('<3f', *n) for n in normals)
    data += b''.join(struct.pack('<4f', *t) for t in tangents)
    data += b''.join(struct.pack('<2f', *t) for t in uvs)
    data += b''.join(struct.pack('<H', i) for i in indices)
    images = [{'uri': color}]
    material = {'name': name, 'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}, 'metallicFactor': 0.0,
                                                       'roughnessFactor': roughness}}
    if normal:
        images.append({'uri': normal})
        material['normalTexture'] = {'index': len(images) - 1}
    if orm:
        images.append({'uri': orm})
        material['pbrMetallicRoughness']['metallicRoughnessTexture'] = {'index': len(images) - 1}
        material['occlusionTexture'] = {'index': len(images) - 1}
        material['pbrMetallicRoughness']['roughnessFactor'] = 1.0
    return {
        'asset': {'version': '2.0', 'generator': 'Pakal levelbuilder own_assets.py'},
        'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0, 'name': name}],
        'meshes': [{'name': name, 'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1, 'TANGENT': 2, 'TEXCOORD_0': 3},
                                                  'indices': 4, 'material': 0}]}],
        'materials': [material],
        'textures': [{'source': i, 'sampler': 0} for i in range(len(images))],
        'samplers': [{'magFilter': 9729, 'minFilter': 9987, 'wrapS': 10497, 'wrapT': 10497}],
        'images': images,
        'buffers': [{'byteLength': len(data), 'uri': 'data:application/octet-stream;base64,' + base64.b64encode(data).decode()}],
        'bufferViews': [{'buffer': 0, 'byteOffset': 0, 'byteLength': 48}, {'buffer': 0, 'byteOffset': 48, 'byteLength': 48},
                        {'buffer': 0, 'byteOffset': 96, 'byteLength': 64}, {'buffer': 0, 'byteOffset': 160, 'byteLength': 32},
                        {'buffer': 0, 'byteOffset': 192, 'byteLength': 12}],
        'accessors': [
            {'bufferView': 0, 'componentType': 5126, 'count': 4, 'type': 'VEC3', 'min': [-size, -0.15, -size], 'max': [0.0, -0.15, 0.0]},
            {'bufferView': 1, 'componentType': 5126, 'count': 4, 'type': 'VEC3'},
            {'bufferView': 2, 'componentType': 5126, 'count': 4, 'type': 'VEC4'},
            {'bufferView': 3, 'componentType': 5126, 'count': 4, 'type': 'VEC2'},
            {'bufferView': 4, 'componentType': 5123, 'count': 6, 'type': 'SCALAR'},
        ],
    }


def pack_texture(path):
    """La ruta de una textura de paquete, relativa a la carpeta de las piezas propias (la exportación la copia)."""
    if not path.is_file():
        raise FileNotFoundError(f'{path}: instala los paquetes con tools/fetch_free_assets.py')
    return Path(os.path.relpath(path, OUT)).as_posix()


def short_street():
    """Street_4Lane (6 m a lo largo de x) escalado a downtown.FILLER: un nodo raíz con la escala sobre el del modelo."""
    import downtown
    source = citykit.find_model('Street_4Lane')
    doc = json.loads(source.read_text(encoding='utf-8'))
    for entry in doc.get('buffers', []) + doc.get('images', []):
        if entry.get('uri') and not entry['uri'].startswith('data:'):
            entry['uri'] = Path(os.path.relpath(source.parent / entry['uri'], OUT)).as_posix()
    roots = doc['scenes'][doc.get('scene', 0)]['nodes']
    doc['nodes'].append({'name': 'Street_4Lane_Short', 'scale': [downtown.FILLER / 6.0, 1.0, 1.0], 'children': roots})
    doc['scenes'][doc.get('scene', 0)]['nodes'] = [len(doc['nodes']) - 1]
    return doc


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    grass_texture().save(OUT / 'Ground_Grass_9x9.png')
    dirt = citykit.CITY / 'Textures'
    nature = citykit.NATURE / 'Textures'
    tiles = {
        'Ground_Grass_9x9': tile_gltf('Ground_Grass_9x9', 9.0, 3.0, 'Ground_Grass_9x9.png'),
        'Ground_Dirt_9x9': tile_gltf('Ground_Dirt_9x9', 9.0, 3.0, pack_texture(dirt / 'T_Dirt_BaseColor.png'),
                                     pack_texture(dirt / 'T_Dirt_Normal.png'), pack_texture(dirt / 'T_Dirt_ORM.png')),
        'Ground_Sand_9x9': tile_gltf('Ground_Sand_9x9', 9.0, 3.0, pack_texture(nature / 'Rocks_Desert_Diffuse.png')),
        'Ground_Path_2x2': tile_gltf('Ground_Path_2x2', 2.0, 1.0, pack_texture(nature / 'PathRocks_Diffuse.png')),
    }
    tiles['Street_4Lane_Short'] = short_street()
    for old in OUT.glob('Ground_Dirt_9x9.png'):
        old.unlink()   # la de ruido de antes: ahora es la del paquete
    for name, gltf in tiles.items():
        (OUT / (name + '.gltf')).write_text(json.dumps(gltf, indent=1), encoding='utf-8')
        print('escrito', OUT / (name + '.gltf'))


if __name__ == '__main__':
    main()
