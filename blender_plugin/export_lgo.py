"""
Main export operator for Tales of Pirate DX9 .lgo/.lmo format.
"""
import struct
import bpy

from .lw_types import (
    EXP_OBJ_VERSION, MODEL_OBJ_TYPE_GEOMETRY,
    pack_model_obj_info_header, pack_geom_obj_info_header,
    LW_INVALID_INDEX,
)
from .lw_material import convert_material
from .lw_mesh import convert_mesh
from .lw_anim import convert_animation


def export(context, filepath='', file_format='LGO', export_mesh=True,
           export_animation=False, global_scale=1.0, texture_dir='',
           frame_start=1, frame_end=250, key_type='MAT43', **kwargs):
    """Main export function."""

    # Collect exportable objects
    objects = [obj for obj in context.selected_objects if obj.type == 'MESH']
    if not objects:
        objects = [obj for obj in context.scene.objects if obj.type == 'MESH']

    if not objects:
        return {'CANCELLED'}

    # Change extension based on format
    if file_format == 'LMO' and not filepath.lower().endswith('.lmo'):
        filepath = filepath.rsplit('.', 1)[0] + '.lmo'

    # Build geometry objects
    geom_objects = []

    for obj_idx, bl_obj in enumerate(objects):
        depsgraph = context.evaluated_depsgraph_get()
        eval_obj = bl_obj.evaluated_get(depsgraph)
        bl_mesh = eval_obj.to_mesh()

        if bl_mesh is None:
            continue

        # Triangulate
        import bmesh
        bm = bmesh.new()
        bm.from_mesh(bl_mesh)
        bmesh.ops.triangulate(bm, faces=bm.faces[:])
        bm.to_mesh(bl_mesh)
        bm.free()

        # Recalculate normals for loop triangles
        bl_mesh.calc_loop_triangles()
        bl_mesh.calc_normals_split()

        # Convert materials
        mtl_data = b''
        mat_count = max(1, len(bl_obj.material_slots))
        mtl_entries = []
        for i in range(mat_count):
            mat = bl_obj.material_slots[i].material if i < len(bl_obj.material_slots) else None
            mtl_entries.append(convert_material(mat, texture_dir))

        # Write mtl section: DWORD num + mtl_data for each
        mtl_section = struct.pack('<I', mat_count)
        for entry in mtl_entries:
            mtl_section += entry

        # Convert mesh
        mesh_data = b''
        bone_names = []
        if export_mesh:
            mesh_data, bone_names = convert_mesh(bl_mesh, bl_obj, global_scale)

        # Convert animation
        anim_data = b''
        if export_animation and bone_names:
            armature_obj = bl_obj.parent if (bl_obj.parent and bl_obj.parent.type == 'ARMATURE') else None
            if armature_obj:
                anim_result = convert_animation(
                    armature_obj, bone_names, frame_start, frame_end, key_type, global_scale
                )
                if anim_result:
                    # Wrap in anim_data_info format: just bone animation
                    # DWORD count of anim entries + type(DWORD) + size(DWORD) + data
                    anim_entry_size = len(anim_result)
                    # Simple: write 1 entry (bone anim = type 0)
                    anim_data = struct.pack('<I', 1)  # 1 anim data entry
                    anim_data += struct.pack('<I', 0)  # ANIM_DATA_BONE type
                    anim_data += struct.pack('<I', anim_entry_size)
                    anim_data += anim_result

        eval_obj.to_mesh_clear()

        # Calculate section sizes
        mtl_size = len(mtl_section) if mtl_section else 0
        mesh_size = len(mesh_data) if mesh_data else 0
        helper_size = 0
        anim_size = len(anim_data) if anim_data else 0

        # Build GeomObjInfo header
        header_data = pack_geom_obj_info_header(
            obj_id=obj_idx,
            parent_id=LW_INVALID_INDEX,
            mtl_size=mtl_size,
            mesh_size=mesh_size,
            helper_size=helper_size,
            anim_size=anim_size,
        )

        # Full geometry object data
        geom_data = header_data + mtl_section + mesh_data + anim_data
        geom_objects.append(geom_data)

    if not geom_objects:
        return {'CANCELLED'}

    # Write the file
    with open(filepath, 'wb') as f:
        # Version
        f.write(struct.pack('<I', EXP_OBJ_VERSION))

        # Object count
        obj_num = len(geom_objects)
        f.write(struct.pack('<I', obj_num))

        # Calculate base offset (version + obj_num + headers)
        header_table_size = 4 + 4 + (12 * obj_num)

        # Write object headers
        current_addr = header_table_size
        for geom_data in geom_objects:
            obj_size = len(geom_data)
            f.write(pack_model_obj_info_header(
                MODEL_OBJ_TYPE_GEOMETRY, current_addr, obj_size
            ))
            current_addr += obj_size

        # Write object data
        for geom_data in geom_objects:
            f.write(geom_data)

    return {'FINISHED'}
