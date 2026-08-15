"""
Import Tales of Pirate DX9 .lgo/.lmo files into Blender.
Reconstructs mesh, materials/textures, armature, and animation.
"""
import os
import struct

import bpy
import bmesh
from mathutils import Matrix, Vector, Quaternion

from .lw_types import (
    EXP_OBJ_VERSION, MODEL_OBJ_TYPE_GEOMETRY,
    LW_MAX_NAME, LW_MAX_TEXTURESTAGE_NUM, LW_MTL_RS_NUM, LW_TEX_TSS_NUM,
    LW_MESH_RS_NUM, LW_INVALID_INDEX,
    D3DFVF_XYZ, D3DFVF_NORMAL, D3DFVF_DIFFUSE, D3DFVF_TEX1,
    D3DFVF_LASTBETA_UBYTE4, OBJECT_STATE_NUM,
    BONE_KEY_TYPE_MAT43, BONE_KEY_TYPE_MAT44, BONE_KEY_TYPE_QUAT,
    TEX_TYPE_FILE, TEX_TYPE_INVALID,
)
from .coord_convert import (
    convert_position_dx_to_blender, convert_normal_dx_to_blender,
    convert_uv_dx_to_blender, convert_quaternion_dx_to_blender,
    convert_matrix4x4_dx_to_blender, convert_matrix4x3_dx_to_blender,
)


def _read_u32(f):
    return struct.unpack('<I', f.read(4))[0]


def _read_i32(f):
    return struct.unpack('<i', f.read(4))[0]


def _read_f32(f):
    return struct.unpack('<f', f.read(4))[0]


def _read_string(f, length):
    raw = f.read(length)
    return raw.split(b'\x00', 1)[0].decode('ascii', errors='replace')


def _read_vec3(f):
    return struct.unpack('<3f', f.read(12))


def _read_vec2(f):
    return struct.unpack('<2f', f.read(8))


def _read_mat44(f):
    return list(struct.unpack('<16f', f.read(64)))


def _read_mat43(f):
    return list(struct.unpack('<12f', f.read(48)))


def _read_quat(f):
    return struct.unpack('<4f', f.read(16))


def import_lgo(context, filepath='', global_scale=1.0, load_textures=True,
               texture_search_dir='', **kwargs):
    """Import a .lgo or .lmo file."""
    with open(filepath, 'rb') as f:
        # Version
        version = _read_u32(f)
        if version != EXP_OBJ_VERSION:
            print(f"Warning: unexpected version 0x{version:X}, expected 0x{EXP_OBJ_VERSION:X}")

        # Object count
        obj_num = _read_u32(f)

        # Read object headers
        obj_headers = []
        for _ in range(obj_num):
            obj_type = _read_u32(f)
            addr = _read_u32(f)
            size = _read_u32(f)
            obj_headers.append((obj_type, addr, size))

        # Process each geometry object
        for obj_idx, (obj_type, addr, size) in enumerate(obj_headers):
            if obj_type != MODEL_OBJ_TYPE_GEOMETRY:
                continue

            f.seek(addr)
            _import_geom_object(context, f, obj_idx, global_scale,
                                load_textures, texture_search_dir, filepath)

    return {'FINISHED'}


