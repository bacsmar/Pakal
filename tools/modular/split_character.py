"""split_character.py: a skinned character cut into parts, for trying modular characters (docs/MODULAR_CHARACTERS.md).

    blender -b -P tools/modular/split_character.py -- <character.glb> <out dir> <prefix>

Writes <prefix>_head.glb, <prefix>_torso.glb and <prefix>_legs.glb into <out dir>: each part is the faces whose vertices
mostly follow the head and neck, the legs and hips, or the rest, skinned to the whole skeleton. The torso and the legs
get a blend shape, "defaultHeavy" (the body pushed out along its normals), to try baking body shapes. No animations are
written: a recipe takes its clips from its skeleton file (the character itself).

The free kits' characters (the Zombie Apocalypse Kit's Lis, Matt, Sam, Shaun) share one skeleton and one atlas, so parts
cut from two of them make a character of both, drawn in one call.
"""
import sys
from pathlib import Path

import bpy
import bmesh

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
if len(args) < 3:
    sys.exit('usage: blender -b -P split_character.py -- <character.glb> <out dir> <prefix>')
source, out_dir, prefix = Path(args[0]), Path(args[1]), args[2]
out_dir.mkdir(parents=True, exist_ok=True)

HEAD = {'Neck', 'Head'}
LEGS = {'Hips', 'UpperLeg.L', 'LowerLeg.L', 'Foot.L', 'UpperLeg.R', 'LowerLeg.R', 'Foot.R'}

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(source))
armature = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
mesh_object = max((o for o in bpy.data.objects if o.type == 'MESH'), key=lambda o: len(o.data.vertices))


def region_of_vertex(vertex, groups):
    best, weight = None, 0.0
    for g in vertex.groups:
        if g.weight > weight:
            best, weight = groups[g.group].name, g.weight
    if best in HEAD:
        return 'head'
    if best in LEGS:
        return 'legs'
    return 'torso'


groups = mesh_object.vertex_groups
regions = [region_of_vertex(v, groups) for v in mesh_object.data.vertices]

parts = {}
for name in ('head', 'torso', 'legs'):
    copy = mesh_object.copy()
    copy.data = mesh_object.data.copy()
    copy.name = f'{prefix}_{name}'
    copy.data.name = copy.name
    bpy.context.collection.objects.link(copy)
    bm = bmesh.new()
    bm.from_mesh(copy.data)
    bm.verts.ensure_lookup_table()
    gone = []
    for face in bm.faces:
        votes = {}
        for v in face.verts:
            votes[regions[v.index]] = votes.get(regions[v.index], 0) + 1
        if max(votes, key=votes.get) != name:
            gone.append(face)
    bmesh.ops.delete(bm, geom=gone, context='FACES')
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context='VERTS')
    bm.to_mesh(copy.data)
    bm.free()
    if name in ('torso', 'legs'):
        copy.shape_key_add(name='Basis')
        heavy = copy.shape_key_add(name='defaultHeavy')
        height = max(mesh_object.dimensions)
        for i, v in enumerate(copy.data.vertices):
            heavy.data[i].co = v.co + v.normal * (0.035 * height)
    parts[name] = copy

bpy.data.objects.remove(mesh_object, do_unlink=True)
for name, part in parts.items():
    bpy.ops.object.select_all(action='DESELECT')
    part.select_set(True)
    armature.select_set(True)
    bpy.context.view_layer.objects.active = part
    path = out_dir / f'{prefix}_{name}.glb'
    bpy.ops.export_scene.gltf(filepath=str(path), use_selection=True, export_animations=False, export_morph=True,
                              export_skins=True, export_apply=False)
    print(f'{path}: {len(part.data.vertices)} vertices')
