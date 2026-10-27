"""Copies animation clips from the Universal Animation Library mannequin onto a Zombie Apocalypse Kit character (both CC0,
by Quaternius) and writes the character with its own clips plus the copied ones.

Both rigs rest in a T pose facing -Y, but their bones differ (UE-style names on the mannequin, CharacterArmature on the kit,
which has no hand bones and keeps its feet under the root). Each mapped kit bone takes the world-space turn its mannequin
bone made from the rest pose; the hips' movement is scaled by the two heights; the feet are put at the end of the legs.

    blender --background --python tools/blender/retarget_ual.py -- --target <kit character .gltf> --source <UAL .glb>
        --clips Pistol_Shoot,Pistol_Reload --out <character with clips .glb> [--sheet <frames.png>]

The sources are not modified. --sheet renders a strip of frames of each copied clip for a visual check.
"""
import argparse
import math
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

# mannequin bone -> kit bone, parents before children
BONE_MAP = [
    ('pelvis', 'Body'),
    ('spine_01', 'Hips'), ('spine_02', 'Abdomen'), ('spine_03', 'Torso'), ('neck_01', 'Neck'), ('Head', 'Head'),
    ('clavicle_l', 'Shoulder.L'), ('upperarm_l', 'UpperArm.L'), ('lowerarm_l', 'LowerArm.L'),
    ('middle_01_l', 'Middle1.L'), ('middle_02_l', 'Middle2.L'), ('middle_03_l', 'Middle3.L'),
    ('index_01_l', 'Index1.L'), ('index_02_l', 'Index2.L'), ('index_03_l', 'Index3.L'),
    ('pinky_01_l', 'Pinky1.L'), ('pinky_02_l', 'Pinky2.L'), ('pinky_03_l', 'Pinky3.L'),
    ('thumb_01_l', 'Thumb1.L'), ('thumb_02_l', 'Thumb2.L'),
    ('clavicle_r', 'Shoulder.R'), ('upperarm_r', 'UpperArm.R'), ('lowerarm_r', 'LowerArm.R'),
    ('middle_01_r', 'Middle1.R'), ('middle_02_r', 'Middle2.R'), ('middle_03_r', 'Middle3.R'),
    ('index_01_r', 'Index1.R'), ('index_02_r', 'Index2.R'), ('index_03_r', 'Index3.R'),
    ('pinky_01_r', 'Pinky1.R'), ('pinky_02_r', 'Pinky2.R'), ('pinky_03_r', 'Pinky3.R'),
    ('thumb_01_r', 'Thumb1.R'), ('thumb_02_r', 'Thumb2.R'),
    ('thigh_l', 'UpperLeg.L'), ('calf_l', 'LowerLeg.L'),
    ('thigh_r', 'UpperLeg.R'), ('calf_r', 'LowerLeg.R'),
]
# kit foot -> (mannequin foot, kit lower leg whose tail the foot sits at)
FEET = [('Foot.L', 'foot_l', 'LowerLeg.L'), ('Foot.R', 'foot_r', 'LowerLeg.R')]


def args_after_dashes():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', required=True)
    parser.add_argument('--source', required=True)
    parser.add_argument('--clips', required=True, help='comma separated clip names of the source')
    parser.add_argument('--out', required=True)
    parser.add_argument('--sheet', default='')
    parser.add_argument('--strip', default='', help='comma separated objects of the target to leave out (the weapons a kit '
                                                    'character holds: the game puts its own in the hand)')
    return parser.parse_args(sys.argv[sys.argv.index('--') + 1:])


