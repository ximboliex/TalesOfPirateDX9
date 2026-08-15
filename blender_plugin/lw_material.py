"""
Material and texture conversion from Blender to lwMtlTexInfo format.
"""
import os
from .lw_types import (
    pack_mtl_tex_info, pack_material, pack_tex_info, pack_render_state_atoms,
    LW_MTL_RS_NUM, LW_MAX_TEXTURESTAGE_NUM, MTLTEX_TRANSP_FILTER, TEX_TYPE_FILE,
    LW_INVALID_INDEX,
)


def convert_material(bl_material, texture_dir=""):
    """
    Convert a Blender material to lwMtlTexInfo binary data.
    Returns bytes.
    """
    if bl_material is None:
        return pack_mtl_tex_info()

    # Extract colors
    dif = (1.0, 1.0, 1.0, 1.0)
    amb = (1.0, 1.0, 1.0, 1.0)
    spe = (0.0, 0.0, 0.0, 0.0)
    emi = (0.0, 0.0, 0.0, 0.0)
    power = 0.0
    opacity = 1.0

    if bl_material.use_nodes and bl_material.node_tree:
        bsdf = None
        for node in bl_material.node_tree.nodes:
            if node.type == 'BSDF_PRINCIPLED':
                bsdf = node
                break

        if bsdf:
            # Base Color
            base_color = bsdf.inputs.get('Base Color')
            if base_color and hasattr(base_color, 'default_value'):
                c = base_color.default_value
                dif = (c[0], c[1], c[2], c[3])

            # Specular
            spec_input = bsdf.inputs.get('Specular IOR Level') or bsdf.inputs.get('Specular')
            if spec_input and hasattr(spec_input, 'default_value'):
                s = spec_input.default_value
                if isinstance(s, float):
                    spe = (s, s, s, 1.0)

            # Roughness -> power
            rough_input = bsdf.inputs.get('Roughness')
            if rough_input and hasattr(rough_input, 'default_value'):
                power = max(0.0, (1.0 - rough_input.default_value) * 100.0)

            # Emission
            emit_input = bsdf.inputs.get('Emission Color') or bsdf.inputs.get('Emission')
            if emit_input and hasattr(emit_input, 'default_value'):
                e = emit_input.default_value
                if hasattr(e, '__len__') and len(e) >= 3:
                    emi = (e[0], e[1], e[2], 1.0)

            # Alpha
            alpha_input = bsdf.inputs.get('Alpha')
            if alpha_input and hasattr(alpha_input, 'default_value'):
                opacity = alpha_input.default_value
    else:
        dif = (bl_material.diffuse_color[0], bl_material.diffuse_color[1],
               bl_material.diffuse_color[2], bl_material.diffuse_color[3])
        opacity = bl_material.diffuse_color[3]

    amb = dif  # Use diffuse as ambient fallback

    material_data = pack_material(dif=dif, amb=amb, spe=spe, emi=emi, power=power)
    rs_set_data = pack_render_state_atoms(LW_MTL_RS_NUM)

    # Textures
    tex_infos = []
    tex_files = _get_texture_files(bl_material)

    for i in range(LW_MAX_TEXTURESTAGE_NUM):
        if i < len(tex_files) and tex_files[i]:
            file_path = tex_files[i]
            if texture_dir:
                file_path = texture_dir.rstrip('/\\') + '/' + os.path.basename(file_path)
            tex_infos.append(pack_tex_info(stage=i, file_name=file_path))
        else:
            tex_infos.append(pack_tex_info())

    return pack_mtl_tex_info(
        opacity=opacity,
        transp_type=MTLTEX_TRANSP_FILTER,
        material_data=material_data,
        rs_set_data=rs_set_data,
        tex_infos=tex_infos,
    )


def _get_texture_files(bl_material):
    """Extract texture file paths from a Blender material's node tree."""
    textures = []
    if not bl_material.use_nodes or not bl_material.node_tree:
        return textures

    for node in bl_material.node_tree.nodes:
        if node.type == 'TEX_IMAGE' and node.image:
            filepath = node.image.filepath
            if filepath:
                # Clean up blender path
                filepath = filepath.replace('//', '')
                textures.append(os.path.basename(filepath))

    return textures
