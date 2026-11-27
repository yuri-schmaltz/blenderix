# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Menu, Panel, UIList
from rna_prop_ui import PropertyPanel

from bpy.app.translations import (
    pgettext_iface as iface_,
    pgettext_tip as rpt_,
)


def _context_mesh(context):
    ob = context.object
    return ob.data if ob is not None and ob.type == 'MESH' else None


class MESH_MT_vertex_group_context_menu(Menu):
    bl_label = "Vertex Group Specials"

    def draw(self, _context):
        layout = self.layout

        layout.operator(
            "object.vertex_group_sort",
            icon='SORTALPHA',
            text="Sort by Name",
        ).sort_type = 'NAME'
        layout.operator(
            "object.vertex_group_sort",
            icon='BONE_DATA',
            text="Sort by Bone Hierarchy",
        ).sort_type = 'BONE_HIERARCHY'
        layout.separator()
        layout.operator("object.vertex_group_copy", icon='DUPLICATE')
        layout.operator("object.vertex_group_copy_to_selected")
        layout.separator()
        layout.operator("object.vertex_group_mirror", icon='ARROW_LEFTRIGHT').use_topology = False
        layout.operator("object.vertex_group_mirror", text="Mirror Vertex Group (Topology)").use_topology = True
        layout.separator()
        layout.operator(
            "object.vertex_group_remove_from",
            icon='X',
            text="Remove from All Groups",
        ).use_all_groups = True
        layout.operator("object.vertex_group_remove_from", text="Clear Active Group").use_all_verts = True
        layout.operator("object.vertex_group_remove", text="Delete All Unlocked Groups").all_unlocked = True
        layout.operator("object.vertex_group_remove", text="Delete All Groups").all = True
        layout.separator()
        props = layout.operator("object.vertex_group_lock", icon='LOCKED', text="Lock All")
        props.action, props.mask = 'LOCK', 'ALL'
        props = layout.operator("object.vertex_group_lock", icon='UNLOCKED', text="Unlock All")
        props.action, props.mask = 'UNLOCK', 'ALL'
        props = layout.operator("object.vertex_group_lock", text="Lock Invert All")
        props.action, props.mask = 'INVERT', 'ALL'


class MESH_MT_color_attribute_context_menu(Menu):
    bl_label = "Color Attribute Specials"

    def draw(self, _context):
        layout = self.layout



class MESH_MT_attribute_context_menu(Menu):
    bl_label = "Attribute Specials"

    def draw(self, _context):
        layout = self.layout



class MESH_UL_vgroups(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data_, _active_propname, _index):
        # assert(isinstance(item, bpy.types.VertexGroup))
        vgroup = item
        layout.prop(vgroup, "name", text="", emboss=False, icon_value=icon)
        icon = 'LOCKED' if vgroup.lock_weight else 'UNLOCKED'
        layout.prop(vgroup, "lock_weight", text="", icon=icon, emboss=False)


class MeshButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        engine = context.engine
        return _context_mesh(context) is not None and (engine in cls.COMPAT_ENGINES)


