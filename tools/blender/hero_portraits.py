"""Portraits of the kit's survivors for the menu's hero cards (examples/zombies_module/ui/images/hero_<id>.png): the head and
shoulders in their idle pose, front on, on a transparent background.

    blender --background --python tools/blender/hero_portraits.py -- --kit <Characters/glTF folder> --out <images folder>
"""
import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

HEROES = ('Sam', 'Lis', 'Matt', 'Shaun')
WEAPONS = {'Axe', 'Guitar', 'Shoulder', 'Knife', 'Pistol', 'Rifle', 'Shotgun', 'SMG', 'Spear', 'WoodenBat_Barbed', 'WoodenBat_Saw'}
SIZE = 256


def args_after_dashes():
    parser = argparse.ArgumentParser()
    parser.add_argument('--kit', required=True)
    parser.add_argument('--out', required=True)
    return parser.parse_args(sys.argv[sys.argv.index('--') + 1:])


def stage():
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in [e.identifier for e in
                                                                            bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items] else 'BLENDER_EEVEE'
    scene.render.resolution_x = scene.render.resolution_y = SIZE
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    scene.view_settings.view_transform = 'Standard'
    world = bpy.data.worlds.new('World')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.8
    scene.world = world
    # a key light from the front left and a rim from behind
    for name, rotation, energy in (('Key', (math.radians(55), 0, math.radians(-35)), 4.0),
                                   ('Rim', (math.radians(60), 0, math.radians(160)), 2.5)):
        light = bpy.data.objects.new(name, bpy.data.lights.new(name, 'SUN'))
        light.data.energy = energy
        light.rotation_euler = rotation
        scene.collection.objects.link(light)
    camera = bpy.data.objects.new('Camera', bpy.data.cameras.new('Camera'))
    camera.data.type = 'ORTHO'
    scene.collection.objects.link(camera)
    scene.camera = camera
    return camera


def portrait(path, camera, out):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    new = [o for o in bpy.data.objects if o not in before]
    weapons = [o for o in new if o.name.split('.')[0] in WEAPONS]
    new = [o for o in new if o not in weapons]
    for obj in weapons:
        bpy.data.objects.remove(obj, do_unlink=True)
    armature = next((o for o in new if o.type == 'ARMATURE'), None)
    if armature and armature.animation_data:
        idle = bpy.data.actions.get('Idle')
        if idle:
            armature.animation_data.action = idle
        bpy.context.scene.frame_set(1)
    # frame the head and shoulders: the top quarter of the body's height, front on (glTF front is Blender -Y)
    bpy.context.view_layer.update()
    meshes = [o for o in new if o.type == 'MESH']
    zs = [(o.matrix_world @ Vector(corner)).z for o in meshes for corner in o.bound_box]
    top, bottom = max(zs), min(zs)
    height = top - bottom
    camera.data.ortho_scale = height * 0.62
    camera.location = (0.0, -6.0, top - height * 0.27)
    camera.rotation_euler = (math.radians(90), 0.0, 0.0)
    bpy.context.scene.render.filepath = str(out)
    bpy.ops.render.render(write_still=True)
    for obj in new:
        if obj.name in bpy.data.objects:
            bpy.data.objects.remove(obj, do_unlink=True)


def main():
    args = args_after_dashes()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    camera = stage()
    out = Path(args.out).resolve()   # Blender reads a relative output path from the .blend's folder
    for hero in HEROES:
        target = out / f'hero_{hero.lower()}.png'
        portrait(Path(args.kit) / f'Characters_{hero}_SingleWeapon.gltf', camera, target)
        print('WROTE', target)


main()
