"""
Binary struct definitions for the lwExpObj format used by Tales of Pirate DX9 engine.
All values are little-endian. Sizes match the C++ structs exactly.
"""
import struct

# Version constants
EXP_OBJ_VERSION = 0x1005
MTLTEX_VERSION = 0x0002
MESH_VERSION = 0x0001

# Limits
LW_MAX_PATH = 260
LW_MAX_NAME = 64
LW_MAX_TEXTURESTAGE_NUM = 4
LW_MAX_SUBSET_NUM = 16
LW_MAX_BONE_NUM = 25
LW_MAX_MODEL_GEOM_OBJ_NUM = 32
LW_MESH_RS_NUM = 8
LW_MTL_RS_NUM = 8
LW_TEX_TSS_NUM = 8
OBJECT_STATE_NUM = 8

# Object types
MODEL_OBJ_TYPE_GEOMETRY = 1
MODEL_OBJ_TYPE_HELPER = 2

# Geometry object types
GEOMOBJ_TYPE_GENERIC = 0

# Bone key types
BONE_KEY_TYPE_MAT43 = 1
BONE_KEY_TYPE_MAT44 = 2
BONE_KEY_TYPE_QUAT = 3

# D3DFVF flags
D3DFVF_XYZ = 0x002
D3DFVF_NORMAL = 0x010
D3DFVF_DIFFUSE = 0x040
D3DFVF_TEX1 = 0x100
D3DFVF_TEX2 = 0x200
D3DFVF_TEX3 = 0x300
D3DFVF_TEX4 = 0x400
D3DFVF_LASTBETA_UBYTE4 = 0x1000

# D3DPRIMITIVETYPE
D3DPT_TRIANGLELIST = 4

# D3DFORMAT
D3DFMT_UNKNOWN = 0

# D3DPOOL
D3DPOOL_MANAGED = 1
D3DPOOL_FORCE_DWORD = 0x7fffffff

# Texture type
TEX_TYPE_FILE = 0
TEX_TYPE_INVALID = 0xFFFFFFFF

# Colorkey type
COLORKEY_TYPE_NONE = 0

# Transparency type
MTLTEX_TRANSP_FILTER = 0
MTLTEX_TRANSP_ADDITIVE = 1

# Invalid index
LW_INVALID_INDEX = 0xFFFFFFFF

# Helper type
HELPER_TYPE_INVALID = LW_INVALID_INDEX


def pack_render_state_atom(state=LW_INVALID_INDEX, value0=0, value1=0):
    """Pack a single lwRenderStateAtom (12 bytes)."""
    return struct.pack('<III', state, value0, value1)


def pack_render_state_atoms(count):
    """Pack array of default (invalid) lwRenderStateAtom."""
    return pack_render_state_atom() * count


def pack_matrix44_identity():
    """Pack a 4x4 identity matrix (64 bytes), row-major."""
    m = [1, 0, 0, 0,
         0, 1, 0, 0,
         0, 0, 1, 0,
         0, 0, 0, 1]
    return struct.pack('<16f', *m)


def pack_matrix44(mat):
    """Pack a 4x4 matrix from a flat list of 16 floats (row-major)."""
    return struct.pack('<16f', *mat)


def pack_matrix43(mat):
    """Pack a 4x3 matrix from a flat list of 12 floats (row-major, no last column)."""
    return struct.pack('<12f', *mat)


def pack_vector3(x, y, z):
    return struct.pack('<3f', x, y, z)


def pack_vector2(u, v):
    return struct.pack('<2f', u, v)


def pack_quaternion(x, y, z, w):
    """Pack quaternion in XYZW order."""
    return struct.pack('<4f', x, y, z, w)


def pack_color4f(r, g, b, a):
    """Pack lwColorValue4f (16 bytes)."""
    return struct.pack('<4f', r, g, b, a)


def pack_color4b(r, g, b, a):
    """Pack lwColorValue4b as DWORD (4 bytes, ARGB)."""
    return struct.pack('<I', (a << 24) | (r << 16) | (g << 8) | b)


def pack_material(dif=(1, 1, 1, 1), amb=(1, 1, 1, 1), spe=(0, 0, 0, 0), emi=(0, 0, 0, 0), power=0.0):
    """Pack lwMaterial (68 bytes): dif + amb + spe + emi + power."""
    data = b''
    data += pack_color4f(*dif)
    data += pack_color4f(*amb)
    data += pack_color4f(*spe)
    data += pack_color4f(*emi)
    data += struct.pack('<f', power)
    return data


def pack_tex_info(stage=LW_INVALID_INDEX, file_name=""):
    """Pack lwTexInfo (208 bytes on 32-bit)."""
    # stage(4)+level(4)+usage(4)+format(4)+pool(4)+byte_alignment_flag(4)+type(4)+
    # width(4)+height(4)+colorkey_type(4)+colorkey(4)+file_name(64)+data_ptr(4)+tss_set[8](96)
    data = b''
    if stage == LW_INVALID_INDEX:
        tex_type = TEX_TYPE_INVALID
    else:
        tex_type = TEX_TYPE_FILE

    data += struct.pack('<I', stage)          # stage
    data += struct.pack('<I', 0)              # level
    data += struct.pack('<I', 0)              # usage
    data += struct.pack('<i', D3DFMT_UNKNOWN) # format (D3DFORMAT is enum/int)
    data += struct.pack('<I', D3DPOOL_FORCE_DWORD)  # pool
    data += struct.pack('<I', 0)              # byte_alignment_flag
    data += struct.pack('<I', tex_type)       # type
    data += struct.pack('<I', 0)              # width
    data += struct.pack('<I', 0)              # height
    data += struct.pack('<I', COLORKEY_TYPE_NONE)  # colorkey_type
    data += struct.pack('<I', 0)              # colorkey (lwColorValue4b)

    # file_name[LW_MAX_NAME] = 64 bytes
    name_bytes = file_name.encode('ascii', errors='replace')[:LW_MAX_NAME - 1]
    data += name_bytes + b'\x00' * (LW_MAX_NAME - len(name_bytes))

    data += struct.pack('<I', 0)              # data pointer (always 0 on save)

    # tss_set[LW_TEX_TSS_NUM]
    data += pack_render_state_atoms(LW_TEX_TSS_NUM)

    return data


