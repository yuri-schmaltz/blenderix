# SPDX-FileCopyrightText: 2019-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# ------------------------------------------------------------------------------
# Configurable Parameters

class Params:
    __slots__ = (
        "select_mouse",
        "select_mouse_value",
        "action_mouse",
        "tool_mouse",
        "use_mouse_emulate_3_button",

    )

    def __init__(
            self,
            *,
            use_mouse_emulate_3_button=False,
    ):
        self.tool_mouse = 'LEFTMOUSE'
        self.select_mouse = 'LEFTMOUSE'
        self.select_mouse_value = 'CLICK'
        self.action_mouse = 'RIGHTMOUSE'
        self.use_mouse_emulate_3_button = use_mouse_emulate_3_button


# ------------------------------------------------------------------------------
# Constants


# Physical layout.
NUMBERS_1 = ('ONE', 'TWO', 'THREE', 'FOUR', 'FIVE', 'SIX', 'SEVEN', 'EIGHT', 'NINE', 'ZERO')
# Numeric order.
NUMBERS_0 = ('ZERO', 'ONE', 'TWO', 'THREE', 'FOUR', 'FIVE', 'SIX', 'SEVEN', 'EIGHT', 'NINE')


# ------------------------------------------------------------------------------
# Keymap Item Wrappers

def op_menu(menu, kmi_args):
    return ("wm.call_menu", kmi_args, {"properties": [("name", menu)]})


def op_menu_pie(menu, kmi_args):
    return ("wm.call_menu_pie", kmi_args, {"properties": [("name", menu)]})


def op_panel(menu, kmi_args, kmi_data=()):
    return ("wm.call_panel", kmi_args, {"properties": [("name", menu), *kmi_data]})


def op_asset_shelf_popup(asset_shelf, kmi_args):
    return ("wm.call_asset_shelf_popover", kmi_args, {"properties": [("name", asset_shelf)]})


def op_tool(tool, kmi_args):
    return ("wm.tool_set_by_id", kmi_args, {"properties": [("name", tool)]})


def op_tool_cycle(tool, kmi_args):
    return ("wm.tool_set_by_id", kmi_args, {"properties": [("name", tool), ("cycle", True)]})


# ------------------------------------------------------------------------------
# Keymap Templates

def _template_items_context_menu(menu, key_args_primary):
    return [
        op_menu(menu, kmi_args)
        for kmi_args in (key_args_primary, {"type": 'APP', "value": 'PRESS'})
    ]


def _template_items_context_panel(menu, key_args_primary):
    return [
        op_panel(menu, kmi_args)
        for kmi_args in (key_args_primary, {"type": 'APP', "value": 'PRESS'})
    ]


def _template_items_object_subdivision_set():
    return [
        ("object.subdivision_set",
         {"type": NUMBERS_0[i], "value": 'PRESS', "ctrl": True},
         {"properties": [("level", i), ("relative", False), ("ensure_modifier", True)]})
        for i in range(6)
    ]


def _template_items_animation():
    return [
        ("screen.frame_offset", {"type": 'LEFT_ARROW', "value": 'PRESS'},
         {"properties": [("delta", -1)]}),
        ("screen.frame_offset", {"type": 'RIGHT_ARROW', "value": 'PRESS'},
         {"properties": [("delta", 1)]}),
        ("screen.frame_jump", {"type": 'RIGHT_ARROW', "value": 'PRESS', "ctrl": True},
         {"properties": [("end", True)]}),
        ("screen.frame_jump", {"type": 'LEFT_ARROW', "value": 'PRESS', "ctrl": True},
         {"properties": [("end", False)]}),

    ]


def _template_items_gizmo_tweak_value_drag():
    return [
        ("gizmogroup.gizmo_tweak", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, None),
    ]


# Tool System Templates

def _template_items_basic_tools(*, connected=False):
    return [
        op_tool_cycle("builtin.select_box", {"type": 'Q', "value": 'PRESS'}),
        op_tool_cycle("builtin.move", {"type": 'W', "value": 'PRESS'}),
        op_tool_cycle("builtin.rotate", {"type": 'E', "value": 'PRESS'}),
        op_tool_cycle("builtin.scale", {"type": 'R', "value": 'PRESS'}),
        op_tool_cycle("builtin.transform", {"type": 'T', "value": 'PRESS'}),
        op_tool_cycle("builtin.annotate", {"type": 'D', "value": 'PRESS'}),
        op_tool_cycle("builtin.measure", {"type": 'M', "value": 'PRESS'}),
        op_tool_cycle("builtin.cursor", {"type": 'C', "value": 'PRESS'}),
    ]


def _template_items_tool_select(params, operator, *, extend):
    return [
        (operator, {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("deselect_all", True)]}),
        (operator, {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [(extend, True)]}),
    ]


def _template_items_tool_select_actions(operator, *, type, value):
    kmi_args = {"type": type, "value": value}
    return [
        (operator, kmi_args, None),
        (operator, {**kmi_args, "shift": True},
         {"properties": [("mode", 'ADD')]}),
        (operator, {**kmi_args, "ctrl": True},
         {"properties": [("mode", 'SUB')]}),
        (operator, {**kmi_args, "shift": True, "ctrl": True},
         {"properties": [("mode", 'AND')]}),
    ]


# This could have a more generic name, for now use for circle select.
def _template_items_tool_select_actions_simple(operator, *, type, value, properties=()):
    kmi_args = {"type": type, "value": value}
    return [
        # Don't define 'SET' here, take from the tool options.
        (operator, kmi_args,
         {"properties": [*properties]}),
        (operator, {**kmi_args, "shift": True},
         {"properties": [*properties, ("mode", 'ADD')]}),
        (operator, {**kmi_args, "ctrl": True},
         {"properties": [*properties, ("mode", 'SUB')]}),
    ]


def _template_items_editmode_mesh_select_mode(params):
    return [
        (
            "mesh.select_mode",
            {"type": k, "value": 'PRESS', **key_expand, **key_extend},
            {"properties": [*prop_extend, *prop_expand, ("type", e)]}
        )
        for key_expand, prop_expand in (({}, ()), ({"ctrl": True}, (("use_expand", True),)))
        for key_extend, prop_extend in (({}, ()), ({"shift": True}, (("use_extend", True),)))
        for k, e in (('ONE', 'VERT'), ('TWO', 'EDGE'), ('THREE', 'FACE'))
    ]


# ------------------------------------------------------------------------------
# Window, Screen, Areas, Regions

def km_window(params):
    items = []
    keymap = (
        "Window",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("wm.batch_rename", {"type": 'RET', "value": 'PRESS', "alt": True}, None),

        # File operations
        ("wm.read_homefile", {"type": 'N', "value": 'PRESS', "ctrl": True}, None),
        op_menu("TOPBAR_MT_file_open_recent", {"type": 'O', "value": 'PRESS', "shift": True, "ctrl": True}),
        ("wm.open_mainfile", {"type": 'O', "value": 'PRESS', "ctrl": True}, None),
        ("wm.save_mainfile", {"type": 'S', "value": 'PRESS', "ctrl": True},
         {"properties": [("show_save_modified_images_dialog", True)]}),
        ("wm.save_as_mainfile", {"type": 'S', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("show_save_modified_images_dialog", True)]}),
        ("wm.save_mainfile",
         {"type": 'S', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("incremental", True), ("show_save_modified_images_dialog", True)]}),
        ("wm.quit_blender", {"type": 'Q', "value": 'PRESS', "ctrl": True}, None),

        # Quick menu and toolbar
        op_menu("SCREEN_MT_user_menu", {"type": 'TAB', "value": 'PRESS', "shift": True}),

        # NDOF settings
        op_panel("USERPREF_PT_ndof_settings", {"type": 'NDOF_BUTTON_MENU', "value": 'PRESS'}),
        ("wm.context_scale_float", {"type": 'NDOF_BUTTON_PLUS', "value": 'PRESS'},
         {"properties": [("data_path", "preferences.inputs.ndof_translation_sensitivity"), ("value", 1.1)]}),
        ("wm.context_scale_float", {"type": 'NDOF_BUTTON_MINUS', "value": 'PRESS'},
         {"properties": [("data_path", "preferences.inputs.ndof_translation_sensitivity"), ("value", 1.0 / 1.1)]}),
        ("wm.context_scale_float", {"type": 'NDOF_BUTTON_PLUS', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", "preferences.inputs.ndof_translation_sensitivity"), ("value", 1.5)]}),
        ("wm.context_scale_float", {"type": 'NDOF_BUTTON_MINUS', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", "preferences.inputs.ndof_translation_sensitivity"), ("value", 2.0 / 3.0)]}),
        ("info.reports_display_update", {"type": 'TIMER_REPORT', "value": 'ANY', "any": True}, None),
    ])

    return keymap


def km_screen(params):
    items = []
    keymap = (
        "Screen",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("screen.repeat_last", {"type": 'G', "value": 'PRESS', "repeat": True}, None),
        # Animation
        ("screen.userpref_show", {"type": 'COMMA', "value": 'PRESS', "ctrl": True}, None),
        ("screen.animation_step", {"type": 'TIMER0', "value": 'ANY', "any": True}, None),
        ("screen.region_blend", {"type": 'TIMERREGION', "value": 'ANY', "any": True}, None),
        # Full screen and cycling
        ("screen.space_context_cycle", {"type": 'TAB', "value": 'PRESS', "ctrl": True},
         {"properties": [("direction", 'NEXT')]}),
        ("screen.space_context_cycle", {"type": 'TAB', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("direction", 'PREV')]}),
        # Files
        ("file.execute", {"type": 'RET', "value": 'PRESS'}, None),
        ("file.execute", {"type": 'NUMPAD_ENTER', "value": 'PRESS'}, None),
        ("file.cancel", {"type": 'ESC', "value": 'PRESS'}, None),
        # Asset Catalog undo is only available in the asset browser, and should take priority over `ed.undo`.
        ("asset.catalog_undo", {"type": 'Z', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("asset.catalog_redo", {"type": 'Z', "value": 'PRESS', "ctrl": True, "shift": True, "repeat": True}, None),
        # Undo
        ("ed.undo", {"type": 'Z', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("ed.redo", {"type": 'Z', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True}, None),
        ("ed.undo_history", {"type": 'Z', "value": 'PRESS', "alt": True, "ctrl": True}, None),
        # Render
        ("render.view_cancel", {"type": 'ESC', "value": 'PRESS'}, None),
    ])

    return keymap


def km_screen_editing(params):
    items = []
    keymap = (
        "Screen Editing",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Action zones
        ("screen.actionzone", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("modifier", 0)]}),
        ("screen.actionzone", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("modifier", 1)]}),
        ("screen.actionzone", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("modifier", 2)]}),
        # Screen tools
        ("screen.area_split", {"type": 'ACTIONZONE_AREA', "value": 'ANY'}, None),
        ("screen.area_join", {"type": 'ACTIONZONE_AREA', "value": 'ANY'}, None),
        ("screen.area_dupli", {"type": 'ACTIONZONE_AREA', "value": 'ANY', "shift": True}, None),
        ("screen.area_swap", {"type": 'ACTIONZONE_AREA', "value": 'ANY', "ctrl": True}, None),
        ("screen.region_scale", {"type": 'ACTIONZONE_REGION', "value": 'ANY'}, None),
        ("screen.quadview_size", {"type": 'ACTIONZONE_REGION_QUAD', "value": 'ANY'}, None),
        ("screen.screen_full_area", {"type": 'ACTIONZONE_FULLSCREEN', "value": 'ANY'},
         {"properties": [("use_hide_panels", True)]}),
        # Area move after action zones
        ("screen.area_move", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("screen.area_move", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("snap", True)]}),
        ("screen.area_options", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
        ("render.view_cancel", {"type": 'ESC', "value": 'PRESS'}, None),
    ])

    return keymap


