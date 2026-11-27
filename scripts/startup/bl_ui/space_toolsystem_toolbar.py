# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# For documentation on tool definitions: see "bl_ui.space_toolsystem_common.ToolDef"
# where there are comments for each field and their use.

# For now group all tools together
# we may want to move these into per space-type files.
#
# For now keep this in a single file since it's an area that may change,
# so avoid making changes all over the place.

import bpy

from collections import (
    namedtuple
)

from bpy.types import (
    Panel,
)
from bpy.app.translations import (
    pgettext_iface as iface_,
    pgettext_tip as tip_,
    contexts as i18n_contexts,
)

from bl_ui.space_toolsystem_common import (
    ToolSelectPanelHelper,
    ToolDef,
)


def kmi_to_string_or_none(kmi):
    return kmi.to_string() if kmi else "<none>"


def generate_from_enum_ex(
        _context, *,
        idname_prefix,
        icon_prefix,
        type,
        attr,
        options,
        cursor='DEFAULT',
        tooldef_keywords=None,
        icon_map=None,
        use_separators=True,
):
    if tooldef_keywords is None:
        tooldef_keywords = {}

    tool_defs = []

    enum_items = getattr(
        type.bl_rna.properties[attr],
        "enum_items_static_ui" if use_separators else
        "enum_items_static",
    )

    for enum in enum_items:
        if use_separators:
            if not (name := enum.name):
                # Empty string for a UI Separator.
                tool_defs.append(None)
                continue
            if not (idname := enum.identifier):
                # This is a heading, there is no purpose in showing headings here.
                continue
        else:
            name = enum.name
            idname = enum.identifier

        icon = icon_prefix + idname.lower()
        if icon_map is not None:
            icon = icon_map.get(icon, icon)

        tool_defs.append(
            ToolDef.from_dict(
                dict(
                    idname=idname_prefix + name,
                    label=name,
                    description=enum.description,
                    icon=icon,
                    cursor=cursor,
                    data_block=idname,
                    options=options,
                    **tooldef_keywords,
                ),
            ),
        )
    return tuple(tool_defs)


# Use for shared widget data.
class _template_widget:
    class VIEW3D_GGT_xform_extrude:
        @staticmethod
        def draw_settings(_context, layout, tool):
            props = tool.gizmo_group_properties("VIEW3D_GGT_xform_extrude")
            row = layout.row(align=True)
            row.prop(props, "axis_type", expand=True)

    class VIEW3D_GGT_xform_gizmo:
        @staticmethod
        def draw_settings_with_index(context, layout, index):
            scene = context.scene
            orient_slot = scene.transform_orientation_slots[index]
            layout.prop(orient_slot, "type")


