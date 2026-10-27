"""Prints the bones of the armature in a glTF/GLB (name, parent, head and tail in rest pose) and its action names.

    blender --background --python tools/blender/dump_rig.py -- <model.gltf|glb>
"""
import sys

import bpy

path = sys.argv[sys.argv.index('--') + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=path)
for obj in bpy.data.objects:
    if obj.type != 'ARMATURE':
        continue
    print(f'ARMATURE {obj.name} bones={len(obj.data.bones)} scale={tuple(round(v, 4) for v in obj.scale)}')
    for bone in obj.data.bones:
        head = obj.matrix_world @ bone.head_local
        tail = obj.matrix_world @ bone.tail_local
        print(f'  BONE {bone.name} parent={bone.parent.name if bone.parent else "-"} '
              f'head=({head.x:.3f},{head.y:.3f},{head.z:.3f}) tail=({tail.x:.3f},{tail.y:.3f},{tail.z:.3f})')
print('ACTIONS', [a.name for a in bpy.data.actions][:80])
