"""
Animation and bone data conversion from Blender to lwAnimDataBone format.
"""
import struct
from .lw_types import (
    pack_bone_info_header, pack_bone_base_info, pack_bone_dummy_info,
    pack_matrix44, pack_matrix43, pack_quaternion, pack_vector3,
    BONE_KEY_TYPE_MAT43, BONE_KEY_TYPE_MAT44, BONE_KEY_TYPE_QUAT,
    LW_INVALID_INDEX,
)
from .coord_convert import (
    convert_matrix4x4_blender_to_dx, convert_matrix4x3_blender_to_dx,
    convert_quaternion_blender_to_dx, convert_position,
)


def convert_animation(bl_obj, bone_names, frame_start, frame_end, key_type_str='MAT43', scale=1.0):
    """
    Convert Blender armature animation to lwAnimDataBone binary data.
    bl_obj: the armature object
    bone_names: list of bone names (from mesh export, defining bone indices)
    frame_start/frame_end: animation range
    key_type_str: 'MAT43', 'MAT44', or 'QUAT'
    Returns bytes or None if no animation.
    """
    if not bl_obj or bl_obj.type != 'ARMATURE':
        return None

    armature = bl_obj.data
    scene = bl_obj.id_data.users_scene[0] if bl_obj.id_data.users_scene else None

    # Map key type string to constant
    key_type_map = {
        'MAT43': BONE_KEY_TYPE_MAT43,
        'MAT44': BONE_KEY_TYPE_MAT44,
        'QUAT': BONE_KEY_TYPE_QUAT,
    }
    key_type = key_type_map.get(key_type_str, BONE_KEY_TYPE_MAT43)

    bone_num = len(bone_names)
    if bone_num == 0:
        return None

    frame_num = frame_end - frame_start + 1
    if frame_num <= 0:
        return None

    # Build bone hierarchy info
    bone_base_data = b''
    bone_parent_map = {}
    for i, bname in enumerate(bone_names):
        bone = armature.bones.get(bname)
        parent_id = LW_INVALID_INDEX
        if bone and bone.parent and bone.parent.name in bone_names:
            parent_id = bone_names.index(bone.parent.name)
        bone_parent_map[bname] = parent_id
        bone_base_data += pack_bone_base_info(bname, i, parent_id)

    # Compute inverse bind-pose matrices
    invmat_data = b''
    for bname in bone_names:
        bone = armature.bones.get(bname)
        if bone:
            # bone.matrix_local is the bind pose in armature space
            inv_mat = bone.matrix_local.inverted()
            mat_flat = convert_matrix4x4_blender_to_dx(inv_mat, scale)
            invmat_data += pack_matrix44(mat_flat)
        else:
            # Identity
            invmat_data += pack_matrix44([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1])

    # Sample keyframes
    import bpy
    original_frame = bpy.context.scene.frame_current

    key_data = b''
    for frame in range(frame_start, frame_end + 1):
        bpy.context.scene.frame_set(frame)
        bpy.context.view_layer.update()

        for bname in bone_names:
            pose_bone = bl_obj.pose.bones.get(bname)
            if pose_bone is None:
                # Write identity
                if key_type == BONE_KEY_TYPE_MAT43:
                    key_data += struct.pack('<12f', 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0)
                elif key_type == BONE_KEY_TYPE_MAT44:
                    key_data += pack_matrix44([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1])
                else:  # QUAT
                    key_data += pack_quaternion(0, 0, 0, 1)
                    key_data += pack_vector3(0, 0, 0)
                continue

            # Get the bone's matrix in armature space
            mat = pose_bone.matrix

            if key_type == BONE_KEY_TYPE_MAT43:
                mat_flat = convert_matrix4x3_blender_to_dx(mat, scale)
                key_data += struct.pack('<12f', *mat_flat)
            elif key_type == BONE_KEY_TYPE_MAT44:
                mat_flat = convert_matrix4x4_blender_to_dx(mat, scale)
                key_data += pack_matrix44(mat_flat)
            else:  # QUAT
                # Decompose to quaternion and position
                loc, rot, _ = mat.decompose()
                dx_q = convert_quaternion_blender_to_dx(rot.w, rot.x, rot.y, rot.z)
                key_data += pack_quaternion(*dx_q)
                dx_pos = convert_position(loc.x, loc.y, loc.z, scale)
                key_data += pack_vector3(*dx_pos)

    # Restore frame
    bpy.context.scene.frame_set(original_frame)

    # Pack the full lwAnimDataBone
    dummy_num = 0
    data = pack_bone_info_header(bone_num, frame_num, dummy_num, key_type)
    data += bone_base_data
    data += invmat_data
    # dummy_seq (none)
    data += key_data

    return data
