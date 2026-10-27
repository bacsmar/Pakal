"""Corre dentro de Blender (blender --background --python blender_scene.py -- ...): arma una escena del constructor de
niveles con los modelos del catálogo, la exporta a .glb (un nodo vacío por entidad, con su id como nombre, y sus mallas
como hijos) y saca dos vistas: cenital ortográfica y en perspectiva desde el suroeste, como la cámara del juego.

    --plan <blender_plan.json>   lo que export.py escribe: límites y entidades (ruta del modelo, x, y, z, giro, escala)
    --top <png> --perspective <png> [--glb <archivo>]

Ejes: los del nivel (x este, y norte, z arriba) son los de Blender; un glTF importado mira a -Y (el sur), como en el juego
con giro 0, y el giro es antihorario visto desde arriba.
"""
import argparse
import json
import math
import sys

import bpy
from mathutils import Vector


def args_after_dashes():
    parser = argparse.ArgumentParser()
    parser.add_argument('--plan', required=True)
    parser.add_argument('--top', required=True)
    parser.add_argument('--perspective', required=True)
    parser.add_argument('--glb', default='')
    return parser.parse_args(sys.argv[sys.argv.index('--') + 1:])


def import_model(path):
    """Importa un glTF una vez y devuelve sus objetos raíz (quedan fuera de la escena: son el molde de las copias)."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    for obj in new:
        for collection in list(obj.users_collection):
            collection.objects.unlink(obj)
    return [o for o in new if o.parent is None or o.parent not in new]


def copy_tree(obj, parent, collection):
    """Copia un objeto y sus hijos compartiendo las mallas (las copias no pesan)."""
    duplicate = obj.copy()
    collection.objects.link(duplicate)
    duplicate.parent = parent
    for child in obj.children:
        copy_tree(child, duplicate, collection)
    return duplicate


def main():
    args = args_after_dashes()
    plan = json.loads(open(args.plan, encoding='utf-8').read())
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    collection = scene.collection
    molds = {}
    for entity in plan['entities']:
        if entity['path'] not in molds:
            molds[entity['path']] = import_model(entity['path'])
        holder = bpy.data.objects.new(entity['id'], None)
        collection.objects.link(holder)
        holder.location = (entity['x'], entity['y'], entity['z'])
        holder.rotation_euler = (0.0, 0.0, math.radians(entity['yaw']))
        holder.scale = (entity['scale'],) * 3
        for root in molds[entity['path']]:
            copy_tree(root, holder, collection)

    if args.glb:
        bpy.ops.export_scene.gltf(filepath=args.glb, export_format='GLB', export_animations=False)

    # vistas: el motor workbench con las texturas de los modelos y una luz de estudio
    x0, y0, x1, y1 = plan['bounds']
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    width, depth = x1 - x0, y1 - y0
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.color_type = 'TEXTURE'
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.world = bpy.data.worlds.new('Sky')
    scene.world.color = (0.55, 0.62, 0.7)

    camera_data = bpy.data.cameras.new('TopCamera')
    camera = bpy.data.objects.new('TopCamera', camera_data)
    collection.objects.link(camera)
    scene.camera = camera
    camera_data.type = 'ORTHO'
    camera_data.ortho_scale = max(width, depth) * 1.04
    camera_data.clip_end = 2000.0
    camera.location = (cx, cy, 300.0)
    camera.rotation_euler = (0.0, 0.0, 0.0)
    scene.render.resolution_x = 1600 if width >= depth else int(1600 * width / depth)
    scene.render.resolution_y = 1600 if depth >= width else int(1600 * depth / width)
    scene.render.filepath = args.top
    bpy.ops.render.render(write_still=True)

    # en perspectiva: desde el suroeste y arriba, mirando al centro (como la cámara del juego, de más lejos)
    camera_data.type = 'PERSP'
    camera_data.lens = 35.0
    span = max(width, depth)
    camera.location = (cx - span * 0.55, cy - span * 0.6, span * 0.7)
    direction = Vector((cx, cy, 0.0)) - camera.location
    camera.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
    scene.render.resolution_x, scene.render.resolution_y = 1600, 900
    scene.render.filepath = args.perspective
    bpy.ops.render.render(write_still=True)
    print('LEVELBUILDER_DONE')


main()
