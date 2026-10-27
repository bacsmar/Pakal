"""Corre dentro de Blender: junta lo fijo de una escena en pocos modelos (uno por cuadro del mapa), para que el juego lo
dibuje con pocas llamadas. Cada cuadro sale como <out>/<nombre>.gltf con una pieza por material; las texturas van a
<out>/textures/, compartidas por todos (el juego carga cada archivo de textura una sola vez).

    blender --background --python blender_batch.py -- --plan <batch_plan.json> --out <carpeta>

El plan (export.py o buildings.py lo escriben): {"chunks": {"<nombre>": [{"path", "x", "y", "z", "yaw", "scale"}, ...]},
"optimize": {"max_triangles": N}, "lightmap_uv": true}. Ejes del nivel (x este, y norte, z arriba), los mismos de Blender; el glTF exportado
queda en los ejes del juego.

La pasada de optimización (si el plan trae "optimize"): los materiales repetidos (cada pieza importada trae su copia) se
vuelven uno, así el modelo se dibuja con una llamada por textura y no por pieza; las caras planas que están en el mismo
plano se juntan (no cambia lo que se ve); si todavía pasa de max_triangles, se simplifica hasta ese número.
"""
import argparse
import json
import math
import sys

import bpy


def args_after_dashes():
    parser = argparse.ArgumentParser()
    parser.add_argument('--plan', required=True)
    parser.add_argument('--out', required=True)
    return parser.parse_args(sys.argv[sys.argv.index('--') + 1:])


