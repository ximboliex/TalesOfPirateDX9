"""
Export standalone animation data to Tales of Pirate DX9 .lab format.

The .lab file contains only bone animation data (lwAnimDataBone) which can be
loaded independently and applied to any compatible skeleton at runtime.

File structure:
  - DWORD version (EXP_OBJ_VERSION)
  - DWORD bone_num
  - DWORD frame_num
  - DWORD dummy_num
  - DWORD key_type
  - bone_base_info[bone_num]  (name + id + parent_id per bone)
  - inv_bind_matrices[bone_num] (4x4 matrix per bone)
  - keyframes[frame_num * bone_num]
"""
import struct
import bpy

from .lw_types import (
    EXP_OBJ_VERSION,
    pack_bone_info_header, pack_bone_base_info,
    pack_matrix44, pack_quaternion, pack_vector3,
    BONE_KEY_TYPE_MAT43, BONE_KEY_TYPE_MAT44, BONE_KEY_TYPE_QUAT,
    LW_INVALID_INDEX,
)
from .coord_convert import (
    convert_matrix4x4_blender_to_dx, convert_matrix4x3_blender_to_dx,
    convert_quaternion_blender_to_dx, convert_position,
)


def export_lab(context, filepath='', global_scale=1.0,
               frame_start=1, frame_end=250, key_type='MAT43',
               use_all_actions=False, **kwargs):
    """
    Export animation data from the active armature to a .lab file.

    Supports exporting from:
    - The current active action on the armature
    - All actions in the blend file (use_all_actions=True exports the active one,
      future: could export multiple .lab files)
    - Any animation source that drives an armature (BVH imported, FBX imported, etc.)
    """
    # Find the armature
    armature_obj = context.active_object
    if not armature_obj or armature_obj.type != 'ARMATURE':
        # Try to find an armature in selection
        for obj in context.selected_objects:
            if obj.type == 'ARMATURE':
                armature_obj = obj
                break

    if not armature_obj or armature_obj.type != 'ARMATURE':
        return {'CANCELLED'}

    armature = armature_obj.data

    # Collect bone names in a consistent order (depth-first from roots)
    bone_names = []
    _collect_bones_ordered(armature.bones, bone_names)

    if not bone_names:
        return {'CANCELLED'}

    # Map key type string to constant
    key_type_map = {
        'MAT43': BONE_KEY_TYPE_MAT43,
        'MAT44': BONE_KEY_TYPE_MAT44,
        'QUAT': BONE_KEY_TYPE_QUAT,
    }
    key_type_val = key_type_map.get(key_type, BONE_KEY_TYPE_MAT43)

    bone_num = len(bone_names)
    frame_num = frame_end - frame_start + 1
    if frame_num <= 0:
        return {'CANCELLED'}

    # Build bone hierarchy info
    bone_base_data = b''
    for i, bname in enumerate(bone_names):
        bone = armature.bones.get(bname)
        parent_id = LW_INVALID_INDEX
        if bone and bone.parent and bone.parent.name in bone_names:
            parent_id = bone_names.index(bone.parent.name)
        bone_base_data += pack_bone_base_info(bname, i, parent_id)

    # Compute inverse bind-pose matrices
    invmat_data = b''
    for bname in bone_names:
        bone = armature.bones.get(bname)
        if bone:
            inv_mat = bone.matrix_local.inverted()
            mat_flat = convert_matrix4x4_blender_to_dx(inv_mat, global_scale)
            invmat_data += pack_matrix44(mat_flat)
        else:
            invmat_data += pack_matrix44([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1])

    # Sample keyframes
    original_frame = bpy.context.scene.frame_current
    key_data = b''

    for frame in range(frame_start, frame_end + 1):
        bpy.context.scene.frame_set(frame)
        bpy.context.view_layer.update()

        for bname in bone_names:
            pose_bone = armature_obj.pose.bones.get(bname)
            if pose_bone is None:
                if key_type_val == BONE_KEY_TYPE_MAT43:
                    key_data += struct.pack('<12f', 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0)
                elif key_type_val == BONE_KEY_TYPE_MAT44:
                    key_data += pack_matrix44([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1])
                else:
                    key_data += pack_quaternion(0, 0, 0, 1)
                    key_data += pack_vector3(0, 0, 0)
                continue

            mat = pose_bone.matrix

            if key_type_val == BONE_KEY_TYPE_MAT43:
                mat_flat = convert_matrix4x3_blender_to_dx(mat, global_scale)
                key_data += struct.pack('<12f', *mat_flat)
            elif key_type_val == BONE_KEY_TYPE_MAT44:
                mat_flat = convert_matrix4x4_blender_to_dx(mat, global_scale)
                key_data += pack_matrix44(mat_flat)
            else:
                loc, rot, _ = mat.decompose()
                dx_q = convert_quaternion_blender_to_dx(rot.w, rot.x, rot.y, rot.z)
                key_data += pack_quaternion(*dx_q)
                dx_pos = convert_position(loc.x, loc.y, loc.z, global_scale)
                key_data += pack_vector3(*dx_pos)

    # Restore frame
    bpy.context.scene.frame_set(original_frame)

    # Write .lab file
    with open(filepath, 'wb') as f:
        # Version header
        f.write(struct.pack('<I', EXP_OBJ_VERSION))
        # Bone info header (bone_num, frame_num, dummy_num, key_type)
        f.write(pack_bone_info_header(bone_num, frame_num, 0, key_type_val))
        # Bone base info
        f.write(bone_base_data)
        # Inverse bind matrices
        f.write(invmat_data)
        # Keyframe data
        f.write(key_data)

    return {'FINISHED'}


def _collect_bones_ordered(bones, result):
    """Collect bones in depth-first order starting from root bones."""
    roots = [b for b in bones if b.parent is None]
    for root in roots:
        _collect_bone_recursive(root, result)


def _collect_bone_recursive(bone, result):
    result.append(bone.name)
    for child in bone.children:
        _collect_bone_recursive(child, result)