def import_armature(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    armature = next(o for o in new if o.type == 'ARMATURE')
    return armature, new


def set_action(armature, action):
    data = armature.animation_data or armature.animation_data_create()
    data.action = action
    if hasattr(data, 'action_slot') and getattr(action, 'slots', None):
        data.action_slot = action.slots[0]


def world_rest(armature, bone_name):
    return (armature.matrix_world @ armature.data.bones[bone_name].matrix_local).to_quaternion()


def world_pose(armature, bone_name):
    return (armature.matrix_world @ armature.pose.bones[bone_name].matrix).to_quaternion()


def height(armature, bone_name):
    return (armature.matrix_world @ armature.data.bones[bone_name].head_local).z


def retarget(src, dst, clip):
    action = bpy.data.actions['UAL_' + clip]
    set_action(src, action)
    start, end = (int(round(v)) for v in action.frame_range)
    # a character without some bone (Zombie_Ribcage has no Torso) keeps it as it is
    bones = [(s, d) for s, d in BONE_MAP if d in dst.data.bones]
    feet = [f for f in FEET if f[0] in dst.data.bones and f[2] in dst.data.bones]

    rest_src = {s: world_rest(src, s) for s, _ in bones}
    rest_dst = {d: world_rest(dst, d) for _, d in bones}
    for foot, src_foot, _ in feet:
        rest_src[src_foot] = world_rest(src, src_foot)
        rest_dst[foot] = world_rest(dst, foot)
    scale = height(dst, 'Body') / max(0.001, height(src, 'pelvis'))
    pelvis_rest = (src.matrix_world @ src.data.bones['pelvis'].head_local)
    body_rest = (dst.matrix_world @ dst.data.bones['Body'].head_local)

    new_action = bpy.data.actions.new(clip)
    set_action(dst, new_action)
    dst_inv = dst.matrix_world.inverted()
    for frame in range(start, end + 1):
        bpy.context.scene.frame_set(frame)
        for src_bone, dst_bone in bones:
            turn = world_pose(src, src_bone) @ rest_src[src_bone].inverted()
            rotation = turn @ rest_dst[dst_bone]
            pose = dst.pose.bones[dst_bone]
            head = (dst.matrix_world @ pose.matrix).to_translation()
            if dst_bone == 'Body':
                moved = (src.matrix_world @ src.pose.bones['pelvis'].matrix).to_translation() - pelvis_rest
                head = body_rest + moved * scale
            pose.matrix = dst_inv @ (Matrix.Translation(head) @ rotation.to_matrix().to_4x4())
            bpy.context.view_layer.update()
        for foot, src_foot, leg in feet:
            turn = world_pose(src, src_foot) @ rest_src[src_foot].inverted()
            rotation = turn @ rest_dst[foot]
            tail = dst.matrix_world @ dst.pose.bones[leg].tail
            dst.pose.bones[foot].matrix = dst_inv @ (Matrix.Translation(tail) @ rotation.to_matrix().to_4x4())
            bpy.context.view_layer.update()
        for _, dst_bone in bones:
            pose = dst.pose.bones[dst_bone]
            pose.keyframe_insert('rotation_quaternion', frame=frame)
            if dst_bone == 'Body':
                pose.keyframe_insert('location', frame=frame)
        for foot, _, _ in feet:
            dst.pose.bones[foot].keyframe_insert('rotation_quaternion', frame=frame)
            dst.pose.bones[foot].keyframe_insert('location', frame=frame)
    new_action.use_fake_user = True
    return new_action, start, end


def render_sheet(dst, clips, path):
    """A strip of 6 frames per clip, seen from the front-left, in the workbench renderer."""
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.render.resolution_x, scene.render.resolution_y = 320, 360
    camera_data = bpy.data.cameras.new('SheetCamera')
    camera = bpy.data.objects.new('SheetCamera', camera_data)
    scene.collection.objects.link(camera)
    camera.location = Vector((-1.6, -2.4, 1.0))
    camera.rotation_euler = (math.radians(80), 0, math.radians(-35))
    scene.camera = camera
    frames = []
    for action, start, end in clips:
        set_action(dst, action)
        for i in range(6):
            frame = start + (end - start) * i // 5
            scene.frame_set(frame)
            out = f'{path}.{action.name}.{i}.png'
            scene.render.filepath = out
            bpy.ops.render.render(write_still=True)
            frames.append(out)
    print('SHEET_FRAMES', ';'.join(frames))


def main():
    args = args_after_dashes()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    dst, dst_objects = import_armature(args.target)
    target_actions = set(bpy.data.actions)
    src, src_objects = import_armature(args.source)
    # the mannequin's clips get a prefix, so the copies can keep the clip names
    for action in set(bpy.data.actions) - target_actions:
        action.name = 'UAL_' + action.name
    for obj in src_objects:
        if obj.type == 'MESH':
            obj.hide_render = True
    made = [retarget(src, dst, clip.strip()) for clip in args.clips.split(',') if clip.strip()]
    print('RETARGETED', [a.name for a, _, _ in made])
    if args.sheet:
        render_sheet(dst, made, args.sheet)
    # keep only the character: remove the mannequin before exporting
    for obj in src_objects:
        bpy.data.objects.remove(obj, do_unlink=True)
    for action in [a for a in bpy.data.actions if a.name.startswith('UAL_')]:
        bpy.data.actions.remove(action)
    strip = {name.strip() for name in args.strip.split(',') if name.strip()}
    for obj in [o for o in dst_objects if o.name.split('.')[0] in strip]:
        bpy.data.objects.remove(obj, do_unlink=True)
    set_action(dst, made[0][0] if made else None)
    bpy.ops.export_scene.gltf(filepath=args.out, export_format='GLB', export_animations=True,
                              export_animation_mode='ACTIONS', export_force_sampling=True)
    print('WROTE', args.out)


main()