def km_screen_region_context_menu(_params):
    items = []
    keymap = (
        "Region Context Menu",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("screen.region_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
    ])

    return keymap


def km_view2d(params):
    items = []
    keymap = (
        "View2D",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Scroll-bars.
        ("view2d.scroller_activate", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroller_activate", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        # Pan/scroll
        ("view2d.pan", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view2d.pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view2d.zoom", {"type": 'RIGHTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view2d.pan", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("view2d.scroll_right", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("view2d.scroll_left", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("view2d.scroll_down", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "shift": True}, None),
        ("view2d.scroll_up", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "shift": True}, None),
        ("view2d.ndof", {"type": 'NDOF_MOTION', "value": 'ANY'}, None),
        # Zoom with single step
        ("view2d.zoom_out", {"type": 'WHEELOUTMOUSE', "value": 'PRESS'}, None),
        ("view2d.zoom_in", {"type": 'WHEELINMOUSE', "value": 'PRESS'}, None),
        ("view2d.zoom_out", {"type": 'WHEELOUTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view2d.zoom_in", {"type": 'WHEELINMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view2d.zoom_out", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "repeat": True}, None),
        ("view2d.zoom_in", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "repeat": True}, None),
        ("view2d.zoom", {"type": 'TRACKPADPAN', "value": 'ANY', "ctrl": True}, None),
        ("view2d.smoothview", {"type": 'TIMER1', "value": 'ANY', "any": True}, None),
        # Scroll up/down, only when zoom is not available.
        ("view2d.scroll_down", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_up", {"type": 'WHEELUPMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_right", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_left", {"type": 'WHEELUPMOUSE', "value": 'PRESS'}, None),
        # Zoom with drag and border
        ("view2d.zoom", {"type": 'TRACKPADZOOM', "value": 'ANY'}, None),
        ("view2d.zoom_border", {"type": 'Z', "value": 'PRESS'}, None),
    ])

    return keymap


def km_view2d_buttons_list(params):
    items = []
    keymap = (
        "View2D Buttons List",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Scroll-bars.
        ("view2d.scroller_activate", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroller_activate", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        # Pan scroll
        ("view2d.pan", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view2d.pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view2d.pan", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        # NOTE: mostly navigation uses `Alt`, in the buttons window
        # there is no need for tool access, so allow MMB panning.
        ("view2d.pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_down", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_up", {"type": 'WHEELUPMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_down", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True},
         {"properties": [("page", True)]}),
        ("view2d.scroll_up", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True},
         {"properties": [("page", True)]}),
        # Zoom
        ("view2d.zoom", {"type": 'RIGHTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view2d.zoom", {"type": 'TRACKPADZOOM', "value": 'ANY'}, None),
        ("view2d.zoom", {"type": 'TRACKPADPAN', "value": 'ANY', "ctrl": True}, None),
        ("view2d.zoom_out", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "repeat": True}, None),
        ("view2d.zoom_in", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "repeat": True}, None),
        ("view2d.reset", {"type": 'A', "value": 'PRESS'}, None),
    ])

    return keymap


def km_user_interface(params):
    items = []
    keymap = (
        "User Interface",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Eyedroppers all have the same event, and pass it through until
        # a suitable eyedropper handles it.
        ("ui.eyedropper_color", {"type": 'I', "value": 'PRESS'}, None),
        ("ui.eyedropper_colorramp", {"type": 'I', "value": 'PRESS'}, None),
        ("ui.eyedropper_colorramp_point", {"type": 'I', "value": 'PRESS', "alt": True}, None),
        ("ui.eyedropper_id", {"type": 'I', "value": 'PRESS'}, None),
        ("ui.eyedropper_depth", {"type": 'I', "value": 'PRESS'}, None),
        ("ui.eyedropper_bone", {"type": 'I', "value": 'PRESS'}, None),
        # Copy data path
        ("ui.copy_data_path_button", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("ui.copy_data_path_button", {"type": 'C', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("full_path", True)]}),
        # Frames and drivers.
        ("anim.keyframe_insert_button", {"type": 'S', "value": 'PRESS'}, None),
        ("anim.keyframe_delete_button", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        ("anim.keyframe_clear_button", {"type": 'S', "value": 'PRESS', "shift": True, "alt": True}, None),
        ("anim.driver_button_add", {"type": 'D', "value": 'PRESS'}, None),
        ("anim.driver_button_remove", {"type": 'D', "value": 'PRESS', "alt": True}, None),
        ("anim.keyingset_button_add", {"type": 'K', "value": 'PRESS'}, None),
        ("anim.keyingset_button_remove", {"type": 'K', "value": 'PRESS', "alt": True}, None),
        ("ui.view_item_select", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("ui.view_scroll", {"type": 'WHEELUPMOUSE', "value": 'ANY'}, None),
        ("ui.view_scroll", {"type": 'WHEELDOWNMOUSE', "value": 'ANY'}, None),
        ("ui.view_scroll", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Editors


def km_property_editor(params):
    items = []
    keymap = (
        "Property Editor",
        {"space_type": 'PROPERTIES', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        ("buttons.context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
        ("screen.space_context_cycle", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("direction", 'PREV'), ], },),
        ("screen.space_context_cycle", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("direction", 'NEXT'), ], },),
        ("buttons.start_filter", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        ("buttons.clear_filter", {"type": 'ESC', "value": 'PRESS'}, None),
        # Modifier panels
        ("object.modifier_set_active", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("object.modifier_remove", {"type": 'BACK_SPACE', "value": 'PRESS'}, {"properties": [("report", True)]}),
        ("object.modifier_remove", {"type": 'DEL', "value": 'PRESS'}, {"properties": [("report", True)]}),
        ("object.modifier_copy", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        # ShaderFX panels
        ("object.shaderfx_remove", {"type": 'BACK_SPACE', "value": 'PRESS'}, {"properties": [("report", True)]}),
        ("object.shaderfx_remove", {"type": 'DEL', "value": 'PRESS'}, {"properties": [("report", True)]}),
        ("objectshaderfx_copy", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        # Constraint panels
        ("constraint.delete", {"type": 'BACK_SPACE', "value": 'PRESS'}, {"properties": [("report", True)]}),
        ("constraint.delete", {"type": 'DEL', "value": 'PRESS'}, {"properties": [("report", True)]}),
        ("constraint.copy", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        # Strip modifiers
        ("sequencer.strip_modifier_set_active", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
    ])

    return keymap


def km_outliner(params):
    items = []
    keymap = (
        "Outliner",
        {"space_type": 'OUTLINER', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("outliner.item_rename", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("outliner.item_rename", {"type": 'RET', "value": 'PRESS'},
         {"properties": [("use_active", True)]}),
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        ("outliner.highlight_update", {"type": 'MOUSEMOVE', "value": 'ANY', "any": True}, None),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'CLICK'},
         {"properties": [("extend", False), ("deselect_all", True)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'CLICK', "ctrl": True},
         {"properties": [("extend", True), ("deselect_all", True)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'CLICK', "shift": True},
         {"properties": [("extend", False), ("extend_range", True), ("deselect_all", True)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'CLICK', "ctrl": True, "shift": True},
         {"properties": [("extend", True), ("extend_range", True), ("deselect_all", True)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'},
         {"properties": [("recurse", True), ("deselect_all", True)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "ctrl": True},
         {"properties": [("recurse", True), ("extend", True), ("deselect_all", True)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "shift": True},
         {"properties": [("recurse", True), ("extend_range", True), ("deselect_all", True)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "ctrl": True, "shift": True},
            {"properties": [("recurse", True), ("extend", True), ("extend_range", True), ("deselect_all", True)]}),
        ("outliner.select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, {"properties": [("tweak", True)]}),
        ("outliner.select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "shift": True},
         {"properties": [("tweak", True), ("mode", 'ADD')]}),
        ("outliner.select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "ctrl": True},
         {"properties": [("tweak", True), ("mode", 'SUB')]}),
        ("outliner.select_walk", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'UP')]}),
        ("outliner.select_walk", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'UP'), ("extend", True)]}),
        ("outliner.select_walk", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'DOWN')]}),
        ("outliner.select_walk", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'DOWN'), ("extend", True)]}),
        ("outliner.select_walk", {"type": 'LEFT_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'LEFT')]}),
        ("outliner.select_walk", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'LEFT'), ("toggle_all", True)]}),
        ("outliner.select_walk", {"type": 'RIGHT_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'RIGHT')]}),
        ("outliner.select_walk", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'RIGHT'), ("toggle_all", True)]}),
        ("outliner.item_openclose", {"type": 'LEFTMOUSE', "value": 'CLICK'},
         {"properties": [("all", False)]}),
        ("outliner.item_openclose", {"type": 'LEFTMOUSE', "value": 'CLICK', "shift": True},
         {"properties": [("all", True)]}),
        ("outliner.item_openclose", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'},
         {"properties": [("all", False)]}),
        # Fall through to generic context menu if the item(s) selected have no type specific actions.
        ("outliner.operation", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
        op_menu("OUTLINER_MT_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        ("outliner.item_drag_drop", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, None),
        ("outliner.item_drag_drop", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "shift": True}, None),
        ("outliner.show_hierarchy", {"type": 'A', "value": 'PRESS'}, None),
        ("outliner.show_active", {"type": 'PERIOD', "value": 'PRESS'}, None),
        ("outliner.show_active", {"type": 'F', "value": 'PRESS'}, None),
        ("outliner.scroll_page", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True},
         {"properties": [("up", False)]}),
        ("outliner.scroll_page", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True},
         {"properties": [("up", True)]}),
        ("outliner.show_one_level", {"type": 'NUMPAD_PLUS', "value": 'PRESS'}, None),
        ("outliner.show_one_level", {"type": 'NUMPAD_MINUS', "value": 'PRESS'},
         {"properties": [("open", False)]}),
        ("outliner.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("outliner.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True,
         "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("outliner.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("outliner.keyingset_add_selected", {"type": 'K', "value": 'PRESS'}, None),
        ("outliner.keyingset_remove_selected", {"type": 'K', "value": 'PRESS', "alt": True}, None),
        ("anim.keyframe_insert", {"type": 'S', "value": 'PRESS'}, None),
        ("anim.keyframe_delete", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        ("outliner.drivers_add_selected", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("outliner.drivers_delete_selected", {"type": 'D', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("outliner.delete", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("outliner.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        op_menu("OBJECT_MT_move_to_collection", {"type": 'G', "value": 'PRESS', "ctrl": True}),
        op_menu("OBJECT_MT_link_to_collection", {"type": 'M', "value": 'PRESS', "shift": True, "ctrl": True}),
        ("outliner.collection_exclude_set", {"type": 'E', "value": 'PRESS'}, None),
        ("outliner.collection_exclude_clear", {"type": 'E', "value": 'PRESS', "alt": True}, None),
        ("outliner.hide", {"type": 'H', "value": 'PRESS', "ctrl": True}, None),
        ("outliner.unhide_all", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("outliner.start_filter", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        ("outliner.clear_filter", {"type": 'F', "value": 'PRESS', "alt": True}, None),
        # Copy/paste.
        ("outliner.id_copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("outliner.id_paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
    ])

    return keymap


# 3D View: all regions.
def km_view3d_generic(_params):
    items = []
    keymap = (
        "3D View Generic",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("wm.context_toggle", {"type": 'LEFT_BRACKET', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.show_region_toolbar")]}),
        ("wm.context_toggle", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.show_region_ui")]}),
        ("wm.context_toggle", {"type": 'SPACE', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", "space_data.show_region_asset_shelf")]}),
    ])

    return keymap


# 3D View: main region.
def km_view3d(params):
    items = []
    keymap = (
        "3D View",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        op_panel("TOPBAR_PT_name", {"type": 'RET', "value": 'PRESS'}, [("keep_open", False)]),
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        # 3D Cursor
        ("view3d.cursor3d", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True}, None),
        ("transform.translate", {"type": 'RIGHTMOUSE', "value": 'CLICK_DRAG', "shift": True},
         {"properties": [("release_confirm", True), ("cursor_transform", True)]}),
        # Visibility.
        ("view3d.localview", {"type": 'I', "value": 'PRESS', "shift": True}, None),
        ("view3d.localview", {"type": 'MOUSESMARTZOOM', "value": 'ANY'}, None),
        op_menu_pie("VIEW3D_MT_view_pie", {"type": 'V', "value": 'PRESS'}),
        # Navigation.
        ("view3d.rotate", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view3d.view_pan", {"type": 'WHEELLEFTMOUSE', "value": 'PRESS'},
            {"properties": [("type", 'PANLEFT')]}),
        ("view3d.view_pan", {"type": 'WHEELRIGHTMOUSE', "value": 'PRESS'},
            {"properties": [("type", 'PANRIGHT')]}),
        ("view3d.move", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view3d.zoom", {"type": 'RIGHTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view3d.view_selected", {"type": 'F', "value": 'PRESS'},
         {"properties": [("use_all_regions", False)]}),
        ("view3d.view_center_pick", {"type": 'F', "value": 'PRESS', "shift": True}, None),
        ("view3d.smoothview", {"type": 'TIMER1', "value": 'ANY', "any": True}, None),
        # Trackpad
        ("view3d.rotate", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("view3d.rotate", {"type": 'MOUSEROTATE', "value": 'ANY'}, None),
        ("view3d.move", {"type": 'TRACKPADPAN', "value": 'ANY', "shift": True}, None),
        ("view3d.zoom", {"type": 'TRACKPADZOOM', "value": 'ANY'}, None),
        ("view3d.zoom", {"type": 'TRACKPADPAN', "value": 'ANY', "ctrl": True}, None),
        # Numpad
        ("view3d.zoom", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "repeat": True},
         {"properties": [("delta", 1)]}),
        ("view3d.zoom", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "repeat": True},
         {"properties": [("delta", -1)]}),
        ("view3d.zoom", {"type": 'WHEELINMOUSE', "value": 'PRESS'},
         {"properties": [("delta", 1)]}),
        ("view3d.zoom", {"type": 'WHEELOUTMOUSE', "value": 'PRESS'},
         {"properties": [("delta", -1)]}),
        ("view3d.zoom", {"type": 'WHEELINMOUSE', "value": 'PRESS', "alt": True},
         {"properties": [("delta", 1)]}),
        ("view3d.zoom", {"type": 'WHEELOUTMOUSE', "value": 'PRESS', "alt": True},
         {"properties": [("delta", -1)]}),
        ("view3d.dolly", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("delta", 1)]}),
        ("view3d.dolly", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("delta", -1)]}),
        ("view3d.view_all", {"type": 'A', "value": 'PRESS'},
         {"properties": [("center", False)]}),
        ("view3d.view_all", {"type": 'A', "value": 'PRESS', "shift": True},
         {"properties": [("use_all_regions", True), ("center", False)]}),
        # Numpad views.
        ("view3d.view_camera", {"type": 'F4', "value": 'PRESS'}, None),
        ("view3d.view_axis", {"type": 'F1', "value": 'PRESS'},
         {"properties": [("type", 'FRONT')]}),
        ("view3d.view_axis", {"type": 'F2', "value": 'PRESS'},
         {"properties": [("type", 'RIGHT')]}),
        ("view3d.view_axis", {"type": 'F3', "value": 'PRESS'},
         {"properties": [("type", 'TOP')]}),
        ("view3d.view_axis", {"type": 'F1', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'BACK')]}),
        ("view3d.view_axis", {"type": 'F2', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'LEFT')]}),
        ("view3d.view_axis", {"type": 'F3', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'BOTTOM')]}),
        ("view3d.view_orbit", {"type": 'F5', "value": 'PRESS'},
         {"properties": [("angle", 3.1415927), ("type", 'ORBITRIGHT')]}),
        # NDOF
        ("view3d.ndof_orbit_zoom", {"type": 'NDOF_MOTION', "value": 'ANY'}, None),
        ("view3d.ndof_orbit", {"type": 'NDOF_MOTION', "value": 'ANY', "ctrl": True}, None),
        ("view3d.ndof_pan", {"type": 'NDOF_MOTION', "value": 'ANY', "shift": True}, None),
        ("view3d.ndof_all", {"type": 'NDOF_MOTION', "value": 'ANY', "shift": True, "ctrl": True}, None),
        ("view3d.view_selected", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'},
         {"properties": [("use_all_regions", False)]}),
        ("view3d.view_roll", {"type": 'NDOF_BUTTON_ROLL_CCW', "value": 'PRESS'},
         {"properties": [("type", 'LEFT')]}),
        ("view3d.view_roll", {"type": 'NDOF_BUTTON_ROLL_CCW', "value": 'PRESS'},
         {"properties": [("type", 'RIGHT')]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_FRONT', "value": 'PRESS'},
         {"properties": [("type", 'FRONT')]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_BACK', "value": 'PRESS'},
         {"properties": [("type", 'BACK')]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_LEFT', "value": 'PRESS'},
         {"properties": [("type", 'LEFT')]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_RIGHT', "value": 'PRESS'},
         {"properties": [("type", 'RIGHT')]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_TOP', "value": 'PRESS'},
         {"properties": [("type", 'TOP')]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_BOTTOM', "value": 'PRESS'},
         {"properties": [("type", 'BOTTOM')]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_FRONT', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'FRONT'), ("align_active", True)]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_RIGHT', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'RIGHT'), ("align_active", True)]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_TOP', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'TOP'), ("align_active", True)]}),
        # Selection.
        *((operator,
           {"type": 'LEFTMOUSE', "value": 'CLICK', **{m: True for m in mods}},
           {"properties": [(c, True) for c in props]},
           ) for operator, props, mods in (
            ("view3d.select", ("deselect_all",), ()),
            ("view3d.select", ("toggle",), ("shift",)),
            ("view3d.select", ("center", "object"), ("ctrl",)),
            ("view3d.select", ("extend", "toggle", "center"), ("shift", "ctrl")),
            ("view3d.select", ("toggle", "enumerate"), ("shift", "alt")),
            ("view3d.select", ("toggle", "center", "enumerate"), ("shift", "ctrl", "alt")),
        )),
        ("view3d.zoom_border", {"type": 'Z', "value": 'PRESS'}, None),
        # Copy/paste.
        ("view3d.copybuffer", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("view3d.pastebuffer", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        # Menus.
        op_menu_pie("VIEW3D_MT_snap_pie", {"type": 'X', "value": 'PRESS', "shift": True}),
        # Transform.
        ("transform.translate", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, None),
        op_menu_pie("VIEW3D_MT_pivot_pie", {"type": 'PERIOD', "value": 'PRESS'}),
        op_menu_pie("VIEW3D_MT_orientations_pie", {"type": 'COMMA', "value": 'PRESS'}),
        ("view3d.toggle_xray", {"type": 'X', "value": 'PRESS', "alt": True}, None),
        ("wm.context_toggle", {"type": 'X', "value": 'PRESS'},
         {"properties": [("data_path", "tool_settings.use_snap")]}),
    ])

    return keymap


def km_markers(params):
    items = []
    keymap = (
        "Markers",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        ("marker.add", {"type": 'M', "value": 'PRESS'}, None),
        ("marker.move", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, None),
        ("marker.duplicate", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("marker.select", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("marker.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("marker.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("extend", False), ("camera", True)]}),
        ("marker.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("extend", True), ("camera", True)]}),
        ("marker.select_leftright",
         {"type": params.select_mouse, "value": 'CLICK', "ctrl": True},
         {"properties": [("mode", 'CLICK_SIDE'), ("extend", False)]}),
        ("marker.select_leftright",
         {"type": params.select_mouse, "value": 'CLICK', "ctrl": True, "shift": True},
         {"properties": [("mode", 'CLICK_SIDE'), ("extend", True)]}),
        ("marker.select_leftright", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'LEFT'), ("extend", False)]}),
        ("marker.select_leftright", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'RIGHT'), ("extend", False)]}),
        ("marker.select_box", {"type": 'Q', "value": 'PRESS'}, None),
        ("marker.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("marker.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True,
         "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("marker.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("marker.delete", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("marker.delete", {"type": 'DEL', "value": 'PRESS'}, {"properties": [("confirm", False)]}),
        op_panel("TOPBAR_PT_name_marker", {"type": 'RET', "value": 'PRESS'}, [("keep_open", False)]),
        op_panel("TOPBAR_PT_name_marker", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, [("keep_open", False)]),
        ("marker.move", {"type": 'W', "value": 'PRESS'}, None),
    ])

    return keymap


def km_info(params):
    items = []
    keymap = (
        "Info",
        {"space_type": 'INFO', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        op_panel("TOPBAR_PT_name", {"type": 'RET', "value": 'PRESS'}, [("keep_open", False)]),
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        ("info.select_pick", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("info.select_pick", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("info.select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'},
         {"properties": [("wait_for_input", False)]}),
        ("info.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True},
         {"properties": [("action", 'SELECT')]}),
        ("info.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True},
         {"properties": [("action", 'DESELECT')]}),
        ("info.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True},
         {"properties": [("action", 'INVERT')]}),
        ("info.select_box", {"type": 'Q', "value": 'PRESS'}, None),
        ("info.report_replay", {"type": 'R', "value": 'PRESS'}, None),
        ("info.report_delete", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("info.report_delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("info.report_copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        *_template_items_context_menu("INFO_MT_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
    ])

    return keymap


def km_file_browser(params):
    items = []
    keymap = (
        "File Browser",
        {"space_type": 'FILE_BROWSER', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("wm.context_toggle", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.show_region_tool_props")]}),
        ("file.parent", {"type": 'UP_ARROW', "value": 'PRESS', "alt": True}, None),
        ("file.parent", {"type": 'UP_ARROW', "value": 'PRESS', "ctrl": True}, None),
        ("file.previous", {"type": 'LEFT_ARROW', "value": 'PRESS', "alt": True}, None),
        ("file.previous", {"type": 'LEFT_ARROW', "value": 'PRESS', "ctrl": True}, None),
        ("file.previous", {"type": 'BUTTON4MOUSE', "value": 'PRESS'}, None),
        ("file.previous", {"type": 'LEFT_BRACKET', "value": 'PRESS', "ctrl": True}, None),
        ("file.next", {"type": 'RIGHT_ARROW', "value": 'PRESS', "alt": True}, None),
        ("file.next", {"type": 'RIGHT_ARROW', "value": 'PRESS', "ctrl": True}, None),
        ("file.next", {"type": 'BUTTON5MOUSE', "value": 'PRESS'}, None),
        ("file.next", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "ctrl": True}, None),
        ("wm.context_set_enum", {"type": 'ONE', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.params.display_type"), ("value", 'LIST_VERTICAL')]}),
        ("wm.context_set_enum", {"type": 'TWO', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.params.display_type"), ("value", 'LIST_HORIZONTAL')]}),
        ("wm.context_set_enum", {"type": 'THREE', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.params.display_type"), ("value", 'THUMBNAIL')]}),
        # The two refresh operators have polls excluding each other (so only one is available depending on context).
        ("file.refresh", {"type": 'R', "value": 'PRESS', "ctrl": True}, None),
        ("asset.library_refresh", {"type": 'R', "value": 'PRESS', "ctrl": True}, None),
        ("asset.library_reload_listing", {"type": 'R', "value": 'PRESS', "shift": True}, None),
        ("file.previous", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("file.next", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True}, None),
        ("wm.context_toggle", {"type": 'H', "value": 'PRESS'},
         {"properties": [("data_path", "space_data.params.show_hidden")]}),
        ("wm.context_toggle", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.params.show_hidden")]}),
        ("wm.context_toggle", {"type": 'PERIOD', "value": 'PRESS', "ctrl": True, "shift": True},
         {"properties": [("data_path", "space_data.params.show_hidden")]}),
        ("file.directory_new", {"type": 'N', "value": 'PRESS', "ctrl": True, "shift": True},
         {"properties": [("confirm", False)]}),
        ("file.rename", {"type": 'F2', "value": 'PRESS'}, None),
        ("file.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("file.smoothscroll", {"type": 'TIMER1', "value": 'ANY', "any": True}, None),
        ("wm.context_toggle", {"type": 'T', "value": 'PRESS'},
            {"properties": [("data_path", "space_data.show_region_toolbar")]}),
        ("file.bookmark_add", {"type": 'B', "value": 'PRESS', "ctrl": True}, None),
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "repeat": True},
         {"properties": [("increment", 1)]}),
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("increment", 10)]}),
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("increment", 100)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "repeat": True},
         {"properties": [("increment", -1)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("increment", -10)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("increment", -100)]}),

        # Select file under cursor before spawning the context menu.
        ("file.select", {"type": 'RIGHTMOUSE', "value": 'PRESS'},
         {"properties": [("open", False), ("only_activate_if_selected", True), ("pass_through", True)]}),
        *_template_items_context_menu("FILEBROWSER_MT_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
    ])

    return keymap


def km_file_browser_main(params):
    items = []
    keymap = (
        "File Browser Main",
        {"space_type": 'FILE_BROWSER', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("file.mouse_execute", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        # The two refresh operators have polls excluding each other (so only one is available depending on context).
        ("file.refresh", {"type": 'R', "value": 'PRESS', "ctrl": True}, None),
        ("asset.library_refresh", {"type": 'R', "value": 'PRESS', "ctrl": True}, None),
        ("asset.library_reload_listing", {"type": 'R', "value": 'PRESS', "shift": True}, None),
        ("file.select", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("open", False), ("deselect_all", True)]}),
        ("file.select", {"type": 'LEFTMOUSE', "value": 'CLICK', "ctrl": True},
         {"properties": [("extend", True), ("open", False)]}),
        ("file.select", {"type": 'LEFTMOUSE', "value": 'CLICK', "shift": True, },
         {"properties": [("extend", True), ("fill", True), ("open", False)]}),
        ("file.select", {"type": 'RIGHTMOUSE', "value": 'CLICK', "shift": True},
         {"properties": [("extend", True), ("open", False)]}),
        ("file.select", {"type": 'RIGHTMOUSE', "value": 'CLICK', "alt": True},
         {"properties": [("extend", True), ("fill", True), ("open", False)]}),
        ("file.select_walk", {"type": 'UP_ARROW', "value": 'PRESS'},
         {"properties": [("direction", 'UP')]}),
        ("file.select_walk", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'UP'), ("extend", True)]}),
        ("file.select_walk", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("direction", 'UP'), ("extend", True), ("fill", True)]}),
        ("file.select_walk", {"type": 'DOWN_ARROW', "value": 'PRESS'},
         {"properties": [("direction", 'DOWN')]}),
        ("file.select_walk", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'DOWN'), ("extend", True)]}),
        ("file.select_walk", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("direction", 'DOWN'), ("extend", True), ("fill", True)]}),
        ("file.select_walk", {"type": 'LEFT_ARROW', "value": 'PRESS'},
         {"properties": [("direction", 'LEFT')]}),
        ("file.select_walk", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'LEFT'), ("extend", True)]}),
        ("file.select_walk", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("direction", 'LEFT'), ("extend", True), ("fill", True)]}),
        ("file.select_walk", {"type": 'RIGHT_ARROW', "value": 'PRESS'},
         {"properties": [("direction", 'RIGHT')]}),
        ("file.select_walk", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'RIGHT'), ("extend", True)]}),
        ("file.select_walk", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("direction", 'RIGHT'), ("extend", True), ("fill", True)]}),
        ("file.previous", {"type": 'BUTTON4MOUSE', "value": 'CLICK'}, None),
        ("file.next", {"type": 'BUTTON5MOUSE', "value": 'CLICK'}, None),
        ("file.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        ("file.select_box", {"type": 'Q', "value": 'PRESS'}, None),
        ("file.select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, None),
        ("file.select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "shift": True},
         {"properties": [("mode", 'ADD')]}),
        ("file.highlight", {"type": 'MOUSEMOVE', "value": 'ANY', "any": True}, None),
        ("file.sort_column_ui_context", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("file.view_selected", {"type": 'F', "value": 'PRESS'}, None),
        *_template_items_context_menu("ASSETBROWSER_MT_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
    ])

    return keymap


def km_file_browser_buttons(params):
    items = []
    keymap = (
        "File Browser Buttons",
        {"space_type": 'FILE_BROWSER', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "repeat": True},
         {"properties": [("increment", 1)]}),
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("increment", 10)]}),
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("increment", 100)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "repeat": True},
         {"properties": [("increment", -1)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("increment", -10)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("increment", -100)]}),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Animation