def _import_geom_object(context, f, obj_idx, scale, load_textures, texture_search_dir, filepath):
    """Import a single geometry object from the file."""
    # GeomObjInfoHeader (116 bytes)
    obj_id = _read_u32(f)
    parent_id = _read_u32(f)
    obj_type = _read_u32(f)

    # mat_local (4x4 matrix = 64 bytes)
    mat_local = _read_mat44(f)

    # RenderCtrlCreateInfo (16 bytes)
    f.read(16)

    # StateCtrl (8 bytes)
    f.read(OBJECT_STATE_NUM)

    # Section sizes
    mtl_size = _read_u32(f)
    mesh_size = _read_u32(f)
    helper_size = _read_u32(f)
    anim_size = _read_u32(f)

    # Record positions
    mtl_start = f.tell()
    mesh_start = mtl_start + mtl_size
    helper_start = mesh_start + mesh_size
    anim_start = helper_start + helper_size

    # Parse materials
    materials = []
    if mtl_size > 0:
        f.seek(mtl_start)
        materials = _read_materials(f, load_textures, texture_search_dir, filepath)

    # Parse mesh
    mesh_obj = None
    bone_names = []
    bone_indices_map = {}
    if mesh_size > 0:
        f.seek(mesh_start)
        mesh_obj, bone_names, bone_indices_map = _read_mesh(
            context, f, obj_idx, scale, materials)

    # Parse animation
    if anim_size > 0 and mesh_obj and bone_names:
        f.seek(anim_start)
        _read_animation(context, f, mesh_obj, bone_names, scale)

    # Apply local matrix
    if mesh_obj:
        bl_mat = convert_matrix4x4_dx_to_blender(mat_local, scale)
        mesh_obj.matrix_world = Matrix(bl_mat)


def _read_materials(f, load_textures, texture_search_dir, filepath):
    """Read material section. Returns list of Blender materials."""
    mat_count = _read_u32(f)
    materials = []

    for mat_idx in range(mat_count):
        bl_mat = bpy.data.materials.new(name=f"Material_{mat_idx}")
        bl_mat.use_nodes = True
        tree = bl_mat.node_tree
        # Clear default nodes
        for node in tree.nodes:
            tree.nodes.remove(node)

        # Create principled BSDF
        bsdf = tree.nodes.new('ShaderNodeBsdfPrincipled')
        bsdf.location = (0, 0)
        output = tree.nodes.new('ShaderNodeOutputMaterial')
        output.location = (300, 0)
        tree.links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])

        # Read lwMtlTexInfo:
        # opacity (float) + transp_type (DWORD)
        opacity = _read_f32(f)
        transp_type = _read_u32(f)

        # lwMaterial (68 bytes): dif(16) + amb(16) + spe(16) + emi(16) + power(4)
        dif = struct.unpack('<4f', f.read(16))
        amb = struct.unpack('<4f', f.read(16))
        spe = struct.unpack('<4f', f.read(16))
        emi = struct.unpack('<4f', f.read(16))
        power = _read_f32(f)

        # Set BSDF colors
        base_color_input = bsdf.inputs.get('Base Color')
        if base_color_input:
            base_color_input.default_value = dif

        alpha_input = bsdf.inputs.get('Alpha')
        if alpha_input:
            alpha_input.default_value = opacity

        roughness_input = bsdf.inputs.get('Roughness')
        if roughness_input:
            roughness_input.default_value = max(0.0, 1.0 - (power / 100.0))

        # rs_set[LW_MTL_RS_NUM] (12 bytes each)
        f.read(12 * LW_MTL_RS_NUM)

        # tex_seq[4]
        tex_filename = None
        for tex_idx in range(LW_MAX_TEXTURESTAGE_NUM):
            # lwTexInfo: stage(4)+level(4)+usage(4)+format(4)+pool(4)+
            # byte_alignment_flag(4)+type(4)+width(4)+height(4)+
            # colorkey_type(4)+colorkey(4)+file_name(64)+data_ptr(4)+tss_set[8](96)
            stage = _read_u32(f)
            level = _read_u32(f)
            usage = _read_u32(f)
            fmt = _read_i32(f)
            pool = _read_u32(f)
            byte_align = _read_u32(f)
            tex_type = _read_u32(f)
            width = _read_u32(f)
            height = _read_u32(f)
            colorkey_type = _read_u32(f)
            colorkey = _read_u32(f)
            file_name = _read_string(f, LW_MAX_NAME)
            data_ptr = _read_u32(f)
            f.read(12 * LW_TEX_TSS_NUM)  # tss_set

            if tex_idx == 0 and tex_type != TEX_TYPE_INVALID and file_name:
                tex_filename = file_name

        # Try to load texture for viewport preview
        if load_textures and tex_filename:
            _assign_texture(bl_mat, bsdf, tree, tex_filename,
                            texture_search_dir, filepath)

        if opacity < 1.0:
            bl_mat.blend_method = 'BLEND'

        materials.append(bl_mat)

    return materials


