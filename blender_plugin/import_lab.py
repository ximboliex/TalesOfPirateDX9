"""
Import Tales of Pirate DX9 .lab animation files into Blender.
Creates an armature with the bone hierarchy and applies the animation as keyframes.
Can also apply animation to an existing armature.
"""
import struct

import bpy
from mathutils import Matrix, Vector, Quaternion

from .lw_types import (
    EXP_OBJ_VERSION, LW_MAX_NAME, LW_INVALID_INDEX,
    BONE_KEY_TYPE_MAT43, BONE_KEY_TYPE_MAT44, BONE_KEY_TYPE_QUAT,
)
from .coord_convert import (
    convert_position_dx_to_blender, convert_quaternion_dx_to_blender,
    convert_matrix4x4_dx_to_blender, convert_matrix4x3_dx_to_blender,
)


def _read_u32(f):
    return struct.unpack('<I', f.read(4))[0]


def _read_f32(f):
    return struct.unpack('<f', f.read(4))[0]


def _read_string(f, length):
    raw = f.read(length)
    return raw.split(b'\x00', 1)[0].decode('ascii', errors='replace')


def _read_vec3(f):
    return struct.unpack('<3f', f.read(12))


def _read_mat44(f):
    return list(struct.unpack('<16f', f.read(64)))


def _read_mat43(f):
    return list(struct.unpack('<12f', f.read(48)))


def _read_quat(f):
    return struct.unpack('<4f', f.read(16))


def import_lab(context, filepath='', global_scale=1.0,
               apply_to_existing=False, **kwargs):
    """
    Import a .lab animation file.
    If apply_to_existing is True and an armature is selected, applies the
    animation to it. Otherwise creates a new armature.
    """
    with open(filepath, 'rb') as f:
        # Version
        version = _read_u32(f)
        if version != EXP_OBJ_VERSION:
            print(f"Warning: unexpected version 0x{version:X}")

        # BoneInfoHeader: bone_num, frame_num, dummy_num, key_type
        bone_num = _read_u32(f)
        frame_num = _read_u32(f)
        dummy_num = _read_u32(f)
        key_type = _read_u32(f)

        # Read bone base info
        bone_infos = []
        for _ in range(bone_num):
            name = _read_string(f, LW_MAX_NAME)
            bone_id = _read_u32(f)
            parent_id = _read_u32(f)
            bone_infos.append((name, bone_id, parent_id))

        # Read inverse bind matrices
        inv_bind_matrices = []
        for _ in range(bone_num):
            mat = _read_mat44(f)
            inv_bind_matrices.append(mat)

        # Skip dummies
        for _ in range(dummy_num):
            f.read(72)

        # Read keyframes
        keyframes = []
        for frame_idx in range(frame_num):
            frame_keys = []
            for bone_idx in range(bone_num):
                if key_type == BONE_KEY_TYPE_MAT43:
                    mat = _read_mat43(f)
                    frame_keys.append(('MAT43', mat))
                elif key_type == BONE_KEY_TYPE_MAT44:
                    mat = _read_mat44(f)
                    frame_keys.append(('MAT44', mat))
                else:  # QUAT
                    quat = _read_quat(f)
                    pos = _read_vec3(f)
                    frame_keys.append(('QUAT', quat, pos))
            keyframes.append(frame_keys)

    # Find or create armature
    arm_obj = None
    if apply_to_existing:
        if context.active_object and context.active_object.type == 'ARMATURE':
            arm_obj = context.active_object
        else:
            for obj in context.selected_objects:
                if obj.type == 'ARMATURE':
                    arm_obj = obj
                    break

    if arm_obj is None:
        # Create new armature
        arm_obj = _create_armature(context, bone_infos, inv_bind_matrices, global_scale)

    # Apply animation
    _apply_animation(context, arm_obj, bone_infos, keyframes, key_type, global_scale)

    # Set frame range
    context.scene.frame_start = 1
    context.scene.frame_end = frame_num

    return {'FINISHED'}


def _create_armature(context, bone_infos, inv_bind_matrices, scale):
    """Create a new armature from bone data."""
    armature = bpy.data.armatures.new("LAB_Armature")
    arm_obj = bpy.data.objects.new("LAB_Armature", armature)
    context.collection.objects.link(arm_obj)
    context.view_layer.objects.active = arm_obj
    arm_obj.select_set(True)

    bpy.ops.object.mode_set(mode='EDIT')

    edit_bones = []
    for bone_idx, (name, bone_id, parent_id) in enumerate(bone_infos):
        eb = armature.edit_bones.new(name)

        # Compute bind pose from inverse bind matrix
        inv_mat_flat = inv_bind_matrices[bone_idx]
        inv_mat = Matrix([inv_mat_flat[i*4:(i+1)*4] for i in range(4)])
        try:
            bind_mat = inv_mat.inverted()
        except ValueError:
            bind_mat = Matrix.Identity(4)

        bl_mat_rows = convert_matrix4x4_dx_to_blender(
            [bind_mat[r][c] for r in range(4) for c in range(4)], scale)
        bl_mat = Matrix(bl_mat_rows)

        head = bl_mat.translation
        tail = head + (bl_mat.to_3x3() @ Vector((0, 0.1, 0)))
        eb.head = head
        eb.tail = tail
        edit_bones.append(eb)

    # Set parents
    for bone_idx, (name, bone_id, parent_id) in enumerate(bone_infos):
        if parent_id != LW_INVALID_INDEX and parent_id < len(edit_bones):
            edit_bones[bone_idx].parent = edit_bones[parent_id]

    bpy.ops.object.mode_set(mode='OBJECT')
    return arm_obj


def _apply_animation(context, arm_obj, bone_infos, keyframes, key_type, scale):
    """Apply keyframe animation to the armature."""
    if not keyframes:
        return

    arm_obj.animation_data_create()
    action = bpy.data.actions.new(name=arm_obj.name + "_Action")
    arm_obj.animation_data.action = action

    context.view_layer.objects.active = arm_obj
    arm_obj.select_set(True)
    bpy.ops.object.mode_set(mode='POSE')

    # Set rotation mode to quaternion for all pose bones
    for pb in arm_obj.pose.bones:
        pb.rotation_mode = 'QUATERNION'

    for frame_idx, frame_keys in enumerate(keyframes):
        frame_number = frame_idx + 1

        for bone_idx, key_data in enumerate(frame_keys):
            if bone_idx >= len(bone_infos):
                break
            bone_name = bone_infos[bone_idx][0]
            pose_bone = arm_obj.pose.bones.get(bone_name)
            if not pose_bone:
                continue

            if key_data[0] == 'MAT43':
                bl_rows = convert_matrix4x3_dx_to_blender(key_data[1], scale)
                mat = Matrix(bl_rows)
            elif key_data[0] == 'MAT44':
                bl_rows = convert_matrix4x4_dx_to_blender(key_data[1], scale)
                mat = Matrix(bl_rows)
            else:  # QUAT
                qx, qy, qz, qw = key_data[1]
                bw, bx, by, bz = convert_quaternion_dx_to_blender(qx, qy, qz, qw)
                px, py, pz = key_data[2]
                bl_pos = convert_position_dx_to_blender(px, py, pz, scale)
                mat = Quaternion((bw, bx, by, bz)).to_matrix().to_4x4()
                mat.translation = Vector(bl_pos)

            pose_bone.matrix = mat
            pose_bone.keyframe_insert(data_path="location", frame=frame_number)
            pose_bone.keyframe_insert(data_path="rotation_quaternion", frame=frame_number)

    bpy.ops.object.mode_set(mode='OBJECT')