def km_frames(params):
    items = []
    keymap = (
        "Frames",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Frame offsets
        ("screen.frame_jump", {"type": 'MEDIA_LAST', "value": 'PRESS'},
         {"properties": [("end", True)]}),
        ("screen.frame_jump", {"type": 'MEDIA_FIRST', "value": 'PRESS'},
         {"properties": [("end", False)]}),
        ("screen.animation_play", {"type": 'SPACE', "value": 'PRESS'}, None),
        ("screen.animation_cancel", {"type": 'ESC', "value": 'PRESS'}, None),
        ("screen.animation_play", {"type": 'MEDIA_PLAY', "value": 'PRESS'}, None),
        ("screen.animation_cancel", {"type": 'MEDIA_STOP', "value": 'PRESS'}, None),
    ])

    return keymap


def km_animation(params):
    items = []
    keymap = (
        "Animation",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("wm.context_toggle", {"type": 'T', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.show_seconds")]}),
    ])

    return keymap


def km_animation_channels(params):
    items = []
    keymap = (
        "Animation Channels",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Click select.
        ("anim.channels_click", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("anim.channels_click", {"type": 'LEFTMOUSE', "value": 'CLICK', "shift": True},
         {"properties": [("extend_range", True)]}),
        ("anim.channels_click", {"type": 'LEFTMOUSE', "value": 'CLICK', "ctrl": True},
         {"properties": [("extend", True)]}),
        ("anim.channels_click", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("children_only", True)]}),
        # Rename.
        ("anim.channels_rename", {"type": 'RET', "value": 'PRESS'}, None),
        ("anim.channels_rename", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        # Select keys.
        ("anim.channel_select_keys", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("anim.channel_select_keys", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "shift": True},
         {"properties": [("extend", True)]}),
        # Find (setting the name filter).
        ("anim.channels_select_filter", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        # Selection.
        ("anim.channels_select_all", {"type": 'A', "value": 'PRESS',
         "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("anim.channels_select_all", {"type": 'A', "value": 'PRESS',
         "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("anim.channels_select_all", {"type": 'I', "value": 'PRESS',
         "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("anim.channels_select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, None),
        ("anim.channels_select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "shift": True, },
         {"properties": [("extend", True)]}),
        ("anim.channels_select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "ctrl": True, },
         {"properties": [("deselect", True)]}),
        # Delete.
        ("anim.channels_delete", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("anim.channels_delete", {"type": 'DEL', "value": 'PRESS'}, None),
        # Settings.
        ("anim.channels_setting_toggle", {"type": 'W', "value": 'PRESS', "shift": True}, None),
        ("anim.channels_setting_enable", {"type": 'W', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("anim.channels_setting_disable", {"type": 'W', "value": 'PRESS', "alt": True}, None),
        ("anim.channels_editable_toggle", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        # Expand/collapse.
        ("anim.channels_expand", {"type": 'RIGHT_ARROW', "value": 'PRESS'}, None),
        ("anim.channels_collapse", {"type": 'LEFT_ARROW', "value": 'PRESS'}, None),
        ("anim.channels_expand", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True},
         {"properties": [("all", False)]}),
        ("anim.channels_collapse", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True},
         {"properties": [("all", False)]}),
        # Move.
        ("anim.channels_move", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'UP')]}),
        ("anim.channels_move", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'DOWN')]}),
        ("anim.channels_move", {"type": 'PAGE_UP', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'TOP')]}),
        ("anim.channels_move", {"type": 'PAGE_DOWN', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'BOTTOM')]}),
        # Group.
        ("anim.channels_group", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("anim.channels_ungroup", {"type": 'G', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        # Menus.
        *_template_items_context_menu("DOPESHEET_MT_channel_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Modes


def km_face_mask(params):
    items = []
    keymap = (
        "Paint Face Mask (Weight, Vertex, Texture)",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Click Selection
        ("view3d.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("deselect_all", True)]}),
        ("view3d.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True, "shift": True, "alt": True},
         {"properties": [("toggle", True)]}),
        # Selection Operators
        ("paint.face_select_all", {"type": 'A', "value": 'PRESS',
         "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("paint.face_select_all", {"type": 'A', "value": 'PRESS', "ctrl": True,
         "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("paint.face_select_all", {"type": 'I', "value": 'PRESS',
         "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("paint.face_select_hide", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("paint.face_select_hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("paint.face_vert_reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("paint.face_select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("paint.face_select_linked_pick", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "alt": True},
         {"properties": [("deselect", False)]}),
        ("paint.face_select_linked_pick", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "ctrl": True, "alt": True},
         {"properties": [("deselect", True)]}),
    ])

    return keymap


def km_weight_paint_vertex_selection(params):
    items = []
    keymap = (
        "Paint Vertex Selection (Weight, Vertex)",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Click Selection
        ("view3d.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("deselect_all", True)]}),
        ("view3d.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True, "shift": True, "alt": True},
         {"properties": [("toggle", True)]}),
        # Selection Operators
        ("paint.vert_select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        ("paint.vert_select_hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("paint.face_vert_reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("paint.vert_select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("paint.vert_select_linked_pick", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "alt": True},
         {"properties": [("select", True)]}),
        ("paint.vert_select_linked_pick", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "ctrl": True, "alt": True},
         {"properties": [("select", False)]}),
    ])

    return keymap


def km_pose(params):
    items = []
    keymap = (
        "Pose",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_items_animation(),
        ("object.parent_set", {"type": 'P', "value": 'PRESS'}, None),
        ("pose.hide", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("pose.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("pose.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("pose.rot_clear", {"type": 'E', "value": 'PRESS', "alt": True}, None),
        ("pose.loc_clear", {"type": 'W', "value": 'PRESS', "alt": True}, None),
        ("pose.scale_clear", {"type": 'R', "value": 'PRESS', "alt": True}, None),
        ("pose.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("pose.paste", {"type": 'V', "value": 'PRESS', "ctrl": True},
         {"properties": [("flipped", False)]}),
        ("pose.paste", {"type": 'V', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("flipped", True)]}),
        ("pose.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("pose.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True,
         "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("pose.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("pose.select_parent", {"type": 'UP_ARROW', "value": 'PRESS', "ctrl": True}, None),
        ("pose.select_hierarchy", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'PARENT'), ("extend", False)]}),
        ("pose.select_hierarchy", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'PARENT'), ("extend", True)]}),
        ("pose.select_hierarchy", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'CHILD'), ("extend", False)]}),
        ("pose.select_hierarchy", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'CHILD'), ("extend", True)]}),
        ("pose.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("anim.keyframe_insert", {"type": 'S', "value": 'PRESS', "shift": True}, None),
        ("anim.keyframe_insert_by_name", {"type": 'S', "value": 'PRESS'},
         {"properties": [("type", 'LocRotScale')]}),
        ("anim.keyframe_insert_by_name", {"type": 'W', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'Location')]}),
        ("anim.keyframe_insert_by_name", {"type": 'E', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'Rotation')]}),
        ("anim.keyframe_insert_by_name", {"type": 'R', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'Scaling')]}),

        ("anim.keyframe_delete_v3d", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        ("anim.keying_set_active_set", {"type": 'S', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
        *_template_items_context_menu("VIEW3D_MT_pose_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        op_menu("POSE_MT_selection_sets_select", {"type": 'W', "value": 'PRESS', "shift": True, "alt": True}),
        # Tools
        op_tool_cycle("builtin.select_box", {"type": 'Q', "value": 'PRESS'}),
        op_tool_cycle("builtin.move", {"type": 'W', "value": 'PRESS'}),
        op_tool_cycle("builtin.rotate", {"type": 'E', "value": 'PRESS'}),
        op_tool_cycle("builtin.scale", {"type": 'R', "value": 'PRESS'}),
        op_tool_cycle("builtin.transform", {"type": 'T', "value": 'PRESS'}),
        op_tool_cycle("builtin.measure", {"type": 'M', "value": 'PRESS'}),
    ])

    return keymap


def km_object_mode(params):
    items = []
    keymap = (
        "Object Mode",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_items_animation(),
        # Selection
        ("object.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("object.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True,
         "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("object.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("object.select_more", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("object.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("object.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("object.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'PARENT'), ("extend", False)]}),
        ("object.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'PARENT'), ("extend", True)]}),
        ("object.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'CHILD'), ("extend", False)]}),
        ("object.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'CHILD'), ("extend", True)]}),
        ("object.parent_set", {"type": 'P', "value": 'PRESS'}, None),

        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", "tool_settings.use_proportional_edit_objects")]}),
        ("object.parent_clear", {"type": 'P', "value": 'PRESS', "shift": True}, None),
        ("object.location_clear", {"type": 'W', "value": 'PRESS', "alt": True},
         {"properties": [("clear_delta", False)]}),
        ("object.rotation_clear", {"type": 'E', "value": 'PRESS', "alt": True},
         {"properties": [("clear_delta", False)]}),
        ("object.scale_clear", {"type": 'R', "value": 'PRESS', "alt": True},
         {"properties": [("clear_delta", False)]}),
        ("object.delete", {"type": 'BACK_SPACE', "value": 'PRESS'},
         {"properties": [("use_global", False), ("confirm", False)]}),
        ("object.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True},
         {"properties": [("use_global", True), ("confirm", False)]}),
        ("object.delete", {"type": 'DEL', "value": 'PRESS'},
         {"properties": [("use_global", False), ("confirm", False)]}),
        ("object.delete", {"type": 'DEL', "value": 'PRESS', "shift": True},
         {"properties": [("use_global", True), ("confirm", False)]}),
        ("object.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        # Keyframing
        ("anim.keyframe_insert", {"type": 'S', "value": 'PRESS', "shift": True}, None),
        ("anim.keyframe_insert_by_name", {"type": 'S', "value": 'PRESS'},
         {"properties": [("type", 'LocRotScale')]}),
        ("anim.keyframe_insert_by_name", {"type": 'W', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'Location')]}),
        ("anim.keyframe_insert_by_name", {"type": 'E', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'Rotation')]}),
        ("anim.keyframe_insert_by_name", {"type": 'R', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'Scaling')]}),
        ("anim.keyframe_delete_v3d", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        ("anim.keying_set_active_set", {"type": 'S', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
        *_template_items_context_menu("VIEW3D_MT_object_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        op_menu("OBJECT_MT_move_to_collection", {"type": 'G', "value": 'PRESS', "ctrl": True}),
        op_menu("OBJECT_MT_link_to_collection", {"type": 'G', "value": 'PRESS', "shift": True, "ctrl": True}),
        ("object.hide_view_clear", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("object.hide_view_set", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("object.hide_view_set", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),

        *_template_items_basic_tools(),

        # Selection Modes
        ("object.mode_set_with_submode", {"type": 'ONE', "value": 'PRESS'},
         {"properties": [("mode", 'EDIT'), ("mesh_select_mode", {'VERT'})]}),
        ("object.mode_set_with_submode", {"type": 'TWO', "value": 'PRESS'},
         {"properties": [("mode", 'EDIT'), ("mesh_select_mode", {'EDGE'})]}),
        ("object.mode_set_with_submode", {"type": 'THREE', "value": 'PRESS'},
         {"properties": [("mode", 'EDIT'), ("mesh_select_mode", {'FACE'})]}),
    ])

    return keymap


def km_paint_curve(params):
    items = []
    keymap = (
        "Paint Curve",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("paintcurve.add_point_slide", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("paintcurve.select", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("extend", False)]}),
        ("paintcurve.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("paintcurve.slide", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("align", False)]}),
        ("paintcurve.slide", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("align", True)]}),
        ("paintcurve.select", {"type": 'A', "value": 'PRESS'},
         {"properties": [("toggle", True)]}),
        ("paintcurve.cursor", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("paintcurve.delete_point", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("paintcurve.delete_point", {"type": 'DEL', "value": 'PRESS'}, None),
        ("paintcurve.draw", {"type": 'RET', "value": 'PRESS'}, None),
        ("paintcurve.draw", {"type": 'NUMPAD_ENTER', "value": 'PRESS'}, None),
        ("transform.translate", {"type": 'W', "value": 'PRESS'}, None),
        ("transform.translate", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, None),
        ("transform.rotate", {"type": 'E', "value": 'PRESS'}, None),
        ("transform.resize", {"type": 'R', "value": 'PRESS'}, None),
    ])

    return keymap


def km_curve(params):
    items = []
    keymap = (
        "Curve",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("curve.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("curve.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True,
         "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("curve.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("curve.select_row", {"type": 'R', "value": 'PRESS', "shift": True}, None),
        ("curve.select_more", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("curve.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("curve.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("curve.shortest_path_pick", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True, "shift": True}, None),
        ("curve.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        op_menu("VIEW3D_MT_edit_curve_delete", {"type": 'BACK_SPACE', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_edit_curve_delete", {"type": 'DEL', "value": 'PRESS'}),
        ("curve.dissolve_verts", {"type": 'BACK_SPACE', "value": 'PRESS', "ctrl": True}, None),
        ("curve.dissolve_verts", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        ("curve.tilt_clear", {"type": 'T', "value": 'PRESS', "alt": True}, None),
        ("curve.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("curve.hide", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("curve.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        *_template_items_context_menu("VIEW3D_MT_edit_curve_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", "tool_settings.use_proportional_edit")]}),
        # Tools
        *_template_items_basic_tools(),
        op_tool_cycle("builtin.extrude", {"type": 'E', "value": 'PRESS', "ctrl": True}),
        op_tool_cycle("builtin.tilt", {"type": 'Y', "value": 'PRESS'}),
        op_tool_cycle("builtin.radius", {"type": 'U', "value": 'PRESS'}),

    ])

    return keymap

# Radial control setup helpers, this operator has a lot of properties.


def radial_control_properties(paint, prop, secondary_prop, secondary_rotation=False, color=False, zoom=False):
    brush_path = "tool_settings." + paint + ".brush"
    unified_path = "tool_settings." + paint + ".unified_paint_settings"
    rotation = "mask_texture_slot.angle" if secondary_rotation else "texture_slot.angle"
    return {
        "properties": [
            ("data_path_primary", brush_path + "." + prop),
            ("data_path_secondary", unified_path + "." + prop if secondary_prop else ""),
            ("use_secondary", brush_path + "." + secondary_prop if secondary_prop else ""),
            ("rotation_path", brush_path + "." + rotation),
            ("color_path", brush_path + ".cursor_color_add"),
            ("fill_color_path", brush_path + ".color" if color else ""),
            ("fill_color_override_path", unified_path + ".color" if color else ""),
            ("fill_color_override_test_path", brush_path + ".use_unified_color" if color else ""),
            ("zoom_path", "space_data.zoom" if zoom else ""),
            ("image_id", brush_path + ""),
            ("secondary_tex", secondary_rotation),
        ],
    }

# Radial controls for the paint and sculpt modes.


def _template_paint_radial_control(
        paint,
        rotation=False,
        secondary_rotation=False,
        color=False,
        zoom=False,
        weight=False,
):
    items = []

    items.extend([
        ("wm.radial_control", {"type": 'S', "value": 'PRESS'},
         radial_control_properties(
             paint, "size", "use_unified_size", secondary_rotation=secondary_rotation, color=color, zoom=zoom)),
        ("wm.radial_control", {"type": 'U', "value": 'PRESS'},
         radial_control_properties(
             paint, "strength", "use_unified_strength", secondary_rotation=secondary_rotation, color=color)),
    ])

    if rotation:
        items.extend([
            ("wm.radial_control", {"type": 'F', "value": 'PRESS', "ctrl": True},
             radial_control_properties(paint, "texture_slot.angle", None, color=color)),
        ])

    if secondary_rotation:
        items.extend([
            ("wm.radial_control", {"type": 'F', "value": 'PRESS', "ctrl": True, "alt": True},
             radial_control_properties(
                 paint, "mask_texture_slot.angle", None, secondary_rotation=secondary_rotation, color=color,
            )),
        ])

    if weight:
        items.extend([
            ("wm.radial_control", {"type": 'F', "value": 'PRESS', "ctrl": True, "alt": True},
             radial_control_properties(
                 paint, "mask_texture_slot.angle", None, secondary_rotation=secondary_rotation, color=color,
            )),
            ("wm.radial_control", {"type": 'F', "value": 'PRESS', "ctrl": True},
             radial_control_properties(
                paint, "weight", "use_unified_weight"))
        ])

    return items


def km_vertex_paint(params):
    items = []
    keymap = (
        "Vertex Paint",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Brush Strokes
        ("paint.vertex_paint", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("mode", 'NORMAL')]}),
        ("paint.vertex_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'INVERT')]}),
        ("paint.vertex_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("brush_toggle", 'SMOOTH')]}),
        # Colors
        ("paint.sample_color", {"type": 'I', "value": 'PRESS'}, {"properties": [("merged", False)]}),
        ("paint.sample_color", {"type": 'I', "value": 'PRESS', "shift": True}, {"properties": [("merged", True)]}),
        ("paint.brush_colors_flip", {"type": 'X', "value": 'PRESS'}, None),
        ("paint.vertex_color_set", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        # Brush properties
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 1.0 / 0.9)]}),
        *_template_paint_radial_control("vertex_paint", color=True, rotation=True),
        # Stencil Controls
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS'},
         {"properties": [("mode", 'TRANSLATION')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'SCALE')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'ROTATION')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'TRANSLATION'), ("texmode", 'SECONDARY')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("mode", 'SCALE'), ("texmode", 'SECONDARY')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("mode", 'ROTATION'), ("texmode", 'SECONDARY')]}),
        # Mask Modes
        ("wm.context_toggle", {"type": 'ONE', "value": 'PRESS'},
         {"properties": [("data_path", "vertex_paint_object.data.use_paint_mask")]}),
        ("wm.context_toggle", {"type": 'TWO', "value": 'PRESS'},
         {"properties": [("data_path", "vertex_paint_object.data.use_paint_mask_vertex")]}),
        # Stabilize Stroke
        ("wm.context_toggle", {"type": 'L', "value": 'PRESS'},
         {"properties": [("data_path", "tool_settings.vertex_paint.brush.use_smooth_stroke")]}),
        # Context menu.
        *_template_items_context_panel("VIEW3D_PT_paint_vertex_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        # Pie Menu
        op_menu_pie("VIEW3D_MT_mesh_paint_automasking_pie", {"type": 'A', "alt": True, "value": 'PRESS'}),
        # Tools
        op_tool_cycle("builtin.select_box", {"type": 'Q', "value": 'PRESS'}),
        op_tool_cycle("builtin.annotate", {"type": 'D', "value": 'PRESS'}),
        op_asset_shelf_popup("VIEW3D_AST_brush_vertex_paint", {"type": 'B', "value": 'PRESS'}),
    ])

    return keymap


def km_weight_paint(params):
    items = []
    keymap = (
        "Weight Paint",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Brush Strokes
        ("paint.weight_paint", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("mode", 'NORMAL')]}),
        ("paint.weight_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'INVERT')]}),
        ("paint.weight_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("brush_toggle", 'SMOOTH')]}),
        # Weight
        ("paint.weight_sample", {"type": 'I', "value": 'PRESS'}, None),
        ("paint.weight_sample_group", {"type": 'I', "value": 'PRESS', "alt": True}, None),
        ("paint.weight_set", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        # Vertex Lock Pie
        op_menu_pie("VIEW3D_MT_wpaint_vgroup_lock_pie", {"type": 'I', "value": 'PRESS', "ctrl": True, "alt": True}),
        # Brush properties
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 1.0 / 0.9)]}),
        *_template_paint_radial_control("weight_paint", weight=True),
        # Mask Modes
        ("wm.context_toggle", {"type": 'ONE', "value": 'PRESS'},
         {"properties": [("data_path", "weight_paint_object.data.use_paint_mask")]}),
        ("wm.context_toggle", {"type": 'TWO', "value": 'PRESS'},
         {"properties": [("data_path", "weight_paint_object.data.use_paint_mask_vertex")]}),
        # Stabilize Stroke
        ("wm.context_toggle", {"type": 'L', "value": 'PRESS'},
         {"properties": [("data_path", "tool_settings.weight_paint.brush.use_smooth_stroke")]}),
        # Context menu.
        *_template_items_context_panel("VIEW3D_PT_paint_weight_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        # Pie Menu
        op_menu_pie("VIEW3D_MT_mesh_paint_automasking_pie", {"type": 'A', "alt": True, "value": 'PRESS'}),
        # For combined weight paint + pose mode.
        ("view3d.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "alt": True}, None),
        op_tool_cycle("builtin.move", {"type": 'W', "value": 'PRESS'}),
        op_tool_cycle("builtin.rotate", {"type": 'E', "value": 'PRESS'}),
        op_tool_cycle("builtin.scale", {"type": 'R', "value": 'PRESS'}),
        op_tool_cycle("builtin.transform", {"type": 'T', "value": 'PRESS'}),
        # Tools
        op_tool_cycle("builtin.cursor", {"type": 'C', "value": 'PRESS'}),
        op_tool_cycle("builtin.select_box", {"type": 'Q', "value": 'PRESS'}),
        op_tool_cycle("builtin.annotate", {"type": 'D', "value": 'PRESS'}),
        op_asset_shelf_popup("VIEW3D_AST_brush_weight_paint", {"type": 'B', "value": 'PRESS'}),
    ])

    return keymap


