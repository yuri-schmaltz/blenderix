# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Panel


class View3DPanel:
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'


# ********** default tools for object mode ****************


class VIEW3D_PT_tools_object_options(View3DPanel, Panel):
    bl_category = ""  # shown in the header, not in the sidebar
    bl_context = ".objectmode"  # dot on purpose (access from topbar)
    bl_label = "Options"

    def draw(self, _context):
        pass


class VIEW3D_PT_tools_object_options_transform(View3DPanel, Panel):
    bl_category = ""  # shown in the header, not in the sidebar
    bl_context = ".objectmode"  # dot on purpose (access from topbar)
    bl_label = "Transform"
    bl_parent_id = "VIEW3D_PT_tools_object_options"
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings

        col = layout.column(heading="Affect Only", align=True)
        col.prop(tool_settings, "use_transform_data_origin", text="Origins")
        col.prop(tool_settings, "use_transform_pivot_point_align", text="Locations")
        col.prop(tool_settings, "use_transform_skip_children", text="Parents")


# ********** default tools for editmode_mesh ****************


class VIEW3D_PT_tools_meshedit_options(View3DPanel, Panel):
    bl_category = ""  # shown in the header, not in the sidebar
    bl_context = ".mesh_edit"  # dot on purpose (access from topbar)
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 12

    def draw(self, _context):
        pass


class VIEW3D_PT_tools_meshedit_options_transform(View3DPanel, Panel):
    bl_category = ""  # shown in the header, not in the sidebar
    bl_context = ".mesh_edit"  # dot on purpose (access from topbar)
    bl_label = "Transform"
    bl_parent_id = "VIEW3D_PT_tools_meshedit_options"

    @classmethod
    def poll(cls, context):
        return context.active_object

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings
        ob = context.active_object
        mesh = ob.data

        col = layout.column(align=True)
        col.prop(tool_settings, "use_transform_correct_face_attributes")
        sub = col.column(align=True)
        sub.active = tool_settings.use_transform_correct_face_attributes
        sub.prop(tool_settings, "use_transform_correct_keep_connected")
        col.separator()

        col = layout.column(heading="Mirror")
        sub = col.row(align=True)
        sub.prop(mesh, "use_mirror_x", text="X", toggle=True)
        sub.prop(mesh, "use_mirror_y", text="Y", toggle=True)
        sub.prop(mesh, "use_mirror_z", text="Z", toggle=True)

        col = layout.column(align=True)
        col.active = mesh.use_mirror_x or mesh.use_mirror_y or mesh.use_mirror_z
        col.prop(mesh, "use_mirror_topology")
        col.separator()

        col = layout.column(align=True)
        col.prop(tool_settings, "use_mesh_automerge", text="Auto Merge", toggle=False)
        sub = col.column(align=True)
        sub.active = tool_settings.use_mesh_automerge
        sub.prop(tool_settings, "use_mesh_automerge_and_split", toggle=False)
        sub.prop(tool_settings, "double_threshold", text="Threshold")


class VIEW3D_PT_tools_meshedit_options_uvs(View3DPanel, Panel):
    bl_category = ""  # shown in the header, not in the sidebar
    bl_context = ".mesh_edit"  # dot on purpose (access from topbar)
    bl_label = "UVs"
    bl_parent_id = "VIEW3D_PT_tools_meshedit_options"

    def draw(self, context):
        layout = self.layout

        layout.use_property_decorate = False
        layout.use_property_split = True

        tool_settings = context.tool_settings

        layout.prop(tool_settings, "use_edge_path_live_unwrap")


# ********** default tools for editmode_armature ****************


class VIEW3D_PT_tools_armatureedit_options(View3DPanel, Panel):
    bl_category = ""  # shown in the header, not in the sidebar
    bl_context = ".armature_edit"  # dot on purpose (access from topbar)
    bl_label = "Options"

    def draw(self, context):
        arm = context.active_object.data

        self.layout.prop(arm, "use_mirror_x")


classes = (
    VIEW3D_PT_tools_object_options,
    VIEW3D_PT_tools_object_options_transform,
    VIEW3D_PT_tools_meshedit_options,
    VIEW3D_PT_tools_meshedit_options_transform,
    VIEW3D_PT_tools_meshedit_options_uvs,
    VIEW3D_PT_tools_armatureedit_options,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
