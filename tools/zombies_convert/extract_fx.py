"""Prepares the shooting effects of a converted Zombies level: the bullet streak, the muzzle flash and the shell casing.

- The muzzle flash uses the original game's textures (the fireball fx_muzzle_01.tga and the glow fx_glow.png) on two
  flat quads. The original model has only a default material once loaded, so the quads are made here.
- The bullet is two crossed quads (one flat, one upright) with a streak texture made here: hot head, orange body, fading
  tail. It is drawn additive, so it shines on any floor and is seen from any side.
- The shell casing is the original model (run ZombiesConvert with --role fx_shell=fx_shellcasing_bronze); it only gets
  a brass color.

Writes into <level>/meshes: fx_bullet.obj/.mtl/.png, fx_muzzle.obj/.mtl, the textures, the .materials.xml files,
and the fx_bullet, fx_muzzle roles in level.json.

Usage: extract_fx.py --level <level.json> --legacy <Imported/Legacy>
"""

import argparse
import json
import math
import os
import shutil
import struct
import sys
import zlib

BULLET_LENGTH = 1.3   # meters, tail to head
BULLET_WIDTH = 0.24
MUZZLE_SIZE = 1.1     # meters, the fireball quad
GLOW_SIZE = 0.7


def write_png(path, width, height, rgba):
    """Minimal PNG writer (8 bit RGBA), so no imaging library is needed."""
    def chunk(tag, data):
        body = tag + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body) & 0xffffffff)
    raw = b''.join(b'\x00' + bytes(rgba[y * width * 4:(y + 1) * width * 4]) for y in range(height))
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(raw, 9)))
        f.write(chunk(b'IEND', b''))


def make_streak(path):
    width, height = 128, 32
    pixels = bytearray()
    for y in range(height):
        across = (y + 0.5) / height * 2.0 - 1.0            # -1..1 over the width of the streak
        side = math.exp(-(across * across) * 3.2)
        for x in range(width):
            u = (x + 0.5) / width                           # 0 tail .. 1 head
            body = u ** 1.6
            head = math.exp(-((1.0 - u) * 9.0) ** 2)        # the hot tip
            level = min(1.0, (body * 0.85 + head) * side)
            r = min(1.0, level * 1.15)
            g = min(1.0, level * (0.55 + 0.45 * head))
            b = min(1.0, level * (0.12 + 0.88 * head))
            pixels += bytes((int(r * 255), int(g * 255), int(b * 255), int(level * 255)))
    write_png(path, width, height, pixels)


def write_materials(path, materials, blend, double_sided):
    with open(path, 'w', encoding='utf-8') as f:
        f.write('<?xml version="1.0" encoding="utf-8"?>\n<materials>\n')
        for name, tint in materials.items():
            f.write('  <material name="%s" blend="%s" doubleSided="%s" tint="%s"/>\n'
                    % (name, blend, 'true' if double_sided else 'false', tint))
        f.write('</materials>\n')


def write_quads(meshes, name, quads, materials):
    """quads: list of (material, [4 corners (x, y, z)], normal). Corners go (u0 v0), (u1 v0), (u1 v1), (u0 v1)."""
    uvs = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
    with open(os.path.join(meshes, name + '.obj'), 'w', encoding='utf-8') as f:
        f.write('mtllib %s.mtl\n' % name)
        for uv in uvs:
            f.write('vt %.4f %.4f\n' % uv)
        base = 0
        for material, corners, normal in quads:
            f.write('usemtl %s\n' % material)
            f.write('vn %.1f %.1f %.1f\n' % normal)
            for c in corners:
                f.write('v %.4f %.4f %.4f\n' % c)
            normal_index = quads.index((material, corners, normal)) + 1
            f.write('f ' + ' '.join('%d/%d/%d' % (base + i + 1, i + 1, normal_index) for i in range(4)) + '\n')
            base += 4
    with open(os.path.join(meshes, name + '.mtl'), 'w', encoding='utf-8') as f:
        for material, texture in materials.items():
            f.write('newmtl %s\nKd 1 1 1\nmap_Kd %s\n' % (material, texture))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--level', required=True)
    parser.add_argument('--legacy', required=True, help='Imported/Legacy folder of the game data')
    args = parser.parse_args()

    level_dir = os.path.dirname(os.path.abspath(args.level))
    meshes = os.path.join(level_dir, 'meshes')
    with open(args.level, encoding='utf-8') as f:
        level = json.load(f)
    roles = level.get('roles', {})

    textures = os.path.join(args.legacy, 'WeaponEffects', 'fx_muzzle_dualglocks_01', 'Textures')
    for name in ('fx_muzzle_01.tga', 'fx_glow.png'):
        shutil.copyfile(os.path.join(textures, name), os.path.join(meshes, name))

    # Bullet: local +Z is the way it flies, the head at the origin and the tail behind it; u runs tail to head
    make_streak(os.path.join(meshes, 'fx_bullet.png'))
    half = BULLET_WIDTH * 0.5
    write_quads(meshes, 'fx_bullet', [
        ('Bullet', [(-half, 0.0, -BULLET_LENGTH), (-half, 0.0, 0.0), (half, 0.0, 0.0), (half, 0.0, -BULLET_LENGTH)], (0.0, 1.0, 0.0)),
        ('Bullet', [(0.0, -half, -BULLET_LENGTH), (0.0, -half, 0.0), (0.0, half, 0.0), (0.0, half, -BULLET_LENGTH)], (1.0, 0.0, 0.0)),
    ], {'Bullet': 'fx_bullet.png'})
    write_materials(os.path.join(meshes, 'fx_bullet.obj.materials.xml'), {'Bullet': '1.0 1.0 1.0 1.0'}, 'additive', True)

    # Muzzle flash: flat quads, the fireball with its flame trailing toward -X, and the glow behind it
    f = MUZZLE_SIZE * 0.5
    g = GLOW_SIZE * 0.5
    # u runs along +X and v along -Z, so the pictures keep their orientation seen from above
    write_quads(meshes, 'fx_muzzle', [
        ('Fireball', [(-f, 0.0, f), (f, 0.0, f), (f, 0.0, -f), (-f, 0.0, -f)], (0.0, 1.0, 0.0)),
        ('Glow', [(-g + 0.15, 0.001, g), (g + 0.15, 0.001, g), (g + 0.15, 0.001, -g), (-g + 0.15, 0.001, -g)], (0.0, 1.0, 0.0)),
    ], {'Fireball': 'fx_muzzle_01.tga', 'Glow': 'fx_glow.png'})
    write_materials(os.path.join(meshes, 'fx_muzzle.obj.materials.xml'),
                    {'Fireball': '1.0 0.9 0.7 1.0', 'Glow': '0.55 0.32 0.12 1.0'}, 'additive', True)

    shell = roles.get('fx_shell')
    if shell:
        # Once loaded the model has one default material: it gets a brass color
        write_materials(os.path.join(level_dir, shell + '.materials.xml'), {'DefaultMaterial': '0.95 0.7 0.25 1.0'}, 'opaque', False)
    else:
        print('no fx_shell role: run ZombiesConvert with --role fx_shell=fx_shellcasing_bronze')

    roles['fx_bullet'] = 'meshes/fx_bullet.obj'
    roles['fx_muzzle'] = 'meshes/fx_muzzle.obj'
    level['roles'] = roles
    with open(args.level, 'w', encoding='utf-8') as out:
        json.dump(level, out, indent=1)
    print('fx: bullet, muzzle and shell (%s) ready in %s' % (shell, meshes))
    return 0


if __name__ == '__main__':
    sys.exit(main())