class DATA_PT_context_mesh(MeshButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout

        ob = context.object
        mesh = _context_mesh(context)
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        elif mesh:
            layout.template_ID(space, "pin_id")


class DATA_PT_vertex_groups(MeshButtonsPanel, Panel):
    bl_label = "Vertex Groups"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    @classmethod
    def poll(cls, context):
        engine = context.engine
        obj = context.object
        return (obj and obj.type in {'MESH', 'LATTICE', 'GREASEPENCIL'} and (engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout

        ob = context.object
        group = ob.vertex_groups.active

        rows = 3
        if group:
            rows = 5

        row = layout.row()
        row.template_list("MESH_UL_vgroups", "", ob, "vertex_groups", ob.vertex_groups, "active_index", rows=rows)

        col = row.column(align=True)

        col.operator("object.vertex_group_add", icon='ADD', text="")
        props = col.operator("object.vertex_group_remove", icon='REMOVE', text="")
        props.all_unlocked = props.all = False

        col.separator()

        col.menu("MESH_MT_vertex_group_context_menu", icon='DOWNARROW_HLT', text="")

        if group:
            col.separator()
            col.operator("object.vertex_group_move", icon='TRIA_UP', text="").direction = 'UP'
            col.operator("object.vertex_group_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

        if (
                ob.vertex_groups and
                (ob.mode == 'EDIT' or
                 (ob.mode == 'WEIGHT_PAINT' and ob.type == 'MESH' and ob.data.use_paint_mask_vertex))
        ):
            row = layout.row()

            sub = row.row(align=True)
            sub.operator("object.vertex_group_assign", text="Assign")
            sub.operator("object.vertex_group_remove_from", text="Remove")

            sub = row.row(align=True)
            sub.operator("object.vertex_group_select", text="Select")
            sub.operator("object.vertex_group_deselect", text="Deselect")

            col = layout.column(align=True)
            col.separator()
            col.use_property_split = True
            col.use_property_decorate = False
            col.prop(context.tool_settings, "vertex_group_weight", text="Weight")
            col.prop(context.tool_settings, "use_auto_normalize", text="Auto Normalize")

        draw_attribute_warnings(context, layout, None)


class DATA_PT_remesh(MeshButtonsPanel, Panel):
    bl_label = "Remesh"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        row = layout.row()

        mesh = _context_mesh(context)
        row.prop(mesh, "remesh_mode", text="Mode", expand=True)
        col = layout.column()
        if mesh.remesh_mode == 'VOXEL':
            col.prop(mesh, "remesh_voxel_size")
            col.prop(mesh, "remesh_voxel_adaptivity")
            col.prop(mesh, "use_remesh_fix_poles")

            col = layout.column(heading="Preserve")
            col.prop(mesh, "use_remesh_preserve_volume", text="Volume")
            col.prop(mesh, "use_remesh_preserve_attributes", text="Attributes")

            col.operator("object.voxel_remesh", text="Voxel Remesh")
        else:
            col.operator("object.quadriflow_remesh", text="QuadriFlow Remesh")


class DATA_PT_customdata(MeshButtonsPanel, Panel):
    bl_label = "Geometry Data"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        me = _context_mesh(context)
        col = layout.column()

        col.operator("mesh.customdata_mask_clear", icon='X')
        col.operator("mesh.customdata_face_sets_clear", icon='X')
        col.operator("mesh.customdata_skin_clear", icon='X')
        col.operator("mesh.reorder_vertices_spatial")
        if me.has_custom_normals:
            col.operator("mesh.customdata_custom_splitnormals_clear", icon='X')
        else:
            col.operator("mesh.customdata_custom_splitnormals_add", icon='ADD')




class DATA_PT_custom_props_mesh(MeshButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }
    _context_path = "object.data"
    _property_type = bpy.types.Mesh


class MESH_UL_attributes(UIList):
    display_domain_names = {
        'POINT': "Vertex",
        'EDGE': "Edge",
        'FACE': "Face",
        'CORNER': "Face Corner",
    }

    def filter_items(self, _context, data, property):
        attributes = getattr(data, property)
        flags = []
        indices = [i for i in range(len(attributes))]

        # Filtering by name
        if self.filter_name:
            flags = bpy.types.UI_UL_list.filter_items_by_name(
                self.filter_name, self.bitflag_filter_item, attributes, "name", reverse=self.use_filter_invert,
            )
        if not flags:
            flags = [self.bitflag_filter_item] * len(attributes)

        # Filtering internal attributes
        for idx, item in enumerate(attributes):
            flags[idx] = self.bitflag_item_never_show if item.is_internal else flags[idx]

        # Reorder by name.
        if self.use_filter_sort_alpha:
            indices = bpy.types.UI_UL_list.sort_items_by_name(attributes, "name")

        return flags, indices

    def draw_item(self, _context, layout, _data, attribute, _icon, _active_data, _active_propname, _index):
        data_type = attribute.bl_rna.properties["data_type"].enum_items[attribute.data_type]

        domain_name = self.display_domain_names.get(attribute.domain, "")

        split = layout.split(factor=0.50)
        split.emboss = 'NONE'
        split.prop(attribute, "name", text="")
        sub = split.row()
        sub.alignment = 'RIGHT'
        sub.active = False
        sub.label(
            text="{:s} - {:s}".format(iface_(domain_name), iface_(data_type.name)),
            translate=False,
        )


class DATA_PT_mesh_attributes(MeshButtonsPanel, Panel):
    bl_label = "Attributes"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        mesh = _context_mesh(context)

        layout = self.layout
        row = layout.row()

        col = row.column()
        col.template_list(
            "MESH_UL_attributes",
            "attributes",
            mesh,
            "attributes",
            mesh.attributes,
            "active_index",
            rows=3,
        )

        col = row.column(align=True)

        col.separator()

        col.menu("MESH_MT_attribute_context_menu", icon='DOWNARROW_HLT', text="")

        draw_attribute_warnings(context, layout, None)


# `attribute` is list of attributes in current UI list
# None for vgroup and mesh. Those are already utilized in comparison.
def draw_attribute_warnings(context, layout, attributes):
    ob = context.object
    mesh = _context_mesh(context)

    if not mesh:
        return

    unique_names = set()
    colliding_names = []
    for collection in (
            mesh.attributes,
            None if ob is None else ob.vertex_groups,
    ):
        if collection is None:
            colliding_names.append("Cannot check for object vertex groups when pinning mesh")
            continue
        for name in collection.keys():
            unique_names_len = len(unique_names)
            unique_names.add(name)
            if (len(unique_names) == unique_names_len):
                if (not attributes or attributes.get(name)):
                    # Print colliding names if they exist in current attribute list, see: !135495
                    colliding_names.append(name)

    if not colliding_names:
        return

    layout.label_multiline(
        text=rpt_("Name collisions: ") +
        ", ".join(
            set(colliding_names)),
        icon='STATUS_WARNING',
        translate=False)


class ColorAttributesListBase:
    display_domain_names = {
        'POINT': "Vertex",
        'EDGE': "Edge",
        'FACE': "Face",
        'CORNER': "Face Corner",
    }

    def filter_items(self, _context, data, property):
        attributes = getattr(data, property)
        flags = []
        indices = [i for i in range(len(attributes))]

        # Filtering by name
        if self.filter_name:
            flags = bpy.types.UI_UL_list.filter_items_by_name(
                self.filter_name, self.bitflag_filter_item, attributes, "name", reverse=self.use_filter_invert,
            )
        if not flags:
            flags = [self.bitflag_filter_item] * len(attributes)

        for idx, item in enumerate(attributes):
            skip = (
                (item.domain not in {'POINT', 'CORNER'}) or
                (item.data_type not in {'FLOAT_COLOR', 'BYTE_COLOR'}) or
                item.is_internal
            )
            flags[idx] = 0 if skip else flags[idx]

        # Reorder by name.
        if self.use_filter_sort_alpha:
            indices = bpy.types.UI_UL_list.sort_items_by_name(attributes, "name")

        return flags, indices


class MESH_UL_color_attributes(UIList, ColorAttributesListBase):
    def draw_item(self, _context, layout, data, attribute, _icon, _active_data, _active_propname, _index):
        data_type = attribute.bl_rna.properties["data_type"].enum_items[attribute.data_type]

        domain_name = self.display_domain_names.get(attribute.domain, "")

        split = layout.split(factor=0.50)
        split.emboss = 'NONE'
        split.prop(attribute, "name", text="", icon='GROUP_VCOL')

        sub = split.row()
        sub.alignment = 'RIGHT'
        sub.active = False
        sub.label(text="{:s} - {:s}".format(iface_(domain_name), iface_(data_type.name)), translate=False)

        active_render = _index == data.color_attributes.render_color_index

        row = layout.row()
        row.emboss = 'NONE'


class MESH_UL_color_attributes_selector(UIList, ColorAttributesListBase):
    def draw_item(self, _context, layout, _data, attribute, _icon, _active_data, _active_propname, _index):
        layout.emboss = 'NONE'
        layout.prop(attribute, "name", text="", icon='GROUP_VCOL')


class DATA_PT_vertex_colors(MeshButtonsPanel, Panel):
    bl_label = "Color Attributes"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        mesh = _context_mesh(context)

        layout = self.layout
        row = layout.row()

        col = row.column()
        col.template_list(
            "MESH_UL_color_attributes",
            "color_attributes",
            mesh,
            "color_attributes",
            mesh.color_attributes,
            "active_color_index",
            rows=3,
        )

        col = row.column(align=True)

        col.separator()

        col.menu("MESH_MT_color_attribute_context_menu", icon='DOWNARROW_HLT', text="")

        draw_attribute_warnings(context, layout, mesh.color_attributes)


classes = (
    MESH_MT_vertex_group_context_menu,
    MESH_MT_color_attribute_context_menu,
    MESH_MT_attribute_context_menu,
    MESH_UL_vgroups,
    MESH_UL_attributes,
    DATA_PT_context_mesh,
    DATA_PT_vertex_groups,
    DATA_PT_vertex_colors,
    DATA_PT_mesh_attributes,
    DATA_PT_remesh,
    DATA_PT_customdata,
    DATA_PT_custom_props_mesh,
    MESH_UL_color_attributes,
    MESH_UL_color_attributes_selector,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