def pack_mtl_tex_info(opacity=1.0, transp_type=MTLTEX_TRANSP_FILTER,
                      material_data=None, rs_set_data=None, tex_infos=None):
    """Pack lwMtlTexInfo."""
    data = b''
    data += struct.pack('<f', opacity)
    data += struct.pack('<I', transp_type)

    if material_data is None:
        material_data = pack_material()
    data += material_data

    if rs_set_data is None:
        rs_set_data = pack_render_state_atoms(LW_MTL_RS_NUM)
    data += rs_set_data

    # tex_seq[4]
    if tex_infos is None:
        for i in range(LW_MAX_TEXTURESTAGE_NUM):
            data += pack_tex_info()
    else:
        for i in range(LW_MAX_TEXTURESTAGE_NUM):
            if i < len(tex_infos):
                data += tex_infos[i]
            else:
                data += pack_tex_info()

    return data


def pack_subset_info(primitive_num, start_index, vertex_num, min_index):
    """Pack lwSubsetInfo (16 bytes)."""
    return struct.pack('<IIII', primitive_num, start_index, vertex_num, min_index)


def pack_blend_info(indices, weights):
    """Pack lwBlendInfo (20 bytes): 4 byte indices as DWORD + 4 floats."""
    idx_bytes = bytes(indices[:4]) + b'\x00' * max(0, 4 - len(indices))
    data = idx_bytes
    w = list(weights[:4]) + [0.0] * max(0, 4 - len(weights))
    data += struct.pack('<4f', *w)
    return data


def pack_render_ctrl_create_info(ctrl_id=LW_INVALID_INDEX, decl_id=LW_INVALID_INDEX,
                                  vs_id=LW_INVALID_INDEX, ps_id=LW_INVALID_INDEX):
    """Pack lwRenderCtrlCreateInfo (16 bytes)."""
    return struct.pack('<IIII', ctrl_id, decl_id, vs_id, ps_id)


def pack_state_ctrl():
    """Pack lwStateCtrl (8 bytes) with default state (visible=1, enable=1)."""
    state_seq = bytearray(OBJECT_STATE_NUM)
    state_seq[0] = 1  # STATE_VISIBLE
    state_seq[1] = 1  # STATE_ENABLE
    return bytes(state_seq)


def pack_geom_obj_info_header(obj_id=0, parent_id=LW_INVALID_INDEX, obj_type=GEOMOBJ_TYPE_GENERIC,
                               mat_local=None, rcci=None, state_ctrl=None,
                               mtl_size=0, mesh_size=0, helper_size=0, anim_size=0):
    """Pack lwGeomObjInfoHeader (116 bytes)."""
    data = b''
    data += struct.pack('<I', obj_id)
    data += struct.pack('<I', parent_id)
    data += struct.pack('<I', obj_type)

    if mat_local is None:
        data += pack_matrix44_identity()
    else:
        data += mat_local

    if rcci is None:
        data += pack_render_ctrl_create_info()
    else:
        data += rcci

    if state_ctrl is None:
        data += pack_state_ctrl()
    else:
        data += state_ctrl

    data += struct.pack('<I', mtl_size)
    data += struct.pack('<I', mesh_size)
    data += struct.pack('<I', helper_size)
    data += struct.pack('<I', anim_size)

    return data


def pack_model_obj_info_header(obj_type, addr, size):
    """Pack lwModelObjInfoHeader (12 bytes)."""
    return struct.pack('<III', obj_type, addr, size)


def pack_mesh_info_header(fvf, vertex_num, index_num, subset_num, bone_index_num,
                           bone_infl_factor=0, vertex_element_num=0, rs_set_data=None):
    """Pack lwMeshInfoHeader."""
    data = b''
    data += struct.pack('<I', fvf)
    data += struct.pack('<I', D3DPT_TRIANGLELIST)
    data += struct.pack('<I', vertex_num)
    data += struct.pack('<I', index_num)
    data += struct.pack('<I', subset_num)
    data += struct.pack('<I', bone_index_num)
    data += struct.pack('<I', bone_infl_factor)
    data += struct.pack('<I', vertex_element_num)

    if rs_set_data is None:
        data += pack_render_state_atoms(LW_MESH_RS_NUM)
    else:
        data += rs_set_data

    return data


def pack_bone_base_info(name, bone_id, parent_id):
    """Pack lwBoneBaseInfo: name[64] + id(4) + parent_id(4) = 72 bytes."""
    name_bytes = name.encode('ascii', errors='replace')[:LW_MAX_NAME - 1]
    data = name_bytes + b'\x00' * (LW_MAX_NAME - len(name_bytes))
    data += struct.pack('<II', bone_id, parent_id)
    return data


def pack_bone_dummy_info(dummy_id, parent_bone_id, mat):
    """Pack lwBoneDummyInfo: id(4) + parent_bone_id(4) + mat(64) = 72 bytes."""
    data = struct.pack('<II', dummy_id, parent_bone_id)
    data += mat
    return data


def pack_bone_info_header(bone_num, frame_num, dummy_num, key_type):
    """Pack lwBoneInfoHeader (16 bytes)."""
    return struct.pack('<IIII', bone_num, frame_num, dummy_num, key_type)
