bl_info = {
    "name": "Tales of Pirate DX9 Exporter (.lgo/.lmo/.lab)",
    "author": "TalesOfPirateDX9 Community",
    "version": (1, 1, 0),
    "blender": (3, 0, 0),
    "location": "File > Export > Tales of Pirate (.lgo/.lmo/.lab)",
    "description": "Export meshes and animations to Tales of Pirate DX9 binary format",
    "category": "Import-Export",
}

import bpy
from bpy.props import StringProperty, BoolProperty, FloatProperty, EnumProperty, IntProperty
from bpy_extras.io_utils import ExportHelper


class ExportLGO(bpy.types.Operator, ExportHelper):
    """Export to Tales of Pirate DX9 format"""
    bl_idname = "export_scene.lgo"
    bl_label = "Export LGO/LMO"
    bl_options = {'PRESET'}

    filename_ext = ".lgo"

    filter_glob: StringProperty(
        default="*.lgo;*.lmo",
        options={'HIDDEN'},
    )

    file_format: EnumProperty(
        name="Format",
        items=[
            ('LGO', ".lgo", "Static geometry object"),
            ('LMO', ".lmo", "Animated model object"),
        ],
        default='LGO',
    )

    export_mesh: BoolProperty(
        name="Export Mesh",
        default=True,
    )

    export_animation: BoolProperty(
        name="Export Animation",
        default=False,
    )

    global_scale: FloatProperty(
        name="Scale",
        default=1.0,
        min=0.001,
        max=1000.0,
    )

    texture_dir: StringProperty(
        name="Texture Directory",
        description="Relative path prefix for textures (e.g. 'model/character/')",
        default="",
    )

    frame_start: IntProperty(
        name="Frame Start",
        default=1,
        min=0,
    )

    frame_end: IntProperty(
        name="Frame End",
        default=250,
        min=1,
    )

    key_type: EnumProperty(
        name="Key Type",
        items=[
            ('MAT43', "Matrix 4x3", "Matrix 4x3 keyframes"),
            ('MAT44', "Matrix 4x4", "Matrix 4x4 keyframes"),
            ('QUAT', "Quaternion+Position", "Quaternion + Position keyframes"),
        ],
        default='MAT43',
    )

    def execute(self, context):
        from . import export_lgo
        keywords = self.as_keywords(ignore=("filter_glob",))
        return export_lgo.export(context, **keywords)

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "file_format")
        layout.prop(self, "export_mesh")
        layout.prop(self, "export_animation")
        layout.prop(self, "global_scale")
        layout.prop(self, "texture_dir")

        if self.export_animation:
            box = layout.box()
            box.label(text="Animation")
            box.prop(self, "frame_start")
            box.prop(self, "frame_end")
            box.prop(self, "key_type")


class ExportLAB(bpy.types.Operator, ExportHelper):
    """Export animation to Tales of Pirate DX9 .lab format"""
    bl_idname = "export_anim.lab"
    bl_label = "Export LAB Animation"
    bl_options = {'PRESET'}

    filename_ext = ".lab"

    filter_glob: StringProperty(
        default="*.lab",
        options={'HIDDEN'},
    )

    global_scale: FloatProperty(
        name="Scale",
        default=1.0,
        min=0.001,
        max=1000.0,
    )

    frame_start: IntProperty(
        name="Frame Start",
        default=1,
        min=0,
    )

    frame_end: IntProperty(
        name="Frame End",
        default=250,
        min=1,
    )

    key_type: EnumProperty(
        name="Key Type",
        items=[
            ('MAT43', "Matrix 4x3", "Matrix 4x3 keyframes"),
            ('MAT44', "Matrix 4x4", "Matrix 4x4 keyframes"),
            ('QUAT', "Quaternion+Position", "Quaternion + Position keyframes"),
        ],
        default='MAT43',
    )

    use_scene_range: BoolProperty(
        name="Use Scene Frame Range",
        description="Use the scene's start/end frame instead of custom values",
        default=True,
    )

    def execute(self, context):
        from . import export_lab
        if self.use_scene_range:
            self.frame_start = context.scene.frame_start
            self.frame_end = context.scene.frame_end
        keywords = self.as_keywords(ignore=("filter_glob", "use_scene_range"))
        return export_lab.export_lab(context, **keywords)

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "global_scale")
        layout.prop(self, "key_type")
        layout.separator()
        layout.prop(self, "use_scene_range")
        if not self.use_scene_range:
            layout.prop(self, "frame_start")
            layout.prop(self, "frame_end")


def menu_func_export(self, context):
    self.layout.operator(ExportLGO.bl_idname, text="Tales of Pirate (.lgo/.lmo)")
    self.layout.operator(ExportLAB.bl_idname, text="Tales of Pirate Animation (.lab)")


def register():
    bpy.utils.register_class(ExportLGO)
    bpy.utils.register_class(ExportLAB)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)
    bpy.utils.unregister_class(ExportLAB)
    bpy.utils.unregister_class(ExportLGO)


if __name__ == "__main__":
    register()