def km_sculpt(params):
    items = []
    keymap = (
        "Sculpt",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Sculpt Pivot
        ("sculpt.set_pivot_position", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'SURFACE')]}),
        # Brush strokes
        ("sculpt.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("mode", 'NORMAL')]}),
        ("sculpt.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'INVERT')]}),
        ("sculpt.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("brush_toggle", 'SMOOTH')]}),
        ("sculpt.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("brush_toggle", 'MASK')]}),
        ("sculpt.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("mode", 'INVERT'), ("brush_toggle", 'MASK')]}),
        # Expand
        ("sculpt.expand", {"type": 'A', "value": 'PRESS', "shift": True},
         {"properties": [
             ("target", "MASK"),
             ("falloff_type", "GEODESIC"),
             ("invert", False),
             ("use_auto_mask", False),
             ("use_mask_preserve", True),
         ]}),
        ("sculpt.expand", {"type": 'A', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [
             ("target", "MASK"),
             ("falloff_type", "NORMALS"),
             ("invert", False),
             ("use_mask_preserve", True),
         ]}),
        ("sculpt.expand", {"type": 'W', "value": 'PRESS', "shift": True},
         {"properties": [
             ("target", "FACE_SETS"),
             ("falloff_type", "GEODESIC"),
             ("invert", False),
             ("use_mask_preserve", False),
             ("use_modify_active", False),
         ]}),
        ("sculpt.expand", {"type": 'W', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [
             ("target", "FACE_SETS"),
             ("falloff_type", "BOUNDARY_FACE_SET"),
             ("invert", False),
             ("use_mask_preserve", False),
             ("use_modify_active", True),
         ]}),
        # Visibility
        ("sculpt.face_set_change_visibility", {"type": 'H', "value": 'PRESS'},
         {"properties": [("mode", 'HIDE_ACTIVE')]}),
        ("sculpt.face_set_change_visibility", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'TOGGLE')]}),
        ("paint.hide_show_masked", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("action", 'HIDE')]}),
        ("paint.hide_show_all", {"type": 'H', "value": 'PRESS', "alt": True},
         {"properties": [("action", 'SHOW')]}),
        # Subdivision levels
        *_template_items_object_subdivision_set(),
        ("object.subdivision_set", {"type": 'D', "value": 'PRESS', "repeat": True},
         {"properties": [("level", 1), ("relative", True), ("ensure_modifier", False)]}),
        ("object.subdivision_set", {"type": 'D', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("level", -1), ("relative", True), ("ensure_modifier", False)]}),
        # Mask
        ("paint.mask_flood_fill", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True},
         {"properties": [("mode", 'VALUE'), ("value", 1.0)]}),
        ("paint.mask_flood_fill", {"type": 'I', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'INVERT')]}),
        # Face Sets
        ("sculpt.face_set_edit", {"type": 'PAGE_UP', "value": 'PRESS'},
         {"properties": [("mode", 'GROW')]}),
        ("sculpt.face_set_edit", {"type": 'PAGE_DOWN', "value": 'PRESS'},
         {"properties": [("mode", 'SHRINK')]}),
        # Dynamic topology
        ("sculpt.detail_flood_fill", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("sculpt.dyntopo_detail_size_edit", {"type": 'D', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        # Remesh
        ("object.voxel_remesh", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("object.voxel_size_edit", {"type": 'D', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        # Color
        ("paint.sample_color", {"type": 'I', "value": 'PRESS'}, {"properties": [("merged", False)]}),
        ("paint.sample_color", {"type": 'I', "value": 'PRESS', "shift": True}, {"properties": [("merged", True)]}),
        ("paint.brush_colors_flip", {"type": 'X', "value": 'PRESS'}, None),
        # Brush properties
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 1.0 / 0.9)]}),
        *_template_paint_radial_control("sculpt", rotation=True),
        # Stencil
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS'},
         {"properties": [("mode", 'TRANSLATION')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'SCALE')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'ROTATION')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'TRANSLATION'), ("texmode", 'SECONDARY')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("mode", 'SCALE'), ("texmode", 'SECONDARY')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("mode", 'ROTATION'), ("texmode", 'SECONDARY')]}),
        # Stabilize Stroke
        ("wm.context_toggle", {"type": 'L', "value": 'PRESS'},
         {"properties": [("data_path", "tool_settings.sculpt.brush.use_smooth_stroke")]}),
        # Tools
        # This is the only mode without an Annotate shortcut.
        # The multi-resolution shortcuts took precedence instead.
        op_tool_cycle("builtin.box_mask", {"type": 'Q', "value": 'PRESS'}),
        op_tool_cycle("builtin.move", {"type": 'W', "value": 'PRESS'}),
        op_tool_cycle("builtin.rotate", {"type": 'E', "value": 'PRESS'}),
        op_tool_cycle("builtin.scale", {"type": 'R', "value": 'PRESS'}),
        op_tool_cycle("builtin.transform", {"type": 'T', "value": 'PRESS'}),
        # Menus
        op_menu_pie("VIEW3D_MT_sculpt_mask_edit_pie", {"type": 'A', "ctrl": True, "value": 'PRESS'}),
        op_menu_pie("VIEW3D_MT_mesh_paint_automasking_pie", {"type": 'A', "alt": True, "value": 'PRESS'}),
        op_menu_pie("VIEW3D_MT_sculpt_face_sets_edit_pie", {"type": 'W', "ctrl": True, "value": 'PRESS'}),
        *_template_items_context_panel("VIEW3D_PT_sculpt_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        op_asset_shelf_popup("VIEW3D_AST_brush_sculpt", {"type": 'B', "value": 'PRESS'}),
    ])

    return keymap