def _assign_texture(bl_mat, bsdf, tree, tex_filename, texture_search_dir, filepath):
    """Try to find and assign a texture image to the material for viewport display."""
    # Search paths for the texture
    search_dirs = []
    if texture_search_dir:
        search_dirs.append(texture_search_dir)
    # Look relative to the imported file
    file_dir = os.path.dirname(filepath)
    search_dirs.append(file_dir)
    search_dirs.append(os.path.join(file_dir, 'texture'))
    search_dirs.append(os.path.join(file_dir, 'textures'))
    search_dirs.append(os.path.join(file_dir, '..', 'texture'))
    search_dirs.append(os.path.join(file_dir, '..', 'textures'))

    image = None
    base_name = os.path.basename(tex_filename)

    for search_dir in search_dirs:
        candidate = os.path.join(search_dir, base_name)
        if os.path.isfile(candidate):
            image = bpy.data.images.load(candidate)
            break

    if image is None:
        # Create a placeholder image node anyway with the filename info
        tex_node = tree.nodes.new('ShaderNodeTexImage')
        tex_node.location = (-300, 0)
        tex_node.label = base_name
        tree.links.new(tex_node.outputs['Color'], bsdf.inputs['Base Color'])
        return

    tex_node = tree.nodes.new('ShaderNodeTexImage')
    tex_node.location = (-300, 0)
    tex_node.image = image
    tree.links.new(tex_node.outputs['Color'], bsdf.inputs['Base Color'])

    # Enable viewport display texture
    bl_mat.use_backface_culling = True


