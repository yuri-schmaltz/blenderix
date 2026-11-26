# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import (
    Menu,
    Operator,
    OperatorFileListElement,
    Panel,
    WindowManager,
)
from bpy.props import (
    BoolProperty,
    CollectionProperty,
    StringProperty,
)
from bpy.app.translations import (
    pgettext_rpt as rpt_,
    pgettext_data as data_,
)
from bl_ui.utils import PresetPanel


# For preset popover menu
WindowManager.preset_name = StringProperty(
    name="Preset Name",
    description="Name for new preset",
    default=data_("New Preset"),
)


# -----------------------------------------------------------------------------
# Private Implementation

def _call_preset_cb(fn, context, filepath, *, deprecated="4.2"):
    # Allow "None" so the caller doesn't have to assign a variable and check it.
    if fn is None:
        return

    if hasattr(fn, "__self__"):
        args_offset = 1
    else:
        args_offset = 0

    # Support a `filepath` argument, optional for backwards compatibility.
    fn_arg_count = getattr(getattr(fn, "__code__", None), "co_argcount", None)
    if fn_arg_count == 2 + args_offset:
        args = (context, filepath)
    else:
        print("Deprecated since Blender {:s}, a filepath argument should be included in: {!r}".format(deprecated, fn))
        args = (context, )

    try:
        fn(*args)
    except Exception as ex:
        print("Internal error running", fn, str(ex))


def _is_path_readonly(path):
    from bpy.utils import (
        is_path_builtin,
        is_path_extension,
    )
    # Consider extension repository paths read-only because they should not be manipulated
    # since the only way to restore the preset is to re-install the extension.
    return is_path_builtin(path) or is_path_extension(path)


# -----------------------------------------------------------------------------
# Main Preset Implementation

