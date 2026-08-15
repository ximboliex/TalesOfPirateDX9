"""
Mesh data conversion from Blender to lwMeshInfo binary format.
"""
import struct
from .lw_types import (
    pack_mesh_info_header, pack_subset_info, pack_blend_info, pack_vector3, pack_vector2,
    D3DFVF_XYZ, D3DFVF_NORMAL, D3DFVF_DIFFUSE, D3DFVF_TEX1, D3DFVF_TEX2,
    D3DFVF_LASTBETA_UBYTE4, D3DPT_TRIANGLELIST, LW_MAX_BONE_NUM, LW_MAX_SUBSET_NUM,
)
from .coord_convert import convert_position, convert_normal, convert_uv


def convert_mesh(bl_mesh, bl_obj, scale=1.0):
    """
    Convert a Blender mesh to lwMeshInfo binary data.
    bl_mesh: evaluated mesh (triangulated)
    bl_obj: the object (for vertex groups/armature)
    Returns (bytes, bone_names_list) where bone_names_list is the ordered list of bone names
    used in the mesh, or empty if no skinning.
    """
    bl_mesh.calc_loop_triangles()
    if not bl_mesh.loop_triangles:
        return b'', []

    # Determine material slots
    mat_indices = {}  # mat_index -> list of triangle indices
    for tri in bl_mesh.loop_triangles:
        mi = tri.material_index
        if mi not in mat_indices:
            mat_indices[mi] = []
        mat_indices[mi].append(tri)

    # Build unique vertices per-loop (split by normals/UVs)
    has_normals = True
    uv_layer = bl_mesh.uv_layers.active
    has_uv = uv_layer is not None
    color_layer = bl_mesh.vertex_colors.active if bl_mesh.vertex_colors else None
    has_colors = color_layer is not None

    # Check for skinning
    armature_obj = None
    if bl_obj.parent and bl_obj.parent.type == 'ARMATURE':
        armature_obj = bl_obj.parent
    bone_names = []
    has_skinning = False
    if armature_obj and bl_obj.vertex_groups:
        # Collect bone names from vertex groups that match armature bones
        arm_bone_names = set(armature_obj.data.bones.keys())
        for vg in bl_obj.vertex_groups:
            if vg.name in arm_bone_names:
                bone_names.append(vg.name)
                if len(bone_names) >= LW_MAX_BONE_NUM:
                    break
        has_skinning = len(bone_names) > 0

    # Build vertex data sorted by material (subset)
    vertices = []  # (pos, normal, uv, color, blend_info)
    indices = []
    subsets = []
    vertex_map = {}  # (loop_index) -> vertex index
    bone_name_to_idx = {name: i for i, name in enumerate(bone_names)}

    sorted_materials = sorted(mat_indices.keys())
    global_index_offset = 0

    for mat_idx in sorted_materials:
        tris = mat_indices[mat_idx]
        subset_start_index = len(indices)
        subset_min_index = len(vertices)
        subset_vertex_set = set()

        for tri in tris:
            for loop_idx in tri.loops:
                loop = bl_mesh.loops[loop_idx]
                vert = bl_mesh.vertices[loop.vertex_index]

                # Position
                pos = convert_position(vert.co.x, vert.co.y, vert.co.z, scale)

                # Normal
                norm = convert_normal(loop.normal.x, loop.normal.y, loop.normal.z)

                # UV
                uv = (0.0, 0.0)
                if has_uv:
                    raw_uv = uv_layer.data[loop_idx].uv
                    uv = convert_uv(raw_uv[0], raw_uv[1])

                # Color
                color = 0xFFFFFFFF
                if has_colors:
                    c = color_layer.data[loop_idx].color
                    r = int(min(1.0, c[0]) * 255)
                    g = int(min(1.0, c[1]) * 255)
                    b = int(min(1.0, c[2]) * 255)
                    a = int(min(1.0, c[3]) * 255) if len(c) > 3 else 255
                    color = (a << 24) | (r << 16) | (g << 8) | b

                # Blend info (skinning)
                blend_indices = [0, 0, 0, 0]
                blend_weights = [0.0, 0.0, 0.0, 0.0]
                if has_skinning:
                    groups = []
                    for g in vert.groups:
                        vg_name = bl_obj.vertex_groups[g.group].name
                        if vg_name in bone_name_to_idx:
                            groups.append((bone_name_to_idx[vg_name], g.weight))

                    # Sort by weight descending, take top 4
                    groups.sort(key=lambda x: x[1], reverse=True)
                    groups = groups[:4]

                    # Normalize weights
                    total_w = sum(w for _, w in groups)
                    if total_w > 0:
                        for i, (bi, bw) in enumerate(groups):
                            blend_indices[i] = bi
                            blend_weights[i] = bw / total_w

                # Create vertex key for deduplication
                key = (loop.vertex_index, loop_idx)

                if key not in vertex_map:
                    vertex_map[key] = len(vertices)
                    vertices.append((pos, norm, uv, color, blend_indices, blend_weights))

                idx = vertex_map[key]
                indices.append(idx)
                subset_vertex_set.add(idx)

        subset_index_count = len(indices) - subset_start_index
        subset_vertex_num = len(subset_vertex_set)
        primitive_num = subset_index_count // 3

        if primitive_num > 0:
            subsets.append(pack_subset_info(
                primitive_num=primitive_num,
                start_index=subset_start_index,
                vertex_num=subset_vertex_num,
                min_index=subset_min_index,
            ))

    if not vertices:
        return b'', []

    # Calculate FVF
    fvf = D3DFVF_XYZ
    if has_normals:
        fvf |= D3DFVF_NORMAL
    if has_uv:
        fvf |= D3DFVF_TEX1
    if has_colors:
        fvf |= D3DFVF_DIFFUSE
    if has_skinning:
        fvf |= D3DFVF_LASTBETA_UBYTE4

    vertex_num = len(vertices)
    index_num = len(indices)
    subset_num = len(subsets)
    bone_index_num = len(bone_names) if has_skinning else 0
    bone_infl_factor = 2 if has_skinning else 0

    # Pack mesh header
    data = pack_mesh_info_header(
        fvf=fvf,
        vertex_num=vertex_num,
        index_num=index_num,
        subset_num=subset_num,
        bone_index_num=bone_index_num,
        bone_infl_factor=bone_infl_factor,
        vertex_element_num=0,
    )

    # vertex_element_seq (none)

    # Vertices (positions)
    for v in vertices:
        data += pack_vector3(*v[0])

    # Normals
    if fvf & D3DFVF_NORMAL:
        for v in vertices:
            data += pack_vector3(*v[1])

    # UVs
    if fvf & D3DFVF_TEX1:
        for v in vertices:
            data += pack_vector2(*v[2])

    # Vertex colors
    if fvf & D3DFVF_DIFFUSE:
        for v in vertices:
            data += struct.pack('<I', v[3])

    # Blend info
    if bone_index_num > 0:
        for v in vertices:
            data += pack_blend_info(v[4], v[5])
        # bone_index_seq as DWORDs
        for i in range(bone_index_num):
            data += struct.pack('<I', i)

    # Indices
    for idx in indices:
        data += struct.pack('<I', idx)

    # Subsets
    for s in subsets:
        data += s

    return data, bone_names