def _read_mesh(context, f, obj_idx, scale, materials):
    """Read mesh data and create a Blender mesh object."""
    # lwMeshInfoHeader
    fvf = _read_u32(f)
    prim_type = _read_u32(f)
    vertex_num = _read_u32(f)
    index_num = _read_u32(f)
    subset_num = _read_u32(f)
    bone_index_num = _read_u32(f)
    bone_infl_factor = _read_u32(f)
    vertex_element_num = _read_u32(f)

    # rs_set[LW_MESH_RS_NUM]
    f.read(12 * LW_MESH_RS_NUM)

    # vertex_element_seq (skip)
    if vertex_element_num > 0:
        f.read(vertex_element_num * 8)  # D3DVERTEXELEMENT9 = 8 bytes

    has_normals = bool(fvf & D3DFVF_NORMAL)
    has_uv = bool(fvf & D3DFVF_TEX1)
    has_colors = bool(fvf & D3DFVF_DIFFUSE)
    has_skinning = bool(fvf & D3DFVF_LASTBETA_UBYTE4)

    # Read positions
    positions = []
    for _ in range(vertex_num):
        x, y, z = _read_vec3(f)
        positions.append(convert_position_dx_to_blender(x, y, z, scale))

    # Read normals
    normals = []
    if has_normals:
        for _ in range(vertex_num):
            x, y, z = _read_vec3(f)
            normals.append(convert_normal_dx_to_blender(x, y, z))

    # Read UVs
    uvs = []
    if has_uv:
        for _ in range(vertex_num):
            u, v = _read_vec2(f)
            uvs.append(convert_uv_dx_to_blender(u, v))

    # Read vertex colors
    colors = []
    if has_colors:
        for _ in range(vertex_num):
            argb = _read_u32(f)
            a = ((argb >> 24) & 0xFF) / 255.0
            r = ((argb >> 16) & 0xFF) / 255.0
            g = ((argb >> 8) & 0xFF) / 255.0
            b = (argb & 0xFF) / 255.0
            colors.append((r, g, b, a))

    # Read blend info
    blend_data = []
    bone_names = []
    if bone_index_num > 0:
        for _ in range(vertex_num):
            # lwBlendInfo: 4 byte indices (as DWORD) + 4 floats
            idx_bytes = f.read(4)
            blend_indices = list(idx_bytes)
            blend_weights = list(struct.unpack('<4f', f.read(16)))
            blend_data.append((blend_indices, blend_weights))

        # bone_index_seq
        for _ in range(bone_index_num):
            _read_u32(f)  # just indices 0..N-1

    # Read indices
    indices = []
    for _ in range(index_num):
        indices.append(_read_u32(f))

    # Read subsets
    subsets = []
    for _ in range(subset_num):
        prim_num = _read_u32(f)
        start_index = _read_u32(f)
        vert_num = _read_u32(f)
        min_index = _read_u32(f)
        subsets.append((prim_num, start_index, vert_num, min_index))

    # Create Blender mesh
    mesh_name = f"Mesh_{obj_idx}"
    bl_mesh = bpy.data.meshes.new(mesh_name)

    # Build faces from indices (triangle list)
    faces = []
    for i in range(0, index_num, 3):
        if i + 2 < index_num:
            faces.append((indices[i], indices[i + 1], indices[i + 2]))

    bl_mesh.from_pydata(positions, [], faces)

    # UVs
    if has_uv and uvs:
        uv_layer = bl_mesh.uv_layers.new(name="UVMap")
        for poly in bl_mesh.polygons:
            for li in poly.loop_indices:
                loop = bl_mesh.loops[li]
                vi = loop.vertex_index
                if vi < len(uvs):
                    uv_layer.data[li].uv = uvs[vi]

    # Vertex colors
    if has_colors and colors:
        color_layer = bl_mesh.vertex_colors.new(name="Color")
        for poly in bl_mesh.polygons:
            for li in poly.loop_indices:
                loop = bl_mesh.loops[li]
                vi = loop.vertex_index
                if vi < len(colors):
                    color_layer.data[li].color = colors[vi]

    # Assign materials per subset
    for mat in materials:
        bl_mesh.materials.append(mat)

    # Assign material indices based on subsets
    if subsets:
        for sub_idx, (prim_num, start_index, vert_num, min_index) in enumerate(subsets):
            mat_index = min(sub_idx, len(materials) - 1) if materials else 0
            for i in range(prim_num):
                face_idx = (start_index // 3) + i
                if face_idx < len(bl_mesh.polygons):
                    bl_mesh.polygons[face_idx].material_index = mat_index

    bl_mesh.update()
    bl_mesh.validate()

    # Create object
    obj = bpy.data.objects.new(mesh_name, bl_mesh)
    context.collection.objects.link(obj)
    context.view_layer.objects.active = obj
    obj.select_set(True)

    # Normals (custom split normals)
    if has_normals and normals:
        bl_mesh.use_auto_smooth = True
        loop_normals = []
        for poly in bl_mesh.polygons:
            for li in poly.loop_indices:
                vi = bl_mesh.loops[li].vertex_index
                if vi < len(normals):
                    loop_normals.append(normals[vi])
                else:
                    loop_normals.append((0, 0, 1))
        bl_mesh.normals_split_custom_set(loop_normals)

    return obj, bone_names, {}


def _read_animation(context, f, mesh_obj, bone_names_from_mesh, scale):
    """Read animation section from an .lmo file and create armature + keyframes."""
    # Animation data section: DWORD entry_count
    entry_count = _read_u32(f)
    if entry_count == 0:
        return

    # Read first (bone) animation entry
    anim_type = _read_u32(f)  # 0 = ANIM_DATA_BONE
    anim_size = _read_u32(f)

    if anim_type != 0:
        f.read(anim_size)
        return

    # lwBoneInfoHeader: bone_num, frame_num, dummy_num, key_type
    bone_num = _read_u32(f)
    frame_num = _read_u32(f)
    dummy_num = _read_u32(f)
    key_type = _read_u32(f)

    _create_armature_and_animation(context, f, mesh_obj, bone_num, frame_num,
                                   dummy_num, key_type, scale)


def _create_armature_and_animation(context, f, mesh_obj, bone_num, frame_num,
                                   dummy_num, key_type, scale):
    """Create armature with bones and apply keyframe animation."""
    # Read bone_base_info[bone_num]
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

    # Skip dummy_seq
    for _ in range(dummy_num):
        f.read(72)  # lwBoneDummyInfo

    # Read keyframes
    keyframes = []  # [frame][bone] = matrix or (quat, pos)
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

    # Create armature
    arm_name = mesh_obj.name + "_Armature" if mesh_obj else "Armature"
    armature = bpy.data.armatures.new(arm_name)
    arm_obj = bpy.data.objects.new(arm_name, armature)
    context.collection.objects.link(arm_obj)
    context.view_layer.objects.active = arm_obj
    arm_obj.select_set(True)

    # Enter edit mode to create bones
    bpy.ops.object.mode_set(mode='EDIT')

    edit_bones = []
    for bone_idx, (name, bone_id, parent_id) in enumerate(bone_infos):
        eb = armature.edit_bones.new(name)
        # Compute bind-pose from inverse bind matrix
        inv_mat_flat = inv_bind_matrices[bone_idx]
        inv_mat = Matrix([inv_mat_flat[i*4:(i+1)*4] for i in range(4)])
        # The bind pose is the inverse of the inverse bind matrix
        try:
            bind_mat = inv_mat.inverted()
        except ValueError:
            bind_mat = Matrix.Identity(4)

        # Convert from DX to Blender space
        bl_mat_rows = convert_matrix4x4_dx_to_blender(
            [bind_mat[r][c] for r in range(4) for c in range(4)], scale)
        bl_mat = Matrix(bl_mat_rows)

        # Set bone position
        head = bl_mat.translation
        # Give bone some length along local Y
        tail = head + (bl_mat.to_3x3() @ Vector((0, 0.1, 0)))
        eb.head = head
        eb.tail = tail
        edit_bones.append(eb)

    # Set parents
    for bone_idx, (name, bone_id, parent_id) in enumerate(bone_infos):
        if parent_id != LW_INVALID_INDEX and parent_id < len(edit_bones):
            edit_bones[bone_idx].parent = edit_bones[parent_id]

    bpy.ops.object.mode_set(mode='OBJECT')

    # Parent mesh to armature
    if mesh_obj:
        mesh_obj.parent = arm_obj
        mod = mesh_obj.modifiers.new(name="Armature", type='ARMATURE')
        mod.object = arm_obj

        # Create vertex groups for bones
        for bone_idx, (name, bone_id, parent_id) in enumerate(bone_infos):
            if name not in mesh_obj.vertex_groups:
                mesh_obj.vertex_groups.new(name=name)

    # Apply animation keyframes
    if frame_num > 0 and keyframes:
        arm_obj.animation_data_create()
        action = bpy.data.actions.new(name=arm_name + "_Action")
        arm_obj.animation_data.action = action

        bpy.ops.object.mode_set(mode='POSE')

        for frame_idx, frame_keys in enumerate(keyframes):
            frame_number = frame_idx + 1

            for bone_idx, key_data in enumerate(frame_keys):
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

                # Set pose bone matrix and insert keyframe
                pose_bone.matrix = mat
                pose_bone.keyframe_insert(data_path="location", frame=frame_number)
                pose_bone.keyframe_insert(data_path="rotation_quaternion", frame=frame_number)

        bpy.ops.object.mode_set(mode='OBJECT')

        # Set scene frame range
        context.scene.frame_start = 1
        context.scene.frame_end = frame_num

    return arm_obj
