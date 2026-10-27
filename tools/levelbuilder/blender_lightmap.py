"""Corre dentro de Blender: hornea el lightmap de la base fija de una escena con Cycles (en la GPU si la hay).

    blender --background --python blender_lightmap.py -- --plan <lightmap_plan.json> --out <atlas.png>

Cada modelo que recibe (piso, edificios) trae un segundo juego de UV ('Lightmap', hecho por blender_batch.py) en [0, 1];
aquí cada copia lo mueve a su rectángulo del atlas. Los que solo tapan (árboles, postes) se ponen tal cual. Un plano
grande abajo hace de suelo fuera de la escena.

Tres horneadas:
  - el cielo (directa y rebotes), sin el sol;
  - el rebote del sol (solo indirecta), sin el cielo;
  - la sombra del sol.
El atlas sale en PNG: RGB = raíz de ((cielo + rebote del sol) * normalización * 0.5): el juego lo eleva al cuadrado y lo
duplica (la raíz deja más niveles a lo oscuro, donde el ojo los nota); A = 1 al sol, 0 a la sombra. La normalización deja el piso abierto típico en el ambiente del nivel.

El plan (lightmaps.py lo escribe): {"atlas": N, "samples": N, "sun": {"travel": [x, y, z], "color": [r, g, b]},
"sky": {"top": [...], "horizon": [...]}, "ambient": [r, g, b], "bounds": [x0, y0, x1, y1],
"receivers": [{"path", "x", "y", "z", "yaw", "scale", "uv": [escala, u0, v0], "reference": bool}],
"occluders": [{"path", "x", "y", "z", "yaw", "scale"}]}. Ejes del nivel (x este, y norte, z arriba).
"""
import argparse
import json
import math
import sys
import time

import bpy
import numpy as np
from mathutils import Vector


def args_after_dashes():
    parser = argparse.ArgumentParser()
    parser.add_argument('--plan', required=True)
    parser.add_argument('--out', required=True)
    return parser.parse_args(sys.argv[sys.argv.index('--') + 1:])


def use_gpu(scene):
    scene.render.engine = 'CYCLES'
    prefs = bpy.context.preferences.addons['cycles'].preferences
    for kind in ('OPTIX', 'CUDA', 'HIP', 'ONEAPI', 'METAL'):
        try:
            prefs.compute_device_type = kind
        except TypeError:
            continue
        prefs.get_devices()
        devices = [d for d in prefs.devices if d.type == kind]
        if devices:
            for d in prefs.devices:
                d.use = d.type == kind
            scene.cycles.device = 'GPU'
            print('LIGHTMAP device', kind, ', '.join(d.name for d in devices))
            return
    scene.cycles.device = 'CPU'
    print('LIGHTMAP device CPU')