# Mesh edit mode.
def km_mesh(params):
    items = []
    keymap = (
        "Mesh",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Selection
        ("mesh.loop_select", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'},
         {"properties": [("extend", False), ("deselect", False), ("toggle", False)]}),
        ("mesh.loop_select", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "shift": True},
         {"properties": [("extend", True), ("deselect", False), ("toggle", False)]}),
        ("mesh.loop_select", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "ctrl": True},
         {"properties": [("extend", False), ("deselect", True), ("toggle", False)]}),

        ("mesh.edgering_select", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "alt": True},
         {"properties": [("extend", False), ("deselect", False), ("toggle", False)]}),
        ("mesh.edgering_select", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "alt": True, "shift": True},
         {"properties": [("extend", True), ("deselect", False), ("toggle", False)]}),
        ("mesh.edgering_select", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "alt": True, "ctrl": True},
         {"properties": [("extend", False), ("deselect", True), ("toggle", False)]}),

        ("mesh.shortest_path_pick", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("use_fill", False)]}),
        ("mesh.shortest_path_pick", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("use_fill", True)]}),

        ("mesh.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("mesh.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True,
         "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("mesh.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("mesh.select_more", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("mesh.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("mesh.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),

        *_template_items_editmode_mesh_select_mode(params),

        # Hide/reveal.
        ("mesh.hide", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("mesh.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("mesh.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        # Tools.
        ("mesh.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        op_menu("VIEW3D_MT_edit_mesh_delete", {"type": 'BACK_SPACE', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_edit_mesh_delete", {"type": 'DEL', "value": 'PRESS'}),
        ("mesh.dissolve_mode", {"type": 'BACK_SPACE', "value": 'PRESS', "ctrl": True}, None),
        ("mesh.dissolve_mode", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", "tool_settings.use_proportional_edit")]}),
        # Menus.
        *_template_items_context_menu("VIEW3D_MT_edit_mesh_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        # Tools
        *_template_items_basic_tools(),
        op_tool_cycle("builtin.bevel", {"type": 'B', "value": 'PRESS', "ctrl": True}),
        op_tool_cycle("builtin.inset_faces", {"type": 'I', "value": 'PRESS'}),
        op_tool_cycle("builtin.extrude_region", {"type": 'E', "value": 'PRESS', "ctrl": True}),
        op_tool_cycle("builtin.knife", {"type": 'K', "value": 'PRESS'}),
        op_tool_cycle("builtin.loop_cut", {"type": 'C', "value": 'PRESS', "alt": True}),

    ])

    return keymap


# Armature edit mode
def km_armature(params):
    items = []
    keymap = (
        "Armature",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Hide/reveal.
        ("armature.hide", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("armature.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("armature.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        # Parenting.
        ("armature.parent_set", {"type": 'P', "value": 'PRESS'}, None),
        ("armature.parent_clear", {"type": 'P', "value": 'PRESS', "shift": True}, None),
        # Selection.
        ("armature.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("armature.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True,
         "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("armature.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),

        ("armature.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("direction", 'PARENT'), ("extend", False)]}),
        ("armature.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'PARENT'), ("extend", True)]}),
        ("armature.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("direction", 'CHILD'), ("extend", False)]}),
        ("armature.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'CHILD'), ("extend", True)]}),

        ("armature.select_more", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("armature.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True}, None),

        ("armature.select_similar", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        ("armature.select_linked_pick", {"type": 'L', "value": 'PRESS', "ctrl": True},
         {"properties": [("deselect", False)]}),

        ("armature.shortest_path_pick", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True, "shift": True}, None),

        # Editing.
        op_menu("VIEW3D_MT_edit_armature_delete", {"type": 'DEL', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_edit_armature_delete", {"type": 'BACK_SPACE', "value": 'PRESS'}),
        ("armature.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("armature.dissolve", {"type": 'BACK_SPACE', "value": 'PRESS', "ctrl": True}, None),
        ("armature.dissolve", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        # Menus.
        *_template_items_context_menu("VIEW3D_MT_armature_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        # Tools.
        *_template_items_basic_tools(),
        op_tool_cycle("builtin.roll", {"type": 'Y', "value": 'PRESS'}),
        op_tool_cycle("builtin.extrude", {"type": 'E', "value": 'PRESS', "ctrl": True}),

    ])

    return keymap


# Meta-ball edit mode.
def km_metaball(params):
    items = []
    keymap = (
        "Metaball",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("mball.reveal_metaelems", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("mball.hide_metaelems", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("mball.hide_metaelems", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("mball.delete_metaelems", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("mball.delete_metaelems", {"type": 'DEL', "value": 'PRESS'}, None),
        ("mball.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("mball.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("mball.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True,
         "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("mball.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("mball.select_similar", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        *_template_items_context_menu("VIEW3D_MT_edit_metaball_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", "tool_settings.use_proportional_edit")]}),
        # Tools
        *_template_items_basic_tools(),
    ])

    return keymap


# Lattice edit mode.
def km_lattice(params):
    items = []
    keymap = (
        "Lattice",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("lattice.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("lattice.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True,
         "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("lattice.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("lattice.select_more", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("lattice.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("object.vertex_parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        *_template_items_context_menu("VIEW3D_MT_edit_lattice_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", "tool_settings.use_proportional_edit")]}),
        # Tools
        op_tool_cycle("builtin.select_box", {"type": 'Q', "value": 'PRESS'}),
        op_tool_cycle("builtin.move", {"type": 'W', "value": 'PRESS'}),
        op_tool_cycle("builtin.rotate", {"type": 'E', "value": 'PRESS'}),
        op_tool_cycle("builtin.scale", {"type": 'R', "value": 'PRESS'}),
        op_tool_cycle("builtin.transform", {"type": 'T', "value": 'PRESS'}),
        op_tool_cycle("builtin.measure", {"type": 'M', "value": 'PRESS'}),
    ])

    return keymap


# Particle edit mode.
def km_particle(params):
    items = []
    keymap = (
        "Particle",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("particle.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("particle.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True,
         "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("particle.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("particle.select_more", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("particle.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("particle.select_linked_pick", {"type": 'L', "value": 'PRESS', "ctrl": True},
         {"properties": [("deselect", False)]}),
        ("particle.select_linked_pick", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "alt": True},
         {"properties": [("deselect", True)]}),
        ("particle.select_linked", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "shift": True, "alt": True}, None),
        ("particle.delete", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("particle.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("particle.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("particle.hide", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("particle.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("particle.brush_edit", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("particle.brush_edit", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True}, None),
        ("wm.radial_control", {"type": 'S', "value": 'PRESS'},
         {"properties": [("data_path_primary", "tool_settings.particle_edit.brush.size")]}),
        ("wm.radial_control", {"type": 'U', "value": 'PRESS'},
         {"properties": [("data_path_primary", "tool_settings.particle_edit.brush.strength")]}),
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", "tool_settings.use_proportional_edit")]}),
        *_template_items_context_menu("VIEW3D_MT_particle_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
    ])

    return keymap


# Text edit mode.
def km_font(params):
    items = []
    keymap = (
        "Font",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("font.style_toggle", {"type": 'B', "value": 'PRESS', "ctrl": True},
         {"properties": [("style", 'BOLD')]}),
        ("font.style_toggle", {"type": 'I', "value": 'PRESS', "ctrl": True},
         {"properties": [("style", 'ITALIC')]}),
        ("font.style_toggle", {"type": 'U', "value": 'PRESS', "ctrl": True},
         {"properties": [("style", 'UNDERLINE')]}),
        ("font.style_toggle", {"type": 'P', "value": 'PRESS', "ctrl": True},
         {"properties": [("style", 'SMALL_CAPS')]}),
        ("font.delete", {"type": 'DEL', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'NEXT_OR_SELECTION')]}),
        ("font.delete", {"type": 'DEL', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("font.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'PREVIOUS_OR_SELECTION')]}),
        ("font.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_OR_SELECTION')]}),
        ("font.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("font.move", {"type": 'HOME', "value": 'PRESS'},
         {"properties": [("type", 'LINE_BEGIN')]}),
        ("font.move", {"type": 'END', "value": 'PRESS'},
         {"properties": [("type", 'LINE_END')]}),
        ("font.move", {"type": 'LEFT_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("font.move", {"type": 'RIGHT_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'NEXT_CHARACTER')]}),
        ("font.move", {"type": 'LEFT_ARROW', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("font.move", {"type": 'RIGHT_ARROW', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("font.move", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'PREVIOUS_LINE')]}),
        ("font.move", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'NEXT_LINE')]}),
        ("font.move", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'PREVIOUS_PAGE')]}),
        ("font.move", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'NEXT_PAGE')]}),
        ("font.move", {"type": 'HOME', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'TEXT_BEGIN')]}),
        ("font.move", {"type": 'END', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'TEXT_END')]}),
        ("font.move_select", {"type": 'HOME', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'LINE_BEGIN')]}),
        ("font.move_select", {"type": 'END', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'LINE_END')]}),
        ("font.move_select", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("font.move_select", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'NEXT_CHARACTER')]}),
        ("font.move_select", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("font.move_select", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("font.move_select", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_LINE')]}),
        ("font.move_select", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'NEXT_LINE')]}),
        ("font.move_select", {"type": 'PAGE_UP', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_PAGE')]}),
        ("font.move_select", {"type": 'PAGE_DOWN', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'NEXT_PAGE')]}),
        ("font.move_select", {"type": 'HOME', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("type", 'TEXT_BEGIN')]}),
        ("font.move_select", {"type": 'END', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("type", 'TEXT_END')]}),
        ("font.change_spacing", {"type": 'LEFT_ARROW', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("delta", -1.0)]}),
        ("font.change_spacing", {"type": 'RIGHT_ARROW', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("delta", 1.0)]}),
        ("font.change_character", {"type": 'UP_ARROW', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("delta", 1)]}),
        ("font.change_character", {"type": 'DOWN_ARROW', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("delta", -1)]}),
        ("font.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        ("font.text_copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("font.text_cut", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("font.text_paste", {"type": 'V', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("font.line_break", {"type": 'RET', "value": 'PRESS'}, None),
        ("font.text_insert", {"type": 'TEXTINPUT', "value": 'ANY', "any": True}, None),
        ("font.text_insert", {"type": 'BACK_SPACE', "value": 'PRESS', "alt": True},
         {"properties": [("accent", True)]}),
        *_template_items_context_menu("VIEW3D_MT_edit_font_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
    ])

    return keymap


# Curves edit mode.
def km_curves(params):
    items = []
    keymap = (
        "Curves",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Selection Modes
        ("curves.set_selection_domain", {"type": 'ONE', "value": 'PRESS'}, {"properties": [("domain", 'POINT')]}),
        ("curves.set_selection_domain", {"type": 'TWO', "value": 'PRESS'}, {"properties": [("domain", 'CURVE')]}),
        ("curves.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        # Selection Operators
        ("curves.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("curves.select_all", {"type": 'A', "value": 'PRESS', "shift": True,
         "ctrl": True}, {"properties": [("action", 'DESELECT')]}),
        ("curves.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("curves.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("curves.select_more", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True}, None),
        ("curves.select_less", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True}, None),
        # Delete
        ("curves.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        # Proportional Editing
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", "tool_settings.use_proportional_edit")]}),
        # Tools
        *_template_items_basic_tools(),
        op_tool_cycle("builtin.annotate", {"type": 'D', "value": 'PRESS'}),
    ])

    return keymap


def km_sculpt_curves(params):
    items = []
    keymap = (
        "Sculpt Curves",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Brush strokes
        ("sculpt_curves.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("mode", 'NORMAL')]}),
        ("sculpt_curves.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'INVERT')]}),
        ("sculpt_curves.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("brush_toggle", 'SMOOTH')]}),
        # Selection modes
        ("curves.set_selection_domain", {"type": 'ONE', "value": 'PRESS'}, {"properties": [("domain", 'POINT')]}),
        ("curves.set_selection_domain", {"type": 'TWO', "value": 'PRESS'}, {"properties": [("domain", 'CURVE')]}),
        # Brush Properties
        *_template_paint_radial_control("curves_sculpt"),
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 1.0 / 0.9)]}),
        # Selection operators
        ("curves.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("curves.select_all", {"type": 'A', "value": 'PRESS', "shift": True,
         "ctrl": True}, {"properties": [("action", 'DESELECT')]}),
        ("curves.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("sculpt_curves.select_grow", {"type": 'A', "value": 'PRESS', "shift": True}, {}),
        # Density
        ("sculpt_curves.min_distance_edit", {"type": 'D', "value": 'PRESS', "ctrl": True}, {}),
        # Tools
        op_tool_cycle("builtin.annotate", {"type": 'D', "value": 'PRESS'}),
        op_asset_shelf_popup("VIEW3D_AST_brush_sculpt_curves", {"type": 'B', "value": 'PRESS'}),
    ])

    return keymap