class _defs_view3d_generic:
    @ToolDef.from_fn
    def cursor():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("view3d.cursor3d")
            layout.prop(props, "use_depth")
            layout.prop(props, "orientation")
        return dict(
            idname="builtin.cursor",
            label="Cursor",
            description=(
                "Set the cursor location, drag to transform"
            ),
            icon="ops.generic.cursor",
            keymap="3D View Tool: Cursor",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def cursor_click():
        return dict(
            idname="builtin.none",
            label="None",
            icon="ops.generic.cursor",
            keymap=(),
        )

    @ToolDef.from_fn
    def ruler():
        def description(_context, _item, km):
            if km is not None:
                kmi_add = km.keymap_items.find_from_operator("view3d.ruler_add")
                kmi_remove = km.keymap_items.find_from_operator("view3d.ruler_remove")
            else:
                kmi_add = None
                kmi_remove = None
            return tip_(
                "Measure distance and angles\n"
                " \u2022 {:s} anywhere for new measurement\n"
                " \u2022 Drag ruler segment to measure an angle\n"
                " \u2022 {:s} to remove the active ruler\n"
                " \u2022 Ctrl while dragging to snap\n"
                " \u2022 Shift while dragging to measure surface thickness"
            ).format(
                kmi_to_string_or_none(kmi_add),
                kmi_to_string_or_none(kmi_remove),
            )
        return dict(
            idname="builtin.measure",
            label="Measure",
            description=description,
            icon="ops.view3d.ruler",
            widget="VIEW3D_GGT_ruler",
            keymap="3D View Tool: Measure",
        )


class _defs_annotate:

    def draw_settings_common(context, layout, tool):
        gpd = context.annotation_data
        region_type = context.region.type

        if gpd is not None:
            if gpd.layers.active_note is not None:
                text = gpd.layers.active_note
                maxw = 25
                if len(text) > maxw:
                    text = text[:maxw - 5] + '..' + text[-3:]
            else:
                text = ""

            gpl = context.active_annotation_layer
            if gpl is not None:
                if context.space_data.type in {'VIEW_3D', 'SEQUENCE_EDITOR', 'IMAGE_EDITOR', 'NODE_EDITOR'}:
                    layout.label(text="Annotation:")
                    if region_type == 'TOOL_HEADER':
                        sub = layout.split(align=True, factor=0.5)
                        sub.ui_units_x = 6.5
                        sub.prop(gpl, "color", text="")
                    else:
                        sub = layout.row(align=True)
                        sub.prop(gpl, "color", text="")
                    sub.popover(
                        panel="TOPBAR_PT_annotation_layers",
                        text=text,
                    )
                elif context.space_data.type == 'PROPERTIES':
                    row = layout.row(align=True)
                    row.prop(gpl, "color", text="Annotation")
                    row.popover(
                        panel="TOPBAR_PT_annotation_layers",
                        text=text,
                    )
                else:
                    layout.label(text="Annotation:")
                    layout.prop(gpl, "color", text="")

        space_type = tool.space_type
        tool_settings = context.tool_settings

        if space_type == 'VIEW_3D':
            if region_type == 'TOOL_HEADER':
                row = layout.row(align=True)
            else:
                row = layout.row().column(align=True)
            row.prop(tool_settings, "annotation_stroke_placement_view3d", text="Placement")
            if tool_settings.annotation_stroke_placement_view3d in {'SURFACE', 'STROKE'}:
                row.prop(tool_settings, "use_annotation_stroke_endpoints")
                row.prop(tool_settings, "use_annotation_project_only_selected")

        elif space_type in {'IMAGE_EDITOR', 'NODE_EDITOR', 'SEQUENCE_EDITOR', 'CLIP_EDITOR'}:
            row = layout.row(align=True)
            row.prop(tool_settings, "annotation_stroke_placement_view2d", text="Placement")

        if tool.idname == "builtin.annotate_line":
            props = tool.operator_properties("gpencil.annotate")
            if region_type == 'TOOL_HEADER':
                row = layout.row()
                row.ui_units_x = 15
                row.prop(props, "arrowstyle_start", text="Start")
                row.separator()
                row.prop(props, "arrowstyle_end", text="End", text_ctxt=i18n_contexts.id_curve)
            else:
                col = layout.row().column(align=True)
                col.prop(props, "arrowstyle_start", text="Style Start")
                col.prop(props, "arrowstyle_end", text="End")
        elif tool.idname == "builtin.annotate":
            props = tool.operator_properties("gpencil.annotate")
            if region_type == 'TOOL_HEADER':
                row = layout.row()
                row.prop(props, "use_stabilizer", text="Stabilize Stroke")
                subrow = layout.row(align=False)
                subrow.active = props.use_stabilizer
                subrow.prop(props, "stabilizer_radius", text="Radius", slider=True)
                subrow.prop(props, "stabilizer_factor", text="Factor", slider=True)
            else:
                layout.prop(props, "use_stabilizer", text="Stabilize Stroke")
                col = layout.column(align=False)
                col.active = props.use_stabilizer
                col.prop(props, "stabilizer_radius", text="Radius", slider=True)
                col.prop(props, "stabilizer_factor", text="Factor", slider=True)

    @ToolDef.from_fn.with_args(draw_settings=draw_settings_common)
    def scribble(*, draw_settings):
        return dict(
            idname="builtin.annotate",
            label="Annotate",
            icon="ops.gpencil.draw",
            cursor='PAINT_BRUSH',
            keymap="Generic Tool: Annotate",
            draw_settings=draw_settings,
            options={'KEYMAP_FALLBACK'},
        )

    @ToolDef.from_fn.with_args(draw_settings=draw_settings_common)
    def line(*, draw_settings):
        return dict(
            idname="builtin.annotate_line",
            label="Annotate Line",
            icon="ops.gpencil.draw.line",
            cursor='PAINT_BRUSH',
            keymap="Generic Tool: Annotate Line",
            draw_settings=draw_settings,
            options={'KEYMAP_FALLBACK'},
        )

    @ToolDef.from_fn.with_args(draw_settings=draw_settings_common)
    def poly(*, draw_settings):
        return dict(
            idname="builtin.annotate_polygon",
            label="Annotate Polygon",
            icon="ops.gpencil.draw.poly",
            cursor='PAINT_BRUSH',
            keymap="Generic Tool: Annotate Polygon",
            draw_settings=draw_settings,
            options={'KEYMAP_FALLBACK'},
        )

    @ToolDef.from_fn
    def eraser():
        def draw_settings(context, layout, _tool):
            # TODO: Move this setting to tool_settings
            prefs = context.preferences
            layout.prop(prefs.edit, "grease_pencil_eraser_radius", text="Radius")
        return dict(
            idname="builtin.annotate_eraser",
            label="Annotate Eraser",
            icon="ops.gpencil.draw.eraser",
            cursor='ERASER',
            keymap="Generic Tool: Annotate Eraser",
            draw_settings=draw_settings,
            options={'KEYMAP_FALLBACK'},
        )


class _defs_transform:

    def draw_transform_sculpt_tool_settings(context, layout):
        if context.mode != 'SCULPT':
            return
        layout.prop(context.tool_settings.sculpt, "transform_mode")

    @ToolDef.from_fn
    def translate():
        def draw_settings(context, layout, _tool):
            _defs_transform.draw_transform_sculpt_tool_settings(context, layout)
            _template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index(context, layout, 1)
        return dict(
            idname="builtin.move",
            label="Move",
            # cursor='SCROLL_XY',
            icon="ops.transform.translate",
            widget="VIEW3D_GGT_xform_gizmo",
            operator="transform.translate",
            keymap="3D View Tool: Move",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def rotate():
        def draw_settings(context, layout, _tool):
            _defs_transform.draw_transform_sculpt_tool_settings(context, layout)
            _template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index(context, layout, 2)
        return dict(
            idname="builtin.rotate",
            label="Rotate",
            # cursor='SCROLL_XY',
            icon="ops.transform.rotate",
            widget="VIEW3D_GGT_xform_gizmo",
            operator="transform.rotate",
            keymap="3D View Tool: Rotate",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def scale():
        def draw_settings(context, layout, _tool):
            _defs_transform.draw_transform_sculpt_tool_settings(context, layout)
            _template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index(context, layout, 3)
        return dict(
            idname="builtin.scale",
            label="Scale",
            # cursor='SCROLL_XY',
            icon="ops.transform.resize",
            widget="VIEW3D_GGT_xform_gizmo",
            operator="transform.resize",
            keymap="3D View Tool: Scale",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def scale_cage():
        def draw_settings(context, layout, _tool):
            _template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index(context, layout, 3)
        return dict(
            idname="builtin.scale_cage",
            label="Scale Cage",
            icon="ops.transform.resize.cage",
            widget="VIEW3D_GGT_xform_cage",
            operator="transform.resize",
            keymap="3D View Tool: Scale",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def shear():
        def draw_settings(context, layout, _tool):
            # props = tool.operator_properties("transform.shear")
            _template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index(context, layout, 2)
        return dict(
            idname="builtin.shear",
            label="Shear",
            icon="ops.transform.shear",
            widget="VIEW3D_GGT_xform_shear",
            keymap="3D View Tool: Shear",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def bend():
        return dict(
            idname="builtin.bend",
            label="Bend",
            icon="ops.gpencil.edit_bend",
            widget=None,
            keymap="3D View Tool: Bend",
        )

    @ToolDef.from_fn
    def transform():
        def draw_settings(context, layout, tool):
            if layout.use_property_split:
                layout.label(text="Gizmos:")

            show_drag = True
            tool_settings = context.tool_settings
            if tool_settings.workspace_tool_type == 'FALLBACK':
                show_drag = False

            if show_drag:
                props = tool.gizmo_group_properties("VIEW3D_GGT_xform_gizmo")
                layout.prop(props, "drag_action")

            _defs_transform.draw_transform_sculpt_tool_settings(context, layout)
            _template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index(context, layout, 1)

        return dict(
            idname="builtin.transform",
            label="Transform",
            description=(
                "Supports any combination of grab, rotate, and scale at once"
            ),
            icon="ops.transform.transform",
            widget="VIEW3D_GGT_xform_gizmo",
            keymap="3D View Tool: Transform",
            draw_settings=draw_settings,
        )


class _defs_view3d_select:

    @ToolDef.from_fn
    def select():
        return dict(
            idname="builtin.select",
            label="Tweak",
            icon="ops.generic.select",
            widget=None,
            keymap="3D View Tool: Tweak",
        )

    @ToolDef.from_fn
    def box():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("view3d.select_box")
            row = layout.row()
            row.use_property_split = False
            row.prop(props, "mode", text="", expand=True, icon_only=True)
        return dict(
            idname="builtin.select_box",
            label="Select Box",
            icon="ops.generic.select_box",
            widget=None,
            keymap="3D View Tool: Select Box",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def lasso():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("view3d.select_lasso")
            row = layout.row()
            row.use_property_split = False
            row.prop(props, "mode", text="", expand=True, icon_only=True)
        return dict(
            idname="builtin.select_lasso",
            label="Select Lasso",
            icon="ops.generic.select_lasso",
            widget=None,
            keymap="3D View Tool: Select Lasso",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def circle():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("view3d.select_circle")
            row = layout.row()
            row.use_property_split = False
            row.prop(props, "mode", text="", expand=True, icon_only=True)
            layout.prop(props, "radius")

        def draw_cursor(_context, tool, xy):
            from gpu_extras.presets import draw_circle_2d
            props = tool.operator_properties("view3d.select_circle")
            radius = props.radius
            draw_circle_2d(xy, (1.0,) * 4, radius, segments=32)

        return dict(
            idname="builtin.select_circle",
            label="Select Circle",
            icon="ops.generic.select_circle",
            widget=None,
            keymap="3D View Tool: Select Circle",
            draw_settings=draw_settings,
            draw_cursor=draw_cursor,
        )


ToolDefaults = namedtuple("ToolDefaults", ["origin_base", "aspect_base", "origin_depth", "aspect_depth"])


class _defs_view3d_add:

    sculpt_tool_defaults = {
        'CUBE': ToolDefaults('EDGE', 'FREE', 'EDGE', 'FREE'),
        'CONE': ToolDefaults('CENTER', 'FIXED', 'EDGE', 'FREE'),
        'CYLINDER': ToolDefaults('CENTER', 'FIXED', 'EDGE', 'FREE'),
        'SPHERE_QUAD': ToolDefaults('CENTER', 'FIXED', 'CENTER', 'FIXED'),
        'SPHERE_UV': ToolDefaults('CENTER', 'FIXED', 'CENTER', 'FIXED'),
        'SPHERE_ICO': ToolDefaults('CENTER', 'FIXED', 'CENTER', 'FIXED'),
    }

    @staticmethod
    def description_interactive_add(context, _item, _km, *, prefix):
        km = context.window_manager.keyconfigs.user.keymaps["View3D Placement Modal"]

        def keymap_item_from_propvalue(propvalue):
            for item in km.keymap_items:
                if item.propvalue == propvalue:
                    return item
            return None

        if km is not None:
            kmi_snap = keymap_item_from_propvalue('SNAP_ON')
            kmi_center = keymap_item_from_propvalue('PIVOT_CENTER_ON')
            kmi_fixed_aspect = keymap_item_from_propvalue('FIXED_ASPECT_ON')
        else:
            kmi_snap = None
            kmi_center = None
            kmi_fixed_aspect = None
        return tip_(
            "{:s}\n"
            " \u2022 {:s} toggles snap while dragging\n"
            " \u2022 {:s} toggles dragging from the center\n"
            " \u2022 {:s} toggles fixed aspect"
        ).format(
            prefix,
            kmi_to_string_or_none(kmi_snap),
            kmi_to_string_or_none(kmi_center),
            kmi_to_string_or_none(kmi_fixed_aspect),
        )

    # Layout tweaks here would be good to avoid,
    # this shows limits in layout engine, as buttons are using a lot of space.
    @staticmethod
    def draw_settings_interactive_add(layout, tool_settings, tool, extra):
        show_extra = False
        if not extra:
            row = layout.row()
            row.prop(tool_settings, "plane_depth", text="Depth")
            row = layout.row()
            row.prop(tool_settings, "plane_orientation", text="Orientation")
            row = layout.row()
            row.prop(tool_settings, "snap_elements_tool")

            region_is_header = bpy.context.region.type == 'TOOL_HEADER'
            if region_is_header:
                # Don't draw the "extra" popover here as we might have other settings & this should be last.
                show_extra = True
            else:
                extra = True

        if extra:
            props = tool.operator_properties("view3d.interactive_add")
            layout.use_property_split = True
            layout.row().prop(tool_settings, "plane_axis", expand=True)
            layout.row().prop(tool_settings, "plane_axis_auto")

            layout.label(text="Base")
            layout.row().prop(props, "plane_origin_base", expand=True)
            layout.row().prop(props, "plane_aspect_base", expand=True)
            layout.label(text="Height")
            layout.row().prop(props, "plane_origin_depth", expand=True)
            layout.row().prop(props, "plane_aspect_depth", expand=True)
        return show_extra

    @staticmethod
    def draw_settings_defaults_init(mode, tool, primitive_type):
        if mode != 'SCULPT':
            return

        defaults = _defs_view3d_add.sculpt_tool_defaults[primitive_type]

        props = tool.operator_properties("view3d.interactive_add")

        if not props.is_property_set("plane_origin_base"):
            props.plane_origin_base = defaults.origin_base

        if not props.is_property_set("plane_aspect_base"):
            props.plane_aspect_base = defaults.aspect_base

        if not props.is_property_set("plane_origin_depth"):
            props.plane_origin_depth = defaults.origin_depth

        if not props.is_property_set("plane_aspect_depth"):
            props.plane_aspect_depth = defaults.aspect_depth

    @ToolDef.from_fn
    def cube_add():
        def draw_settings(context, layout, tool, *, extra=False):
            show_extra = _defs_view3d_add.draw_settings_interactive_add(layout, context.tool_settings, tool, extra)
            if show_extra:
                layout.popover("TOPBAR_PT_tool_settings_extra", text="...")

            _defs_view3d_add.draw_settings_defaults_init(context.mode, tool, 'CUBE')

        return dict(
            idname="builtin.primitive_cube_add",
            label="Add Cube",
            icon="ops.mesh.primitive_cube_add_gizmo",
            description=lambda *args: _defs_view3d_add.description_interactive_add(
                *args, prefix=tip_("Add cube to mesh interactively"),
            ),
            widget="VIEW3D_GGT_placement",
            keymap="3D View Tool: Object, Add Primitive",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def cone_add():
        def draw_settings(context, layout, tool, *, extra=False):
            show_extra = _defs_view3d_add.draw_settings_interactive_add(layout, context.tool_settings, tool, extra)
            if extra:
                return

            props = tool.operator_properties("mesh.primitive_cone_add")
            layout.prop(props, "vertices")
            layout.prop(props, "end_fill_type")

            if show_extra:
                layout.popover("TOPBAR_PT_tool_settings_extra", text="...")

            _defs_view3d_add.draw_settings_defaults_init(context.mode, tool, 'CONE')

        return dict(
            idname="builtin.primitive_cone_add",
            label="Add Cone",
            icon="ops.mesh.primitive_cone_add_gizmo",
            description=lambda *args: _defs_view3d_add.description_interactive_add(
                *args, prefix=tip_("Add cone to mesh interactively"),
            ),
            widget="VIEW3D_GGT_placement",
            keymap="3D View Tool: Object, Add Primitive",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def cylinder_add():
        def draw_settings(context, layout, tool, *, extra=False):
            show_extra = _defs_view3d_add.draw_settings_interactive_add(layout, context.tool_settings, tool, extra)
            if extra:
                return

            props = tool.operator_properties("mesh.primitive_cylinder_add")
            layout.prop(props, "vertices")
            layout.prop(props, "end_fill_type")

            if show_extra:
                layout.popover("TOPBAR_PT_tool_settings_extra", text="...")

            _defs_view3d_add.draw_settings_defaults_init(context.mode, tool, 'CYLINDER')

        return dict(
            idname="builtin.primitive_cylinder_add",
            label="Add Cylinder",
            icon="ops.mesh.primitive_cylinder_add_gizmo",
            description=lambda *args: _defs_view3d_add.description_interactive_add(
                *args, prefix=tip_("Add cylinder to mesh interactively"),
            ),
            widget="VIEW3D_GGT_placement",
            keymap="3D View Tool: Object, Add Primitive",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def uv_sphere_add():
        def draw_settings(context, layout, tool, *, extra=False):
            show_extra = _defs_view3d_add.draw_settings_interactive_add(layout, context.tool_settings, tool, extra)
            if extra:
                return

            props = tool.operator_properties("mesh.primitive_uv_sphere_add")
            layout.prop(props, "segments")
            layout.prop(props, "ring_count")

            if show_extra:
                layout.popover("TOPBAR_PT_tool_settings_extra", text="...")

            _defs_view3d_add.draw_settings_defaults_init(context.mode, tool, 'SPHERE_UV')

        return dict(
            idname="builtin.primitive_uv_sphere_add",
            label="Add UV Sphere",
            icon="ops.mesh.primitive_sphere_add_gizmo",
            description=lambda *args: _defs_view3d_add.description_interactive_add(
                *args, prefix=tip_("Add sphere to mesh interactively"),
            ),
            widget="VIEW3D_GGT_placement",
            keymap="3D View Tool: Object, Add Primitive",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def quad_sphere_add():
        def draw_settings(context, layout, tool, *, extra=False):
            show_extra = _defs_view3d_add.draw_settings_interactive_add(layout, context.tool_settings, tool, extra)
            if extra:
                return

            props = tool.operator_properties("mesh.primitive_quad_sphere_add")
            layout.prop(props, "segments")
            layout.prop(props, "method")

            if show_extra:
                layout.popover("TOPBAR_PT_tool_settings_extra", text="...")

            _defs_view3d_add.draw_settings_defaults_init(context.mode, tool, 'SPHERE_QUAD')

        return dict(
            idname="builtin.primitive_quad_sphere_add",
            label="Add Quad Sphere",
            icon="ops.mesh.primitive_sphere_add_gizmo",
            description=lambda *args: _defs_view3d_add.description_interactive_add(
                *args, prefix=tip_("Add sphere to mesh interactively"),
            ),
            widget="VIEW3D_GGT_placement",
            keymap="3D View Tool: Object, Add Primitive",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def ico_sphere_add():
        def draw_settings(context, layout, tool, *, extra=False):
            show_extra = _defs_view3d_add.draw_settings_interactive_add(layout, context.tool_settings, tool, extra)
            if extra:
                return

            props = tool.operator_properties("mesh.primitive_ico_sphere_add")
            layout.prop(props, "subdivisions")

            if show_extra:
                layout.popover("TOPBAR_PT_tool_settings_extra", text="...")

            _defs_view3d_add.draw_settings_defaults_init(context.mode, tool, 'SPHERE_ICO')

        return dict(
            idname="builtin.primitive_ico_sphere_add",
            label="Add Ico Sphere",
            icon="ops.mesh.primitive_sphere_add_gizmo",
            description=lambda *args: _defs_view3d_add.description_interactive_add(
                *args, prefix=tip_("Add sphere to mesh interactively"),
            ),
            widget="VIEW3D_GGT_placement",
            keymap="3D View Tool: Object, Add Primitive",
            draw_settings=draw_settings,
        )


# -----------------------------------------------------------------------------
# Object Modes (named based on context.mode)

class _defs_edit_armature:

    @ToolDef.from_fn
    def roll():
        return dict(
            idname="builtin.roll",
            label="Roll",
            icon="ops.armature.bone.roll",
            widget="VIEW3D_GGT_tool_generic_handle_free",
            keymap=(),
        )

    @ToolDef.from_fn
    def bone_envelope():
        return dict(
            idname="builtin.bone_envelope",
            label="Bone Envelope",
            icon="ops.transform.bone_envelope",
            widget="VIEW3D_GGT_tool_generic_handle_free",
            keymap=(),
        )

    @ToolDef.from_fn
    def bone_size():
        return dict(
            idname="builtin.bone_size",
            label="B-Bone Size",
            icon="ops.transform.bone_size",
            widget="VIEW3D_GGT_tool_generic_handle_free",
            keymap=(),
        )

    @ToolDef.from_fn
    def extrude():
        return dict(
            idname="builtin.extrude",
            label="Extrude",
            icon="ops.armature.extrude_move",
            widget="VIEW3D_GGT_xform_extrude",
            keymap=(),
            draw_settings=_template_widget.VIEW3D_GGT_xform_extrude.draw_settings,
        )

    @ToolDef.from_fn
    def extrude_cursor():
        return dict(
            idname="builtin.extrude_to_cursor",
            label="Extrude to Cursor",
            cursor='CROSSHAIR',
            icon="ops.armature.extrude_cursor",
            widget=None,
            keymap=(),
        )


class _defs_edit_mesh:

    @ToolDef.from_fn
    def rip_region():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("mesh.rip_move")
            props_macro = props.MESH_OT_rip
            layout.prop(props_macro, "use_fill")

        return dict(
            idname="builtin.rip_region",
            label="Rip Region",
            icon="ops.mesh.rip",
            widget="VIEW3D_GGT_tool_generic_handle_free",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def rip_edge():
        return dict(
            idname="builtin.rip_edge",
            label="Rip Edge",
            icon="ops.mesh.rip_edge",
            widget="VIEW3D_GGT_tool_generic_handle_free",
            keymap=(),
        )

    @ToolDef.from_fn
    def poly_build():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("mesh.polybuild_face_at_cursor_move")
            props_macro = props.MESH_OT_polybuild_face_at_cursor
            layout.prop(props_macro, "create_quads")

        def description(_context, _item, km):
            if km is not None:
                kmi_add = km.keymap_items.find_from_operator("mesh.polybuild_face_at_cursor_move")
                kmi_extrude = km.keymap_items.find_from_operator("mesh.polybuild_extrude_at_cursor_move")
                kmi_delete = km.keymap_items.find_from_operator("mesh.polybuild_delete_at_cursor")
            else:
                kmi_add = None
                kmi_extrude = None
                kmi_delete = None
            return tip_(
                "Use multiple operators in an interactive way to add, delete, or move geometry\n"
                " \u2022 {:s} - Add geometry by moving the cursor close to an element\n"
                " \u2022 {:s} - Extrude edges by moving the cursor\n"
                " \u2022 {:s} - Delete mesh element"
            ).format(
                kmi_to_string_or_none(kmi_add),
                kmi_to_string_or_none(kmi_extrude),
                kmi_to_string_or_none(kmi_delete),
            )
        return dict(
            idname="builtin.poly_build",
            description=description,
            label="Poly Build",
            icon="ops.mesh.polybuild_hover",
            widget="VIEW3D_GGT_mesh_preselect_elem",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def edge_slide():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("transform.edge_slide")
            layout.prop(props, "correct_uv")

        return dict(
            idname="builtin.edge_slide",
            label="Edge Slide",
            icon="ops.transform.edge_slide",
            widget="VIEW3D_GGT_tool_generic_handle_normal",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def vert_slide():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("transform.vert_slide")
            layout.prop(props, "correct_uv")

        return dict(
            idname="builtin.vertex_slide",
            label="Vertex Slide",
            icon="ops.transform.vert_slide",
            widget="VIEW3D_GGT_tool_generic_handle_free",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def spin():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("mesh.spin")
            layout.prop(props, "steps")
            layout.prop(props, "dupli")
            props = tool.gizmo_group_properties("MESH_GGT_spin")
            row = layout.row(align=True)
            row.prop(props, "axis", expand=True)

        return dict(
            idname="builtin.spin",
            label="Spin",
            icon="ops.mesh.spin",
            widget="MESH_GGT_spin",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def inset():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("mesh.inset")
            layout.prop(props, "use_outset")
            layout.prop(props, "use_individual")
            layout.prop(props, "use_even_offset")
            layout.prop(props, "use_relative_offset")

        return dict(
            idname="builtin.inset_faces",
            label="Inset Faces",
            icon="ops.mesh.inset",
            widget="VIEW3D_GGT_tool_generic_handle_free",
            widget_properties=[
                ("radius", 75.0),
                ("backdrop_fill_alpha", 0.0),
            ],
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def bevel():
        def draw_settings(context, layout, tool, *, extra=False):
            props = tool.operator_properties("mesh.bevel")

            region_is_header = context.region.type == 'TOOL_HEADER'

            edge_bevel = props.affect == 'EDGES'

            if not extra:
                if region_is_header:
                    layout.prop(props, "offset_type", text="")
                else:
                    layout.row().prop(props, "affect", expand=True)
                    layout.separator()
                    layout.prop(props, "offset_type")

                layout.prop(props, "segments")

                if region_is_header:
                    layout.prop(props, "affect", text="")

                layout.prop(props, "profile", text="Shape", slider=True)

                if region_is_header:
                    layout.popover("TOPBAR_PT_tool_settings_extra", text="...")
                else:
                    extra = True

            if extra:

                layout.prop(props, "material")

                col = layout.column()
                col.prop(props, "harden_normals")
                col.prop(props, "clamp_overlap")
                col.prop(props, "loop_slide")

                col = layout.column(heading="Mark")
                col.active = edge_bevel
                col.prop(props, "mark_seam", text="Seam")
                col.prop(props, "mark_sharp", text="Sharp")

                col = layout.column()
                col.active = edge_bevel
                col.prop(props, "miter_outer", text="Miter Outer")
                col.prop(props, "miter_inner", text="Inner")
                if props.miter_inner == 'ARC':
                    col.prop(props, "spread")

                layout.separator()

                col = layout.column()
                col.active = edge_bevel
                col.prop(props, "vmesh_method", text="Intersections")

                layout.prop(props, "face_strength_mode", text="Face Strength")

                layout.prop(props, "profile_type")

                if props.profile_type == 'CUSTOM':
                    tool_settings = context.tool_settings
                    layout.template_curveprofile(tool_settings, "custom_bevel_profile_preset")

        return dict(
            idname="builtin.bevel",
            label="Bevel",
            icon="ops.mesh.bevel",
            widget="VIEW3D_GGT_tool_generic_handle_normal",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def extrude():
        return dict(
            idname="builtin.extrude_region",
            label="Extrude Region",
            # The operator description isn't useful in this case, give our own.
            description=(
                "Extrude freely or along an axis"
            ),
            icon="ops.mesh.extrude_region_move",
            widget="VIEW3D_GGT_xform_extrude",
            # Important to use same operator as 'E' key.
            operator="view3d.edit_mesh_extrude_move_normal",
            keymap=(),
            draw_settings=_template_widget.VIEW3D_GGT_xform_extrude.draw_settings,
        )

    @ToolDef.from_fn
    def extrude_manifold():
        return dict(
            idname="builtin.extrude_manifold",
            label="Extrude Manifold",
            description=(
                "Extrude, dissolves edges whose faces form a flat surface and intersect new edges"
            ),
            icon="ops.mesh.extrude_manifold",
            widget="VIEW3D_GGT_tool_generic_handle_normal",
            keymap=(),
        )

    @ToolDef.from_fn
    def extrude_normals():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("mesh.extrude_region_shrink_fatten")
            props_macro = props.TRANSFORM_OT_shrink_fatten
            layout.prop(props_macro, "use_even_offset")
        return dict(
            idname="builtin.extrude_along_normals",
            label="Extrude Along Normals",
            icon="ops.mesh.extrude_region_shrink_fatten",
            widget="VIEW3D_GGT_tool_generic_handle_normal",
            operator="mesh.extrude_region_shrink_fatten",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def extrude_individual():
        return dict(
            idname="builtin.extrude_individual",
            label="Extrude Individual",
            icon="ops.mesh.extrude_faces_move",
            widget="VIEW3D_GGT_tool_generic_handle_normal",
            keymap=(),
        )

    @ToolDef.from_fn
    def extrude_cursor():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("mesh.dupli_extrude_cursor")
            layout.prop(props, "rotate_source")

        return dict(
            idname="builtin.extrude_to_cursor",
            label="Extrude to Cursor",
            cursor='CROSSHAIR',
            icon="ops.mesh.dupli_extrude_cursor",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def loopcut_slide():

        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("mesh.loopcut_slide")
            props_macro = props.MESH_OT_loopcut
            layout.prop(props_macro, "number_cuts")
            props_macro = props.TRANSFORM_OT_edge_slide
            layout.prop(props_macro, "correct_uv")

        return dict(
            idname="builtin.loop_cut",
            label="Loop Cut",
            icon="ops.mesh.loopcut_slide",
            widget="VIEW3D_GGT_mesh_preselect_edgering",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def offset_edge_loops_slide():
        return dict(
            idname="builtin.offset_edge_loop_cut",
            label="Offset Edge Loop Cut",
            icon="ops.mesh.offset_edge_loops_slide",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def vertex_smooth():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("mesh.vertices_smooth")
            layout.prop(props, "repeat")
        return dict(
            idname="builtin.smooth",
            label="Smooth",
            icon="ops.mesh.vertices_smooth",
            widget="VIEW3D_GGT_tool_generic_handle_normal",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def vertex_randomize():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("transform.vertex_random")
            layout.prop(props, "uniform")
            layout.prop(props, "normal")
            layout.prop(props, "seed")
        return dict(
            idname="builtin.randomize",
            label="Randomize",
            icon="ops.transform.vertex_random",
            widget="VIEW3D_GGT_tool_generic_handle_normal",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def tosphere():
        return dict(
            idname="builtin.to_sphere",
            label="To Sphere",
            icon="ops.transform.tosphere",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def shrink_fatten():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("transform.shrink_fatten")
            layout.prop(props, "use_even_offset")

        return dict(
            idname="builtin.shrink_fatten",
            label="Shrink/Fatten",
            icon="ops.transform.shrink_fatten",
            widget="VIEW3D_GGT_tool_generic_handle_normal",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def push_pull():
        return dict(
            idname="builtin.push_pull",
            label="Push/Pull",
            icon="ops.transform.push_pull",
            widget="VIEW3D_GGT_tool_generic_handle_normal",
            keymap=(),
        )

    @ToolDef.from_fn
    def knife():
        def draw_settings(_context, layout, tool, *, extra=False):
            show_extra = False
            props = tool.operator_properties("mesh.knife_tool")
            if not extra:
                layout.prop(props, "use_occlude_geometry")
                layout.prop(props, "only_selected")
                layout.prop(props, "xray")
                region_is_header = bpy.context.region.type == 'TOOL_HEADER'
                if region_is_header:
                    show_extra = True
                else:
                    extra = True
            if extra:

                layout.prop(props, "visible_measurements")
                layout.prop(props, "angle_snapping")
                layout.prop(props, "angle_snapping_increment", text="Snap Increment")
            if show_extra:
                layout.popover("TOPBAR_PT_tool_settings_extra", text="...")
        return dict(
            idname="builtin.knife",
            label="Knife",
            cursor='KNIFE',
            icon="ops.mesh.knife_tool",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
            options={'KEYMAP_FALLBACK'},
        )

    @ToolDef.from_fn
    def bisect():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("mesh.bisect")
            layout.prop(props, "use_fill")
            layout.prop(props, "clear_inner")
            layout.prop(props, "clear_outer")
            layout.prop(props, "threshold")
        return dict(
            idname="builtin.bisect",
            label="Bisect",
            icon="ops.mesh.bisect",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )


def curve_draw_settings(context, layout, tool, *, extra=False):
    # Tool settings initialize operator options.
    tool_settings = context.tool_settings
    cps = tool_settings.curve_paint_settings
    region_type = context.region.type

    if region_type == 'TOOL_HEADER':
        if not extra:
            layout.prop(cps, "curve_type", text="")
            layout.prop(cps, "depth_mode", expand=True)
            layout.popover("TOPBAR_PT_tool_settings_extra", text="...")
            return

    layout.use_property_split = True
    layout.use_property_decorate = False

    if region_type != 'TOOL_HEADER':
        layout.prop(cps, "curve_type")
        layout.separator()
    if cps.curve_type == 'BEZIER':
        layout.prop(cps, "fit_method")
        layout.prop(cps, "error_threshold")
        if region_type != 'TOOL_HEADER':
            row = layout.row(heading="Detect Corners", align=True)
        else:
            row = layout.row(heading="Corners", align=True)
        row.prop(cps, "use_corners_detect", text="")
        sub = row.row(align=True)
        sub.active = cps.use_corners_detect
        sub.prop(cps, "corner_angle", text="")
        layout.separator()

    col = layout.column(align=True)
    col.prop(cps, "radius_taper_start", text="Taper Start", slider=True)
    col.prop(cps, "radius_taper_end", text="End", slider=True)
    col = layout.column(align=True)
    col.prop(cps, "radius_min", text="Radius Min")
    col.prop(cps, "radius_max", text="Max")
    col.prop(cps, "use_pressure_radius")

    if region_type != 'TOOL_HEADER' or cps.depth_mode == 'SURFACE':
        layout.separator()

    if region_type != 'TOOL_HEADER':
        row = layout.row()
        row.prop(cps, "depth_mode", expand=True)
    if cps.depth_mode == 'SURFACE':
        col = layout.column()
        col.prop(cps, "use_project_only_selected")
        col.prop(cps, "surface_offset")
        col.prop(cps, "use_offset_absolute")
        col.prop(cps, "use_stroke_endpoints")
        if cps.use_stroke_endpoints:
            colsub = layout.column(align=True)
            colsub.prop(cps, "surface_plane")

    props = tool.operator_properties("curves.draw")
    col = layout.column(align=True)
    col.prop(props, "is_curve_2d", text="Curve 2D")
    col.prop(props, "bezier_as_nurbs", text="As NURBS")


class _defs_edit_curve:

    @ToolDef.from_fn
    def draw():
        return dict(
            idname="builtin.draw",
            label="Draw",
            cursor='PAINT_BRUSH',
            icon="ops.curve.draw",
            widget=None,
            keymap=(),
            draw_settings=curve_draw_settings,
        )

    @ToolDef.from_fn
    def extrude():
        return dict(
            idname="builtin.extrude",
            label="Extrude",
            icon="ops.curve.extrude_move",
            widget="VIEW3D_GGT_xform_extrude",
            keymap=(),
            draw_settings=_template_widget.VIEW3D_GGT_xform_extrude.draw_settings,
        )

    @ToolDef.from_fn
    def extrude_cursor():
        return dict(
            idname="builtin.extrude_cursor",
            label="Extrude to Cursor",
            cursor='CROSSHAIR',
            icon="ops.curve.extrude_cursor",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def pen():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("curve.pen")
            layout.prop(props, "close_spline")
            layout.prop(props, "extrude_handle")
        return dict(
            idname="builtin.pen",
            label="Curve Pen",
            cursor='CROSSHAIR',
            icon="ops.curve.pen",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def tilt():
        return dict(
            idname="builtin.tilt",
            label="Tilt",
            icon="ops.transform.tilt",
            widget="VIEW3D_GGT_tool_generic_handle_free",
            keymap=(),
        )

    @ToolDef.from_fn
    def curve_radius():
        return dict(
            idname="builtin.radius",
            label="Radius",
            description=(
                "Expand or contract the radius of the selected curve points"
            ),
            icon="ops.curve.radius",
            widget="VIEW3D_GGT_tool_generic_handle_free",
            keymap=(),
        )

    @ToolDef.from_fn
    def curve_vertex_randomize():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("transform.vertex_random")
            layout.prop(props, "uniform")
            layout.prop(props, "normal")
            layout.prop(props, "seed")
        return dict(
            idname="builtin.randomize",
            label="Randomize",
            icon="ops.curve.vertex_random",
            widget="VIEW3D_GGT_tool_generic_handle_normal",
            keymap=(),
            draw_settings=draw_settings,
        )


class _defs_edit_curves:

    @ToolDef.from_fn
    def draw():
        def curve_draw(context, layout, tool, *, extra=False):
            curve_draw_settings(context, layout, tool, extra=extra)

        return dict(
            idname="builtin.draw",
            label="Draw",
            cursor='PAINT_BRUSH',
            icon="ops.curve.draw",
            widget=None,
            keymap=(),
            draw_settings=curve_draw,
        )

    @ToolDef.from_fn
    def pen():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("curves.pen")
            layout.prop(props, "size")
        return dict(
            idname="builtin.pen",
            label="Pen",
            cursor='CROSSHAIR',
            icon="ops.curve.pen",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )


class _defs_edit_text:

    @ToolDef.from_fn
    def select_text():
        return dict(
            idname="builtin.select_text",
            label="Select Text",
            cursor='TEXT',
            icon="ops.generic.select_box",
            widget=None,
            keymap=(),
        )


class _defs_pose:

    @ToolDef.from_fn
    def breakdown():
        return dict(
            idname="builtin.breakdowner",
            label="Breakdowner",
            icon="ops.pose.breakdowner",
            widget=None,
            keymap="3D View Tool: Breakdowner",
        )

    @ToolDef.from_fn
    def push():
        return dict(
            idname="builtin.push",
            label="Push",
            icon="ops.pose.push",
            widget=None,
            keymap="3D View Tool: Push",
        )

    @ToolDef.from_fn
    def relax():
        return dict(
            idname="builtin.relax",
            label="Relax",
            icon="ops.pose.relax",
            widget=None,
            keymap="3D View Tool: Relax",
        )


class _defs_grease_pencil_edit:
    @ToolDef.from_fn
    def shear():
        def draw_settings(context, layout, _tool):
            _template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index(context, layout, 2)
        return dict(
            idname="builtin.shear",
            label="Shear",
            icon="ops.gpencil.edit_shear",
            widget="VIEW3D_GGT_xform_shear",
            keymap="3D View Tool: Shear",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def interpolate():
        def draw_settings(_context, layout, tool):
            props = tool.operator_properties("grease_pencil.interpolate")
            layout.prop(props, "layers")
            layout.prop(props, "exclude_breakdowns")
            layout.prop(props, "flip")
            layout.prop(props, "smooth_factor")
            layout.prop(props, "smooth_steps")

        return dict(
            idname="builtin.interpolate",
            label="Interpolate",
            icon="ops.pose.breakdowner",
            cursor='DEFAULT',
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def texture_gradient():
        return dict(
            idname="builtin.texture_gradient",
            label="Gradient",
            icon="ops.paint.weight_gradient",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def pen():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("grease_pencil.pen")
            layout.prop(props, "size")

            layout.separator()
            tool_settings = context.tool_settings

            sub = layout.row(align=True)
            sub.prop_with_popover(
                tool_settings,
                "gpencil_stroke_placement_view3d",
                text="",
                panel="VIEW3D_PT_grease_pencil_origin",
            )

            sub = layout.row(align=True)
            sub.active = tool_settings.gpencil_stroke_placement_view3d != 'SURFACE'
            sub.prop_with_popover(
                tool_settings.gpencil_sculpt,
                "lock_axis",
                text="",
                panel="VIEW3D_PT_grease_pencil_lock",
            )
        return dict(
            idname="builtin.pen",
            label="Pen",
            cursor='CROSSHAIR',
            icon="ops.curve.pen",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )


class VIEW3D_PT_tools_active(ToolSelectPanelHelper, Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_label = "Tools"  # not visible
    bl_options = {'HIDE_HEADER'}

    # Satisfy the `ToolSelectPanelHelper` API.
    keymap_prefix = "3D View Tool:"

    # Default group to use as a fallback.
    tool_fallback_id = "builtin.select"

    @classmethod
    def tools_from_context(cls, context, mode=None):
        if mode is None:
            mode = context.mode
        for tools in (cls._tools[None], cls._tools.get(mode, ())):
            for item in tools:
                if not (type(item) is ToolDef) and callable(item):
                    yield from item(context)
                else:
                    yield item

    @classmethod
    def tools_all(cls):
        yield from cls._tools.items()

    # Private tool lists for convenient reuse in `_tools`.

    _brush_tool = ToolDef.from_dict(
        dict(
            idname="builtin.brush",
            label="Brush",
            icon="brush.generic",
            options={'USE_BRUSHES'},
        )
    )

    _sculpt_tool = ToolDef.from_dict(
        dict(
            idname="builtin.brush",
            label="Brush",
            icon="brush.sculpt",
            options={'USE_BRUSHES'},
        )
    )

    _draw_tool = ToolDef.from_dict(
        dict(
            idname="builtin.brush",
            label="Brush",
            icon="brush.draw",
            options={'USE_BRUSHES'},
        )
    )

    _tools_transform = (
        _defs_transform.translate,
        _defs_transform.rotate,
        (
            _defs_transform.scale,
            _defs_transform.scale_cage,
        ),
        _defs_transform.transform,
    )

    _tools_select = (
        (
            _defs_view3d_select.select,
            _defs_view3d_select.box,
            _defs_view3d_select.circle,
            _defs_view3d_select.lasso,
        ),
    )

    _tools_annotate = (
        (
            _defs_annotate.scribble,
            _defs_annotate.line,
            _defs_annotate.poly,
            _defs_annotate.eraser,
        ),
    )

    _tools_view3d_add = (
        _defs_view3d_add.cube_add,
        _defs_view3d_add.cone_add,
        _defs_view3d_add.cylinder_add,
        _defs_view3d_add.quad_sphere_add,
        _defs_view3d_add.uv_sphere_add,
        _defs_view3d_add.ico_sphere_add,
    )

    _tools_pose = (
        _defs_pose.breakdown,
        _defs_pose.push,
        _defs_pose.relax,
    )

    _tools_default = (
        *_tools_select,
        _defs_view3d_generic.cursor,
        None,
        *_tools_transform,
        None,
        *_tools_annotate,
        _defs_view3d_generic.ruler,
    )

    # Private tools dictionary, store data to implement `tools_all` & `tools_from_context`.
    # The keys match object-modes from: `context.mode`.
    # The values represent the tools, see `ToolSelectPanelHelper` for details.
    _tools = {
        None: [
            # Don't use this! because of paint modes.
            # _defs_view3d_generic.cursor,
            # End group.
        ],
        'OBJECT': [
            *_tools_default,
            None,
            _tools_view3d_add,
            None,
            _tools_pose,
        ],
        'EDIT_ARMATURE': [
            *_tools_default,
            None,
            _defs_edit_armature.roll,
            (
                _defs_edit_armature.bone_size,
                _defs_edit_armature.bone_envelope,
            ),
            None,
            (
                _defs_edit_armature.extrude,
                _defs_edit_armature.extrude_cursor,
            ),
            _defs_transform.shear,
        ],
        'EDIT_MESH': [
            *_tools_default,

            None,
            _tools_view3d_add,
            None,
            (
                _defs_edit_mesh.extrude,
                _defs_edit_mesh.extrude_manifold,
                _defs_edit_mesh.extrude_normals,
                _defs_edit_mesh.extrude_individual,
                _defs_edit_mesh.extrude_cursor,
            ),
            _defs_edit_mesh.inset,
            _defs_edit_mesh.bevel,
            (
                _defs_edit_mesh.loopcut_slide,
                _defs_edit_mesh.offset_edge_loops_slide,
            ),
            (
                _defs_edit_mesh.knife,
                _defs_edit_mesh.bisect,
            ),
            _defs_edit_mesh.poly_build,
            _defs_edit_mesh.spin,
            (
                _defs_edit_mesh.vertex_smooth,
                _defs_edit_mesh.vertex_randomize,
            ),
            (
                _defs_edit_mesh.edge_slide,
                _defs_edit_mesh.vert_slide,
            ),
            (
                _defs_edit_mesh.shrink_fatten,
                _defs_edit_mesh.push_pull,
            ),
            (
                _defs_transform.shear,
                _defs_edit_mesh.tosphere,
            ),
            (
                _defs_edit_mesh.rip_region,
                _defs_edit_mesh.rip_edge,
            ),
        ],
        'EDIT_CURVE': [
            *_tools_default,
            None,
            _defs_edit_curve.draw,
            _defs_edit_curve.pen,
            (
                _defs_edit_curve.extrude,
                _defs_edit_curve.extrude_cursor,
            ),
            None,
            _defs_edit_curve.curve_radius,
            _defs_edit_curve.tilt,
            None,
            _defs_transform.shear,
            _defs_edit_curve.curve_vertex_randomize,
        ],
        'EDIT_CURVES': [
            *_tools_default,
            None,
            _defs_edit_curves.draw,
            _defs_edit_curves.pen,
            None,
            _defs_edit_curve.curve_radius,
            _defs_edit_curve.tilt,
        ],
        'EDIT_SURFACE': [
            *_tools_default,
            None,
            _defs_transform.shear,
        ],
        'EDIT_METABALL': [
            *_tools_default,
            None,
            _defs_transform.shear,
        ],
        'EDIT_LATTICE': [
            *_tools_default,
            None,
            _defs_transform.shear,
        ],
        'EDIT_TEXT': [
            _defs_edit_text.select_text,
            _defs_view3d_generic.cursor,
            None,
            *_tools_annotate,
            _defs_view3d_generic.ruler,
        ],
        'EDIT_GREASE_PENCIL': [
            *_tools_select,
            _defs_view3d_generic.cursor,
            None,
            *_tools_transform,
            None,
            _defs_grease_pencil_edit.pen,
            None,
            _defs_edit_curve.curve_radius,
            _defs_transform.bend,
            (
                _defs_grease_pencil_edit.shear,
                _defs_edit_mesh.tosphere,
            ),
            None,
            _defs_grease_pencil_edit.interpolate,
            None,
            _defs_grease_pencil_edit.texture_gradient,
            None,
            *_tools_annotate,
        ],
        'EDIT_POINTCLOUD': [
            *_tools_select,
            _defs_view3d_generic.cursor,
            None,
            *_tools_transform,
            None,
            *_tools_annotate,
            _defs_view3d_generic.ruler,
        ],
    }


classes = (
    VIEW3D_PT_tools_active,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