class AddPresetBase:
    """Base preset class, only for subclassing
    subclasses must define
     - preset_values
     - preset_subdir """
    # bl_idname = "script.preset_base_add"
    # bl_label = "Add a Python Preset"

    # only because invoke_props_popup requires. Also do not add to search menu.
    bl_options = {'REGISTER', 'INTERNAL'}

    name: StringProperty(
        name="Name",
        description="Name of the preset, used to make the path name",
        maxlen=64,
        options={'SKIP_SAVE'},
    )
    remove_name: BoolProperty(
        default=False,
        options={'HIDDEN', 'SKIP_SAVE'},
    )
    remove_active: BoolProperty(
        default=False,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    @staticmethod
    def as_filename(name):  # could reuse for other presets

        # lazy init maketrans
        def maketrans_init():
            cls = AddPresetBase
            attr = "_as_filename_trans"

            trans = getattr(cls, attr, None)
            if trans is None:
                trans = str.maketrans({char: "_" for char in " !@#$%^&*(){}:\";'[]<>,.\\/?"})
                setattr(cls, attr, trans)
            return trans

        name = name.strip()
        name = bpy.path.display_name_to_filepath(name)
        trans = maketrans_init()
        # Strip surrounding "_" as they are displayed as spaces.
        return name.translate(trans).strip("_")

    def execute(self, context):
        import os

        if hasattr(self, "pre_cb"):
            self.pre_cb(context)

        preset_menu_class = getattr(bpy.types, self.preset_menu)

        is_xml = getattr(preset_menu_class, "preset_type", None) == 'XML'
        is_preset_add = not (self.remove_name or self.remove_active)

        if is_xml:
            ext = ".xml"
        else:
            ext = ".py"

        name = self.name.strip() if is_preset_add else self.name

        if is_preset_add:
            if not name:
                return {'FINISHED'}

            # Reset preset name
            wm = bpy.data.window_managers[0]
            if name == wm.preset_name:
                wm.preset_name = data_("New Preset")

            filename = self.as_filename(name)

            target_path = os.path.join("presets", self.preset_subdir)
            target_path = bpy.utils.user_resource('SCRIPTS', path=target_path, create=True)

            if not target_path:
                self.report({'WARNING'}, "Failed to create presets path")
                return {'CANCELLED'}

            preset_filepath = bpy.utils.preset_find(filename, self.preset_subdir, ext=ext)
            if _is_path_readonly(target_path) or preset_filepath:
                self.report({'WARNING'}, "Cannot create preset \"{:s}\", as the name already exists".format(name))
                return {'CANCELLED'}

            filepath = os.path.join(target_path, filename) + ext

            if hasattr(self, "add"):
                self.add(context, filepath)
            else:
                print("Writing Preset: {!r}".format(filepath))

                if is_xml:
                    import _rna_xml as rna_xml
                    rna_xml.xml_file_write(context, filepath, preset_menu_class.preset_xml_map)
                else:

                    def rna_recursive_attr_expand(value, rna_path_step, level):
                        if isinstance(value, bpy.types.PropertyGroup):
                            # Avoid properties being handled multiple times.
                            # This happens when a class defines a property which is also defined by it's parent class.
                            # The parents property is shadowed, so it only makes sense to write each property once.
                            # Happens with `OperatorFileListElement` which has two `name` properties.
                            properties_skip = {"rna_type"}
                            for sub_value_attr in value.bl_rna.properties.keys():
                                if sub_value_attr in properties_skip:
                                    continue
                                properties_skip.add(sub_value_attr)

                                sub_value = getattr(value, sub_value_attr)
                                rna_recursive_attr_expand(
                                    sub_value,
                                    "{:s}.{:s}".format(rna_path_step, sub_value_attr),
                                    level,
                                )
                        elif type(value).__name__ == "bpy_prop_collection_idprop":  # could use nicer method
                            file_preset.write("{:s}.clear()\n".format(rna_path_step))
                            for sub_value in value:
                                file_preset.write("item_sub_{:d} = {:s}.add()\n".format(level, rna_path_step))
                                rna_recursive_attr_expand(sub_value, "item_sub_{:d}".format(level), level + 1)
                        else:
                            # convert thin wrapped sequences
                            # to simple lists to repr()
                            try:
                                value = value[:]
                            except Exception:
                                pass

                            file_preset.write("{:s} = {!r}\n".format(rna_path_step, value))

                    with open(filepath, "w", encoding="utf-8") as file_preset:
                        file_preset.write("import bpy\n")

                        namespace_globals = {"bpy": bpy}
                        namespace_locals = {}

                        if hasattr(self, "preset_defines"):
                            for rna_path in self.preset_defines:
                                exec(rna_path, namespace_globals, namespace_locals)
                                file_preset.write("{:s}\n".format(rna_path))
                            file_preset.write("\n")

                        for rna_path in self.preset_values:
                            value = eval(rna_path, namespace_globals, namespace_locals)
                            rna_recursive_attr_expand(value, rna_path, 1)

            preset_menu_class.bl_label = bpy.path.display_name(filename)

        else:
            if self.remove_active:
                name = preset_menu_class.bl_label

            # fairly sloppy but convenient.
            filepath = bpy.utils.preset_find(name, self.preset_subdir, ext=ext)

            if not filepath:
                filepath = bpy.utils.preset_find(name, self.preset_subdir, display_name=True, ext=ext)

            if not filepath:
                return {'CANCELLED'}

            # Do not remove bundled presets
            if _is_path_readonly(filepath):
                self.report({'WARNING'}, "Unable to remove default presets")
                return {'CANCELLED'}

            try:
                if hasattr(self, "remove"):
                    self.remove(context, filepath)
                else:
                    os.remove(filepath)
            except Exception as ex:
                self.report({'ERROR'}, rpt_("Unable to remove preset: {!r}").format(ex))
                import traceback
                traceback.print_exc()
                return {'CANCELLED'}

            # XXX, stupid!
            preset_menu_class.bl_label = "Presets"

        _call_preset_cb(getattr(self, "post_cb", None), context, filepath, deprecated="4.3")

        return {'FINISHED'}

    def check(self, _context):
        self.name = self.as_filename(self.name.strip())

    def invoke(self, context, _event):
        if not (self.remove_active or self.remove_name):
            wm = context.window_manager
            return wm.invoke_props_dialog(self)
        else:
            return self.execute(context)


class ExecutePreset(Operator):
    """Load a preset"""
    bl_idname = "script.execute_preset"
    bl_label = "Execute a Python Preset"

    filepath: StringProperty(
        subtype='FILE_PATH',
        options={'SKIP_SAVE'},
    )
    menu_idname: StringProperty(
        name="Menu ID Name",
        description="ID name of the menu this was called from",
        options={'SKIP_SAVE'},
    )

    def execute(self, context):
        from os.path import basename, splitext
        filepath = self.filepath

        # change the menu title to the most recently chosen option
        preset_class = getattr(bpy.types, self.menu_idname)
        preset_class.bl_label = bpy.path.display_name(basename(filepath), title_case=False)

        ext = splitext(filepath)[1].lower()

        if ext not in {".py", ".xml"}:
            self.report({'ERROR'}, rpt_("Unknown file type: {!r}").format(ext))
            return {'CANCELLED'}

        _call_preset_cb(getattr(preset_class, "reset_cb", None), context, filepath)

        if ext == ".py":
            try:
                bpy.utils.execfile(filepath)
            except Exception as ex:
                self.report({'ERROR'}, rpt_("Failed to execute the preset: {:s}").format(repr(ex)))

        elif ext == ".xml":
            import _rna_xml as rna_xml
            preset_xml_map = preset_class.preset_xml_map
            preset_xml_secure_types = getattr(preset_class, "preset_xml_secure_types", None)

            rna_xml.xml_file_run(context, filepath, preset_xml_map, secure_types=preset_xml_secure_types)

        _call_preset_cb(getattr(preset_class, "post_cb", None), context, filepath)

        return {'FINISHED'}


class AddPresetInterfaceTheme(AddPresetBase, Operator):
    """Add a custom theme to the preset list"""
    bl_idname = "wm.interface_theme_preset_add"
    bl_label = "Add Theme"
    preset_menu = "USERPREF_MT_interface_theme_presets"
    preset_subdir = "interface_theme"

    def post_cb(self, context, filepath):
        # Ensure the saved preset is considered "active" after saving.
        # Typically handled by the classes `bl_label` however themes use the `filepath` instead.
        context.preferences.themes[0].filepath = filepath


class RemovePresetInterfaceTheme(AddPresetBase, Operator):
    """Remove a custom theme from the preset list"""
    bl_idname = "wm.interface_theme_preset_remove"
    bl_label = "Remove Theme"
    preset_menu = "USERPREF_MT_interface_theme_presets"
    preset_subdir = "interface_theme"

    remove_active: BoolProperty(
        default=True,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    # NOTE: leave poll unset as file-system scanning should be avoided
    # while redrawing as it may involve remote file-system access.

    def invoke(self, context, event):
        filepath = context.preferences.themes[0].filepath
        if (not filepath) or _is_path_readonly(filepath):
            self.report({'ERROR'}, "Built-in themes cannot be removed")
            return {'CANCELLED'}

        return context.window_manager.invoke_confirm(self, event, title="Remove Custom Theme", confirm_text="Delete")

    def post_cb(self, context, _filepath):
        # Without this, the name & colors are kept after removing the theme.
        # Even though the theme is removed from the list, it's seems like a bug to keep it displayed after removal.
        bpy.ops.preferences.reset_default_theme()


class SavePresetInterfaceTheme(AddPresetBase, Operator):
    """Save a custom theme in the preset list"""
    bl_idname = "wm.interface_theme_preset_save"
    bl_label = "Save Theme"
    preset_menu = "USERPREF_MT_interface_theme_presets"
    preset_subdir = "interface_theme"

    remove_active: BoolProperty(
        default=True,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    # NOTE: leave poll unset as file-system scanning should be avoided
    # while redrawing as it may involve remote file-system access.

    def execute(self, context):
        import _rna_xml as rna_xml
        filepath = context.preferences.themes[0].filepath
        if (not filepath) or _is_path_readonly(filepath):
            self.report({'ERROR'}, "Built-in themes cannot be overwritten")
            return {'CANCELLED'}

        preset_menu_class = getattr(bpy.types, self.preset_menu)
        try:
            rna_xml.xml_file_write(context, filepath, preset_menu_class.preset_xml_map)
        except Exception as ex:
            self.report({'ERROR'}, rpt_("Unable to overwrite preset: {:s}").format(str(ex)))
            import traceback
            traceback.print_exc()
            return {'CANCELLED'}

        context.preferences.themes[0].filepath = filepath

        return {'FINISHED'}

    def invoke(self, context, event):
        filepath = context.preferences.themes[0].filepath
        if (not filepath) or _is_path_readonly(filepath):
            self.report({'ERROR'}, "Built-in themes cannot be overwritten")
            return {'CANCELLED'}

        return context.window_manager.invoke_confirm(self, event, title="Overwrite Custom Theme?", confirm_text="Save")


class AddPresetKeyconfig(AddPresetBase, Operator):
    """Add a custom keymap configuration to the preset list"""
    bl_idname = "wm.keyconfig_preset_add"
    bl_label = "Add Custom Keymap Configuration"
    preset_menu = "USERPREF_MT_keyconfigs"
    preset_subdir = "keyconfig"

    def add(self, _context, filepath):
        bpy.ops.preferences.keyconfig_export(filepath=filepath)
        bpy.utils.keyconfig_set(filepath)


class RemovePresetKeyconfig(AddPresetBase, Operator):
    """Remove a custom keymap configuration from the preset list"""
    bl_idname = "wm.keyconfig_preset_remove"
    bl_label = "Remove Keymap Configuration"
    preset_menu = "USERPREF_MT_keyconfigs"
    preset_subdir = "keyconfig"

    remove_active: BoolProperty(
        default=True,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    # NOTE: leave poll unset as file-system scanning should be avoided
    # while redrawing as it may involve remote file-system access.

    def pre_cb(self, context):
        keyconfigs = bpy.context.window_manager.keyconfigs
        preset_menu_class = getattr(bpy.types, self.preset_menu)
        preset_menu_class.bl_label = keyconfigs.active.name

    def post_cb(self, context, _filepath):
        keyconfigs = bpy.context.window_manager.keyconfigs
        keyconfigs.remove(keyconfigs.active)

    def invoke(self, context, event):
        keyconfigs = bpy.context.window_manager.keyconfigs
        name = keyconfigs.active.name
        filepath = bpy.utils.preset_find(name, self.preset_subdir, ext=".py")
        if (not filepath) or _is_path_readonly(filepath):
            self.report({'ERROR'}, "Built-in keymap configurations cannot be removed")
            return {'CANCELLED'}

        return context.window_manager.invoke_confirm(
            self, event, title="Remove Keymap Configuration", confirm_text="Delete",
        )


class AddPresetOperator(AddPresetBase, Operator):
    """Add or remove an Operator Preset"""
    bl_idname = "wm.operator_preset_add"
    bl_label = "Operator Preset"
    preset_menu = "WM_MT_operator_presets"

    operator: StringProperty(
        name="Operator",
        maxlen=64,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    preset_defines = [
        "op = bpy.context.active_operator",
    ]

    @property
    def preset_subdir(self):
        return AddPresetOperator.operator_path(self.operator)

    @property
    def preset_values(self):
        properties_blacklist = Operator.bl_rna.properties.keys()

        prefix, suffix = self.operator.split("_OT_", 1)
        op = getattr(getattr(bpy.ops, prefix.lower()), suffix)
        operator_rna = op.get_rna_type()
        del op

        ret = []
        for prop_id, prop in operator_rna.properties.items():
            if not prop.is_skip_preset:
                if prop_id not in properties_blacklist:
                    ret.append("op.{:s}".format(prop_id))

        return ret

    @staticmethod
    def operator_path(operator):
        import os
        prefix, suffix = operator.split("_OT_", 1)
        return os.path.join("operator", "{:s}.{:s}".format(prefix.lower(), suffix))


class WM_MT_operator_presets(Menu):
    bl_label = "Operator Presets"

    def draw(self, context):
        self.operator = context.active_operator.bl_idname

        # dummy 'default' menu item
        layout = self.layout
        layout.operator("wm.operator_defaults")
        layout.separator()

        Menu.draw_preset(self, context)

    @property
    def preset_subdir(self):
        return AddPresetOperator.operator_path(self.operator)

    preset_operator = "script.execute_preset"


class WM_PT_operator_presets(PresetPanel, Panel):
    bl_label = "Operator Presets"
    preset_add_operator = "wm.operator_preset_add"
    preset_operator = "script.execute_preset"

    @property
    def preset_subdir(self):
        return AddPresetOperator.operator_path(self.operator)

    @property
    def preset_add_operator_properties(self):
        return {"operator": self.operator}

    def draw(self, context):
        self.operator = context.active_operator.bl_idname
        PresetPanel.draw(self, context)


class WM_OT_operator_presets_cleanup(Operator):
    """Remove outdated operator properties from presets that may cause problems"""

    bl_idname = "wm.operator_presets_cleanup"
    bl_label = "Clean Up Operator Presets"

    operator: StringProperty(name="operator")
    properties: CollectionProperty(name="properties", type=OperatorFileListElement)

    def _cleanup_preset(self, filepath, properties_exclude):
        import os
        import re
        if not (os.path.isfile(filepath) and os.path.splitext(filepath)[1].lower() == ".py"):
            return
        with open(filepath, "r", encoding="utf-8") as fh:
            lines_prev = fh.read().splitlines(True)
        if not lines_prev:
            return
        regex_exclude = re.compile("(" + "|".join([re.escape("op." + prop) for prop in properties_exclude]) + ")\\b")
        lines_next = []

        i = 0
        while i < len(lines_prev):
            m = regex_exclude.match(lines_prev[i])
            if m is None:
                lines_next.append(lines_prev[i])
                i += 1
            else:
                is_collection = lines_prev[i][m.end():].startswith(".clear()")
                i += 1

                # Skip non operator lines.
                if is_collection:
                    while i < len(lines_prev) and (not lines_prev[i].startswith("op.")):
                        i += 1

        with open(filepath, "w", encoding="utf-8") as fh:
            fh.write("".join(lines_next))

    def _cleanup_operators_presets(self, operators, properties_exclude):
        import os
        base_preset_directory = bpy.utils.user_resource('SCRIPTS', path="presets", create=False)
        if not base_preset_directory:
            return
        for operator in operators:
            operator_path = AddPresetOperator.operator_path(operator)
            directory = os.path.join(base_preset_directory, operator_path)

            if not os.path.isdir(directory):
                continue
            for filename in os.listdir(directory):
                self._cleanup_preset(os.path.join(directory, filename), properties_exclude)

    def execute(self, context):
        properties_exclude = []
        operators = []
        if self.operator:
            operators.append(self.operator)
            for prop in self.properties:
                properties_exclude.append(prop.name)
        else:
            # Cleanup by default I/O Operators Presets
            operators = [
                "WM_OT_alembic_export",
                "WM_OT_alembic_import",
                "WM_OT_obj_export",
                "WM_OT_obj_import",
                "WM_OT_ply_export",
                "WM_OT_ply_import",
                "WM_OT_stl_export",
                "WM_OT_stl_import",
                "WM_OT_usd_export",
                "WM_OT_usd_import",
                "EXPORT_SCENE_OT_fbx",
                "IMPORT_SCENE_OT_fbx",
            ]
            properties_exclude = [
                "filepath",
                "directory",
                "files",
                "filename",
            ]

        self._cleanup_operators_presets(operators, properties_exclude)
        return {'FINISHED'}


classes = (
    AddPresetInterfaceTheme,
    RemovePresetInterfaceTheme,
    SavePresetInterfaceTheme,
    AddPresetKeyconfig,
    RemovePresetKeyconfig,
    AddPresetOperator,
    ExecutePreset,
    WM_MT_operator_presets,
    WM_PT_operator_presets,
    WM_OT_operator_presets_cleanup,
)