# Point cloud edit mode.
def km_pointcloud(params):
    items = []
    keymap = (
        "Point Cloud",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Selection Operators
        ("pointcloud.select_all", {"type": 'A', "value": 'PRESS',
         "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("pointcloud.select_all", {"type": 'A', "value": 'PRESS', "shift": True,
         "ctrl": True}, {"properties": [("action", 'DESELECT')]}),
        ("pointcloud.select_all", {"type": 'I', "value": 'PRESS',
         "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        # Delete
        ("pointcloud.delete", {"type": 'DEL', "value": 'PRESS'}, None),
    ])

    return keymap


def km_object_non_modal(params):
    items = []
    keymap = (
        "Object Non-modal",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("object.transfer_mode", {"type": 'ACCENT_GRAVE', "value": 'PRESS'}, None),
        ("object.mode_set", {"type": 'FOUR', "value": 'PRESS'}, {"properties": [("mode", 'OBJECT')]}),
        op_menu_pie("VIEW3D_MT_object_mode_pie", {"type": 'FIVE', "value": 'PRESS'}),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Modal Maps


def km_knife_tool_modal_map(_params):
    items = []
    keymap = (
        "Knife Tool Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("PANNING", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("CONFIRM", {"type": 'RET', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "any": True}, None),
        ("ADD_CUT_CLOSED", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "any": True}, None),
        ("ADD_CUT", {"type": 'LEFTMOUSE', "value": 'ANY', "any": True}, None),
        ("UNDO", {"type": 'Z', "value": 'PRESS', "ctrl": True}, None),
        ("NEW_CUT", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
        ("SNAP_MIDPOINTS_ON", {"type": 'LEFT_CTRL', "value": 'PRESS'}, None),
        ("SNAP_MIDPOINTS_OFF", {"type": 'LEFT_CTRL', "value": 'RELEASE'}, None),
        ("SNAP_MIDPOINTS_ON", {"type": 'RIGHT_CTRL', "value": 'PRESS'}, None),
        ("SNAP_MIDPOINTS_OFF", {"type": 'RIGHT_CTRL', "value": 'RELEASE'}, None),
        ("IGNORE_SNAP_ON", {"type": 'LEFT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("IGNORE_SNAP_OFF", {"type": 'LEFT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("IGNORE_SNAP_ON", {"type": 'RIGHT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("IGNORE_SNAP_OFF", {"type": 'RIGHT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("X_AXIS", {"type": 'X', "value": 'PRESS'}, None),
        ("Y_AXIS", {"type": 'Y', "value": 'PRESS'}, None),
        ("Z_AXIS", {"type": 'Z', "value": 'PRESS'}, None),
        ("ANGLE_SNAP_TOGGLE", {"type": 'A', "value": 'PRESS'}, None),
        ("CYCLE_ANGLE_SNAP_EDGE", {"type": 'R', "value": 'PRESS'}, None),
        ("CUT_THROUGH_TOGGLE", {"type": 'C', "value": 'PRESS'}, None),
        ("PANNING", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "alt": True}, None),
        ("PANNING", {"type": 'RIGHTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("SHOW_DISTANCE_ANGLE_TOGGLE", {"type": 'D', "value": 'PRESS'}, None),
        ("DEPTH_TEST_TOGGLE", {"type": 'V', "value": 'PRESS'}, None),
    ])

    return keymap


def km_eyedropper_modal_map(_params):
    items = []
    keymap = (
        "Eyedropper Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
        ("SAMPLE_CONFIRM", {"type": 'RET', "value": 'RELEASE', "any": True}, None),
        ("SAMPLE_CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'RELEASE', "any": True}, None),
        ("SAMPLE_CONFIRM", {"type": 'LEFTMOUSE', "value": 'RELEASE', "any": True}, None),
        ("SAMPLE_BEGIN", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("SAMPLE_RESET", {"type": 'SPACE', "value": 'RELEASE', "any": True}, None),
    ])

    return keymap


def km_eyedropper_colorramp_pointsampling_map(_params):
    items = []
    keymap = (
        "Eyedropper ColorRamp PointSampling Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'BACK_SPACE', "value": 'PRESS', "any": True}, None),
        ("SAMPLE_CONFIRM", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
        ("SAMPLE_CONFIRM", {"type": 'RET', "value": 'RELEASE', "any": True}, None),
        ("SAMPLE_CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'RELEASE', "any": True}, None),
        ("SAMPLE_SAMPLE", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("SAMPLE_RESET", {"type": 'SPACE', "value": 'RELEASE', "any": True}, None),
    ])

    return keymap


def km_transform_modal_map(_params):
    items = []
    keymap = (
        "Transform Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'RET', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'SPACE', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("AXIS_X", {"type": 'X', "value": 'PRESS'}, None),
        ("AXIS_Y", {"type": 'Y', "value": 'PRESS'}, None),
        ("AXIS_Z", {"type": 'Z', "value": 'PRESS'}, None),
        ("PLANE_X", {"type": 'X', "value": 'PRESS', "shift": True}, None),
        ("PLANE_Y", {"type": 'Y', "value": 'PRESS', "shift": True}, None),
        ("PLANE_Z", {"type": 'Z', "value": 'PRESS', "shift": True}, None),
        ("CONS_OFF", {"type": 'C', "value": 'PRESS'}, None),
        ("TRANSLATE", {"type": 'G', "value": 'PRESS'}, None),
        ("VERT_EDGE_SLIDE", {"type": 'G', "value": 'PRESS'}, None),
        ("ROTATE", {"type": 'R', "value": 'PRESS'}, None),
        ("TRACKBALL", {"type": 'R', "value": 'PRESS'}, None),
        ("RESIZE", {"type": 'S', "value": 'PRESS'}, None),
        ("ROTATE_NORMALS", {"type": 'N', "value": 'PRESS'}, None),
        ("EDIT_SNAP_SOURCE_ON", {"type": 'B', "value": 'PRESS'}, None),
        ("EDIT_SNAP_SOURCE_OFF", {"type": 'B', "value": 'PRESS'}, None),
        ("SNAP_TOGGLE", {"type": 'TAB', "value": 'PRESS', "shift": True}, None),
        ("SNAP_INV_ON", {"type": 'LEFT_CTRL', "value": 'PRESS', "any": True}, None),
        ("SNAP_INV_OFF", {"type": 'LEFT_CTRL', "value": 'RELEASE', "any": True}, None),
        ("SNAP_INV_ON", {"type": 'RIGHT_CTRL', "value": 'PRESS', "any": True}, None),
        ("SNAP_INV_OFF", {"type": 'RIGHT_CTRL', "value": 'RELEASE', "any": True}, None),
        ("ADD_SNAP", {"type": 'A', "value": 'PRESS'}, None),
        ("ADD_SNAP", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        ("REMOVE_SNAP", {"type": 'A', "value": 'PRESS', "alt": True}, None),
        ("PROPORTIONAL_SIZE_UP", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True}, None),
        ("PROPORTIONAL_SIZE_DOWN", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True}, None),
        ("PROPORTIONAL_SIZE_UP", {"type": 'PAGE_UP', "value": 'PRESS', "shift": True, "repeat": True}, None),
        ("PROPORTIONAL_SIZE_DOWN", {"type": 'PAGE_DOWN', "value": 'PRESS', "shift": True, "repeat": True}, None),
        ("PROPORTIONAL_SIZE_UP", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'}, None),
        ("PROPORTIONAL_SIZE_DOWN", {"type": 'WHEELUPMOUSE', "value": 'PRESS'}, None),
        ("PROPORTIONAL_SIZE_UP", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "shift": True}, None),
        ("PROPORTIONAL_SIZE_DOWN", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "shift": True}, None),
        ("PROPORTIONAL_SIZE", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("AUTOIK_CHAIN_LEN_UP", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True}, None),
        ("AUTOIK_CHAIN_LEN_DOWN", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True}, None),
        ("AUTOIK_CHAIN_LEN_UP", {"type": 'PAGE_UP', "value": 'PRESS', "shift": True, "repeat": True}, None),
        ("AUTOIK_CHAIN_LEN_DOWN", {"type": 'PAGE_DOWN', "value": 'PRESS', "shift": True, "repeat": True}, None),
        ("AUTOIK_CHAIN_LEN_UP", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'}, None),
        ("AUTOIK_CHAIN_LEN_DOWN", {"type": 'WHEELUPMOUSE', "value": 'PRESS'}, None),
        ("AUTOIK_CHAIN_LEN_UP", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "shift": True}, None),
        ("AUTOIK_CHAIN_LEN_DOWN", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "shift": True}, None),
        ("INSERTOFS_TOGGLE_DIR", {"type": 'T', "value": 'PRESS'}, None),
        ("NODE_ATTACH_ON", {"type": 'LEFT_ALT', "value": 'RELEASE', "any": True}, None),
        ("NODE_ATTACH_OFF", {"type": 'LEFT_ALT', "value": 'PRESS', "any": True}, None),
        ("AUTOCONSTRAIN", {"type": 'MIDDLEMOUSE', "value": 'ANY'}, None),
        ("AUTOCONSTRAINPLANE", {"type": 'MIDDLEMOUSE', "value": 'ANY', "shift": True}, None),
        ("PRECISION", {"type": 'LEFT_SHIFT', "value": 'ANY', "any": True}, None),
        ("PRECISION", {"type": 'RIGHT_SHIFT', "value": 'ANY', "any": True}, None),
        ("STRIP_CLAMP_TOGGLE", {"type": 'C', "value": 'PRESS', "any": True}, None),
        ("STRIP_OVERLAP_SHUFFLE", {"type": 'S', "value": 'PRESS'}, None),
        ("STRIP_OVERLAP_RIPPLE", {"type": 'R', "value": 'PRESS'}, None),
        ("STRIP_OVERLAP_OVERWRITE", {"type": 'V', "value": 'PRESS'}, None),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Tool System Key-maps
#
# Named are auto-generated based on the tool name and it's toolbar.


def km_3d_view_tool_select(params):
    return (
        "3D View Tool: Tweak",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select(params, "view3d.select", extend="toggle")},
    )


# NOTE: duplicated from `blender_default.py`.


def km_3d_view_tool_interactive_add(params):
    return (
        "3D View Tool: Object, Add Primitive",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("view3d.interactive_add", {"type": params.tool_mouse, "value": 'CLICK_DRAG'},
             {"properties": [("wait_for_input", False)]}),
            ("view3d.interactive_add", {"type": params.tool_mouse, "value": 'CLICK_DRAG', "ctrl": True},
             {"properties": [("wait_for_input", False)]}),
        ]},
    )


# Fallback for gizmos that don't have custom a custom key-map.


def km_generic_gizmo_drag(_params):
    keymap = (
        "Generic Gizmo Drag",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": _template_items_gizmo_tweak_value_drag()},
    )

    return keymap


def km_generic_gizmo_maybe_drag(params):
    keymap = (
        "Generic Gizmo Maybe Drag",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items":
         _template_items_gizmo_tweak_value_drag()
         },
    )

    return keymap


# ------------------------------------------------------------------------------
# Full Configuration

def generate_keymaps_impl(params=None):
    if params is None:
        params = Params()
    return [
        # Window, screen, area, region.
        km_window(params),
        km_screen(params),
        km_screen_editing(params),
        km_screen_region_context_menu(params),
        km_view2d(params),
        km_view2d_buttons_list(params),
        km_user_interface(params),
        km_property_editor(params),

        # Editors.
        km_outliner(params),
        km_view3d_generic(params),
        km_view3d(params),
        km_markers(params),
        km_info(params),
        km_file_browser(params),
        km_file_browser_main(params),
        km_file_browser_buttons(params),

        # Animation.
        km_frames(params),
        km_animation(params),
        km_animation_channels(params),

        # Modes.
        km_object_mode(params),
        km_mesh(params),
        km_object_non_modal(params),

        # Modal maps.
        km_knife_tool_modal_map(params),
        km_eyedropper_modal_map(params),
        km_eyedropper_colorramp_pointsampling_map(params),
        km_transform_modal_map(params),

        # Gizmos.
        km_generic_gizmo_drag(params),
        km_generic_gizmo_maybe_drag(params),

        # Tool System.
        km_3d_view_tool_select(params),
        km_3d_view_tool_interactive_add(params),
    ]


def keymap_transform_tool_mmb(keymap):
    import re
    # Any tool besides fallback tools.
    re_fallback_tool = re.compile(
        r".*\bTool:\s(?!"
        r".*\bSelect Box$|"
        r".*\bSelect Circle$|"
        r".*\bSelect Lasso$|"
        r".*\bTweak$)",
    )
    for km_name, _km_args, km_content in keymap:
        if re_fallback_tool.match(km_name):
            km_items = km_content["items"]
            km_items_new = []
            for kmi in km_items:
                ty = kmi[1]["type"]
                value = kmi[1]["value"]
                if km_name.endswith(" (fallback)"):
                    if ty == 'RIGHTMOUSE':
                        kmi = (kmi[0], kmi[1].copy(), kmi[2])
                        kmi[1]["type"] = 'LEFTMOUSE'
                        km_items_new.append(kmi)
                else:
                    if ty == 'LEFTMOUSE':
                        if value == 'CLICK_DRAG':
                            kmi = (kmi[0], kmi[1].copy(), kmi[2])
                            if kmi[1].get("direction", 'ANY') == 'ANY':
                                kmi[1]["type"] = 'MIDDLEMOUSE'
                                kmi[1]["value"] = 'PRESS'
                            else:
                                # Directional tweaking can't be replaced by middle-mouse.
                                kmi[1]["type"] = 'MIDDLEMOUSE'
                            km_items_new.append(kmi)
                        else:
                            kmi = (kmi[0], kmi[1].copy(), kmi[2])
                            kmi[1]["type"] = 'MIDDLEMOUSE'
                            kmi[1]["value"] = 'PRESS'
                            km_items_new.append(kmi)
            km_items.extend(km_items_new)


def generate_keymaps(params=None):
    import os
    from bpy.utils import execfile
    keymap = generate_keymaps_impl(params)

    # Combine the key-map to support manipulating it, so we don't need to manually
    # define key-map here just to manipulate them.
    blender_default_mod = execfile(
        os.path.join(os.path.dirname(__file__), "blender_default.py"),
    )

    blender_default = blender_default_mod.generate_keymaps(
        # Use the default key-map with only minor changes to default arguments.
        blender_default_mod.Params(
            # Needed so the fallback key-map items are populated.
            use_fallback_tool=True,
        ),
    )

    keymap_existing_names = {km[0] for km in keymap}
    keymap.extend([km for km in blender_default if km[0] not in keymap_existing_names])

    # Manipulate the key-map.
    keymap_transform_tool_mmb(keymap)

    return keymap