def import_model(path):
    """Importa un glTF una vez; sus objetos quedan fuera de la escena (son el molde de las copias)."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    for obj in new:
        for collection in list(obj.users_collection):
            collection.objects.unlink(obj)
    return [o for o in new if o.parent is None or o.parent not in new]


def copy_tree(obj, parent, collection, out):
    duplicate = obj.copy()
    if duplicate.data is not None and duplicate.type == 'MESH':
        duplicate.data = obj.data.copy()   # se va a unir con otras: necesita su propia malla
    collection.objects.link(duplicate)
    duplicate.parent = parent
    out.append(duplicate)
    for child in obj.children:
        copy_tree(child, duplicate, collection, out)


def material_key(material):
    """Lo que hace distinto a un material: sus texturas y sus valores (dos copias importadas dan la misma clave)."""
    if material is None or not material.use_nodes:
        return ('none',)
    images, values = [], []
    for node in material.node_tree.nodes:
        if node.type == 'TEX_IMAGE' and node.image:
            images.append(node.image.filepath or node.image.name)
        elif node.type == 'BSDF_PRINCIPLED':
            for name in ('Base Color', 'Metallic', 'Roughness', 'Alpha'):
                socket = node.inputs.get(name)
                if socket is not None and not socket.is_linked:
                    value = socket.default_value
                    values.append(tuple(round(v, 3) for v in value) if hasattr(value, '__len__') else round(value, 3))
    return (tuple(sorted(images)), tuple(values), material.blend_method)


def optimize(obj, max_triangles):
    """Materiales repetidos en uno, caras coplanares juntas y, si hace falta, menos triángulos."""
    mesh = obj.data
    first = {}
    remap = []
    for index, material in enumerate(mesh.materials):
        key = material_key(material)
        remap.append(first.setdefault(key, index))
    for polygon in mesh.polygons:
        polygon.material_index = remap[polygon.material_index]
    # quita las ranuras que ya nadie usa (de la última a la primera, para no mover los índices pendientes)
    used = {p.material_index for p in mesh.polygons}
    for index in reversed(range(len(mesh.materials))):
        if index not in used:
            obj.active_material_index = index
            bpy.context.view_layer.objects.active = obj
            bpy.ops.object.material_slot_remove()
    before = sum(len(p.vertices) - 2 for p in mesh.polygons)
    planar = obj.modifiers.new('planar', 'DECIMATE')
    planar.decimate_type = 'DISSOLVE'
    planar.angle_limit = math.radians(0.5)
    planar.delimit = {'MATERIAL', 'SEAM', 'UV'}
    bpy.ops.object.modifier_apply(modifier=planar.name)
    after = sum(len(p.vertices) - 2 for p in mesh.polygons)
    if max_triangles and after > max_triangles:
        collapse = obj.modifiers.new('collapse', 'DECIMATE')
        collapse.decimate_type = 'COLLAPSE'
        collapse.ratio = max_triangles / after
        collapse.use_collapse_triangulate = True
        bpy.ops.object.modifier_apply(modifier=collapse.name)
    final = sum(len(p.vertices) - 2 for p in mesh.polygons)
    print('OPTIMIZED', obj.name, 'materials', len(mesh.materials), 'triangles', before, '->', after, '->', final)


def lightmap_uv(obj, out):
    """Un segundo juego de UV para el lightmap (TEXCOORD_1 en el glTF): cada cara en su lugar propio de [0, 1], sin
    encimarse. Las caras del mismo plano quedan juntas (proyección por ángulo), así una pared es una isla y no cien
    triángulos sueltos. Escribe <out>/<nombre>.lm.json con el área del modelo (m²): el atlas le da lugar según ella."""
    mesh = obj.data
    if not mesh.uv_layers:
        mesh.uv_layers.new(name='UVMap')   # el TEXCOORD_0 tiene que seguir siendo el primero
    while len(mesh.uv_layers) > 1:
        mesh.uv_layers.remove(mesh.uv_layers[1])   # otros juegos que traen las piezas (el lightmap tiene que ser el segundo)
    layer = mesh.uv_layers.new(name='Lightmap')
    mesh.uv_layers.active = layer
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.01, area_weight=0.0, correct_aspect=True,
                             scale_to_bounds=False)
    bpy.ops.uv.pack_islands(rotate=True, margin=0.01)
    bpy.ops.object.mode_set(mode='OBJECT')
    for index, uv in enumerate(mesh.uv_layers):
        uv.active_render = index == 0
    mesh.uv_layers.active = mesh.uv_layers[0]
    area = sum(p.area for p in mesh.polygons)
    with open(f'{out}/{obj.name}.lm.json', 'w', encoding='utf-8') as f:
        json.dump({'area': area}, f)
    print('LIGHTMAP_UV', obj.name, 'area', round(area, 1))


def main():
    args = args_after_dashes()
    plan = json.loads(open(args.plan, encoding='utf-8').read())
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    molds = {}
    for name, entities in plan['chunks'].items():
        collection = bpy.data.collections.new(name)
        scene.collection.children.link(collection)
        made = []
        for entity in entities:
            if entity['path'] not in molds:
                molds[entity['path']] = import_model(entity['path'])
            holder = bpy.data.objects.new(f'{name}_holder', None)
            collection.objects.link(holder)
            holder.location = (entity['x'], entity['y'], entity['z'])
            holder.rotation_euler = (0.0, 0.0, math.radians(entity['yaw']))
            holder.scale = (entity['scale'],) * 3
            made.append(holder)
            for root in molds[entity['path']]:
                copy_tree(root, holder, collection, made)
        bpy.context.view_layer.update()
        # las mallas pasan a coordenadas del mundo, sin padres, y se unen en una
        meshes = [o for o in made if o.type == 'MESH']
        for obj in meshes:
            world = obj.matrix_world.copy()
            obj.parent = None
            obj.data.transform(world)
            obj.matrix_world.identity()
        for obj in made:
            if obj.type != 'MESH':
                bpy.data.objects.remove(obj, do_unlink=True)
        if not meshes:
            continue
        bpy.ops.object.select_all(action='DESELECT')
        for obj in meshes:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        bpy.ops.object.join()
        joined = bpy.context.view_layer.objects.active
        joined.name = name
        if 'optimize' in plan:
            optimize(joined, plan['optimize'].get('max_triangles', 0))
        if plan.get('lightmap_uv'):
            lightmap_uv(joined, args.out)
        bpy.ops.object.select_all(action='DESELECT')
        joined.select_set(True)
        bpy.ops.export_scene.gltf(filepath=f'{args.out}/{name}.gltf', export_format='GLTF_SEPARATE', use_selection=True,
                                  export_texture_dir='textures', export_animations=False)
        bpy.data.objects.remove(joined, do_unlink=True)
        print('BATCHED', name, len(entities))
    print('LEVELBUILDER_BATCH_DONE')


main()