def import_model(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    for obj in new:
        for collection in list(obj.users_collection):
            collection.objects.unlink(obj)
    return new


def place(molds, entity, collection, own_mesh):
    """Copias de las mallas del modelo, en el mundo y sin padres. own_mesh: cada copia con su malla (se le mueven las UV)."""
    holder = (Vector((entity['x'], entity['y'], entity['z'])), math.radians(entity['yaw']), entity['scale'])
    import mathutils
    placement = (mathutils.Matrix.Translation(holder[0]) @ mathutils.Matrix.Rotation(holder[1], 4, 'Z')
                 @ mathutils.Matrix.Scale(holder[2], 4))
    out = []
    for obj in molds:
        if obj.type != 'MESH':
            continue
        duplicate = obj.copy()
        duplicate.parent = None
        if own_mesh:
            duplicate.data = obj.data.copy()
        duplicate.matrix_world = placement @ obj.matrix_world
        collection.objects.link(duplicate)
        out.append(duplicate)
    return out


def world_sky(scene, top, horizon, strength):
    world = bpy.data.worlds.new('sky')
    scene.world = world
    world.use_nodes = True
    nodes, links = world.node_tree.nodes, world.node_tree.links
    nodes.clear()
    coords = nodes.new('ShaderNodeTexCoord')
    split = nodes.new('ShaderNodeSeparateXYZ')
    clamp = nodes.new('ShaderNodeClamp')
    mix = nodes.new('ShaderNodeMix')
    mix.data_type = 'RGBA'
    background = nodes.new('ShaderNodeBackground')
    output = nodes.new('ShaderNodeOutputWorld')
    links.new(coords.outputs['Generated'], split.inputs[0])
    links.new(split.outputs['Z'], clamp.inputs['Value'])
    links.new(clamp.outputs[0], mix.inputs['Factor'])
    mix.inputs['A'].default_value = (*horizon, 1.0)
    mix.inputs['B'].default_value = (*top, 1.0)
    links.new(mix.outputs['Result'], background.inputs['Color'])
    links.new(background.outputs[0], output.inputs['Surface'])
    background.inputs['Strength'].default_value = strength
    return background


def main():
    args = args_after_dashes()
    plan = json.loads(open(args.plan, encoding='utf-8').read())
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    use_gpu(scene)
    scene.cycles.samples = plan.get('samples', 256)
    scene.cycles.max_bounces = 4
    scene.cycles.diffuse_bounces = 3
    scene.cycles.glossy_bounces = 0
    scene.cycles.transmission_bounces = 2
    scene.cycles.transparent_max_bounces = 8
    collection = bpy.data.collections.new('lightmap')
    scene.collection.children.link(collection)

    molds = {}
    def mold(path):
        if path not in molds:
            molds[path] = import_model(path)
        return molds[path]

    size = plan['atlas']
    atlas = bpy.data.images.new('lightmap', size, size, float_buffer=True, alpha=False)
    receivers = []
    uv_name = None
    for entity in plan['receivers']:
        copies = place(mold(entity['path']), entity, collection, own_mesh=True)
        scale, u0, v0 = entity['uv']
        for obj in copies:
            layers = obj.data.uv_layers
            if len(layers) < 2:
                print('LIGHTMAP sin UV de lightmap:', entity['path'])
                continue
            layers[0].name = 'UVMap'
            layer = layers[1]
            layer.name = uv_name = 'Lightmap'   # los mismos nombres en todos: al juntarlas se empatan por nombre
            data = np.empty(len(layer.data) * 2, dtype=np.float32)
            layer.data.foreach_get('uv', data)
            data[0::2] = u0 + data[0::2] * scale
            data[1::2] = v0 + data[1::2] * scale
            layer.data.foreach_set('uv', data)
            receivers.append(obj)
    for entity in plan['occluders']:
        place(mold(entity['path']), entity, collection, own_mesh=False)
    # los que reciben, en una sola malla: Cycles hornea objeto por objeto y prepara la escena cada vez (con cientos de
    # losetas, la GPU casi no trabajaba)
    count = len(receivers)
    bpy.ops.object.select_all(action='DESELECT')
    for obj in receivers:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = receivers[0]
    bpy.ops.object.join()
    receivers = [bpy.context.view_layer.objects.active]
    print(f'LIGHTMAP scene ready: {count} receiving meshes, {len(molds)} models', flush=True)

    # el suelo de afuera: un plano gris grande, un poco abajo del piso de la escena
    x0, y0, x1, y1 = plan['bounds']
    bpy.ops.mesh.primitive_plane_add(size=1.0, location=((x0 + x1) / 2, (y0 + y1) / 2, -0.2))
    floor = bpy.context.active_object
    floor.scale = (x1 - x0 + 400.0, y1 - y0 + 400.0, 1.0)
    gray = bpy.data.materials.new('outside')
    gray.use_nodes = True
    gray.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.25, 0.25, 0.23, 1.0)
    floor.data.materials.append(gray)
    for c in list(floor.users_collection):
        c.objects.unlink(floor)
    collection.objects.link(floor)

    # cada material de los que reciben hornea al atlas (el nodo de imagen activo, sin conectar)
    for material in {slot.material for obj in receivers for slot in obj.material_slots if slot.material}:
        material.use_nodes = True
        node = material.node_tree.nodes.new('ShaderNodeTexImage')
        node.image = atlas
        node.select = True
        material.node_tree.nodes.active = node

    # luces: el sol con su dirección y color; el cielo con un brillo tal que un piso abierto recibe el ambiente
    sun_data = bpy.data.lights.new('sun', 'SUN')
    sun_data.angle = math.radians(1.5)
    sun = bpy.data.objects.new('sun', sun_data)
    collection.objects.link(sun)
    travel = Vector(plan['sun']['travel']).normalized()
    sun.rotation_euler = travel.to_track_quat('-Z', 'Y').to_euler()
    color = plan['sun']['color']
    strength = max(color)
    sun_data.color = [c / strength for c in color]
    sky = world_sky(scene, plan['sky']['top'], plan['sky']['horizon'], 1.0)
    sky_mean = (np.mean(plan['sky']['top']) + np.mean(plan['sky']['horizon'])) / 2
    sky_strength = float(np.mean(plan['ambient']) / max(sky_mean, 1e-4) / math.pi)

    bpy.ops.object.select_all(action='DESELECT')
    for obj in receivers:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = receivers[0]

    def bake(kind, passes=None):
        start = time.time()
        options = dict(type=kind, uv_layer=uv_name, margin=4, margin_type='EXTEND', use_clear=True, target='IMAGE_TEXTURES')
        if passes:
            options['pass_filter'] = passes
            options['use_selected_to_active'] = False
        bpy.ops.object.bake(**options)
        pixels = np.empty(size * size * 4, dtype=np.float32)
        atlas.pixels.foreach_get(pixels)
        print(f'LIGHTMAP   {kind} {time.time() - start:.0f} s', flush=True)
        return pixels.reshape(size, size, 4)[..., :3].copy()

    sun_data.energy = 0.0
    sky.inputs['Strength'].default_value = sky_strength
    print('LIGHTMAP baking sky...', flush=True)
    sky_light = bake('DIFFUSE', {'DIRECT', 'INDIRECT'})
    sun_data.energy = strength
    sky.inputs['Strength'].default_value = 0.0
    print('LIGHTMAP baking sun bounce...', flush=True)
    bounce = bake('DIFFUSE', {'INDIRECT'})
    print('LIGHTMAP baking sun shadow...', flush=True)
    scene.cycles.samples = max(16, plan.get('samples', 256) // 4)
    shadow = bake('SHADOW')

    # normalización: la mediana del piso abierto (los rectángulos marcados de referencia) queda en el ambiente del nivel
    indirect = sky_light + bounce
    mask = np.zeros((size, size), dtype=bool)
    for entity in plan['receivers']:
        if entity.get('reference'):
            scale, u0, v0 = entity['uv']
            a, b = int(u0 * size), int(v0 * size)
            n = max(1, int(scale * size))
            mask[b:b + n, a:a + n] = True
    level = indirect[mask].mean(axis=1) if mask.any() else indirect.reshape(-1, 3).mean(axis=1)
    level = level[level > 1e-4]
    typical = float(np.median(level)) if len(level) else 1.0
    norm = float(np.mean(plan['ambient'])) / max(typical, 1e-4)
    out = np.empty((size, size, 4), dtype=np.float32)
    out[..., :3] = np.sqrt(np.clip(indirect * norm * 0.5, 0.0, 1.0))
    out[..., 3] = np.clip(shadow.mean(axis=2), 0.0, 1.0)
    result = bpy.data.images.new('result', size, size, alpha=True)
    result.pixels.foreach_set(out.reshape(-1))
    result.filepath_raw = args.out
    result.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    result.save()
    print('LIGHTMAP normalization', round(norm, 3), 'receivers', len(receivers))
    print('LIGHTMAP_DONE')


main()
