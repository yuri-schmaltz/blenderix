# SPDX-FileCopyrightText: 2011-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# note, properties_animviz is a helper module only.

# support reloading sub-modules
if "bpy" in locals():
    from importlib import reload
    _modules_loaded[:] = [reload(val) for val in _modules_loaded]
    del reload

_modules = [
    "properties_data_empty",
    "properties_data_mesh",
    "properties_data_modifier",
    "properties_object",
    "properties_physics_geometry_nodes",
    "properties_scene",
    "properties_collection",
    "generic_ui_list",

    # Generic Space Modules
    #
    # Depends on DNA_WORKSPACE_TOOL (C define).
    "space_toolsystem_common",
    "space_toolsystem_toolbar",

    "space_filebrowser",
    "space_info",
    "space_outliner",
    "space_project",
    "space_properties",
    "space_statusbar",
    "space_topbar",
    "space_userpref",
    "space_view3d",
    "space_view3d_sidebar",
    "space_view3d_toolbar",

    # XXX, keep last so panels show after all other tool options.
]

import bpy

__import__(name=__name__, fromlist=_modules)
_namespace = globals()
_modules_loaded = [_namespace[name] for name in _modules]
del _namespace


def register():
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
    for mod in _modules_loaded:
        for cls in mod.classes:
            if hasattr(bpy.types, "AssetShelf") and issubclass(cls, bpy.types.AssetShelf):
                continue
            try:
                register_class(cls)
            except Exception:
                pass

    space_filebrowser.register_props()

    from bpy.props import (
        EnumProperty,
        StringProperty,
    )
    from bpy.types import (
        WindowManager,
    )

    # space_userprefs.py
    def addon_filter_items(_self, _context):
        import addon_utils

        items = [
            ('All', "All", "All Add-ons"),
            ('User', "User", "All Add-ons Installed by User"),
        ]

        items_unique = set()

        for mod in addon_utils.modules(refresh=False):
            bl_info = addon_utils.module_bl_info(mod)
            items_unique.add(bl_info["category"])

        items.extend([(cat, cat, "") for cat in sorted(items_unique)])
        return items

    WindowManager.addon_search = StringProperty(
        name="Filter",
        description="Filter by add-on name, author & category",
        options={'TEXTEDIT_UPDATE'},
    )
    WindowManager.addon_filter = EnumProperty(
        items=addon_filter_items,
        name="Category",
        description="Filter add-ons by category",
    )

    WindowManager.addon_support = EnumProperty(
        items=(
            ('OFFICIAL', "Official", "Officially supported"),
            ('COMMUNITY', "Community", "Maintained by community developers"),
        ),
        name="Support",
        description="Display support level",
        default={'OFFICIAL', 'COMMUNITY'},
        options={'ENUM_FLAG'},
    )

    # done...


def unregister():
    from bpy.utils import unregister_class

    for mod in reversed(_modules_loaded):
        for cls in reversed(mod.classes):
            if cls.is_registered:
                unregister_class(cls)
    for cls in reversed(classes):
        if cls.is_registered:
            unregister_class(cls)


# Define a default UIList, when a list does not need any custom drawing...
# Keep in sync with its #defined name in UI_interface.hh


class UI_UL_list(bpy.types.UIList):
    # These are common filtering or ordering operations (same as the default C ones!).
    @staticmethod
    def filter_items_by_name(pattern, bitflag, items, propname="name", flags=None, reverse=False):
        """
        Set FILTER_ITEM for items which name matches filter_name one (case-insensitive).

        :param pattern: Filtering pattern (shell-style glob, case-insensitive).
        :type pattern: str
        :param bitflag: Bit to set on matching items.
        :type bitflag: int
        :param items: Collection to filter.
        :type items: Sequence[Any]
        :param propname: Name of the string property on each item to match against.
        :type propname: str
        :param flags: Existing flag list to update; one int per item. ``None`` allocates a new list.
        :type flags: Sequence[int] | None
        :param reverse: Invert the match (set the bit on non-matching items).
        :type reverse: bool
        :return: Updated flags list, or an empty list when no filtering was needed.
        :rtype: list[int]
        """
        import fnmatch
        import re

        if not pattern or not items:  # Empty pattern or list = no filtering!
            return flags or []

        if flags is None:
            flags = [0] * len(items)

        # Implicitly add heading/trailing wildcards.
        pattern_regex = re.compile(fnmatch.translate("*" + pattern + "*"), re.IGNORECASE)

        for i, item in enumerate(items):
            name = getattr(item, propname, None)
            # This is similar to a logical XOR.
            if bool(name and pattern_regex.match(name)) is not reverse:
                flags[i] |= bitflag
        return flags

    @staticmethod
    def sort_items_helper(sort_data, key, reverse=False):
        """
        Common sorting utility. Returns a neworder list mapping org_index -> new_index.

        :param sort_data: Unordered list of tuples ``[(org_index, ...), ...]`` to sort in place.
        :type sort_data: list[tuple[int, Any, ...]]
        :param key: Sort key callable, same as for the ``sorted`` builtin.
        :type key: Callable[[tuple[int, Any, ...]], Any]
        :param reverse: Reverse the sort order.
        :type reverse: bool
        :return: A list mapping original index to new index.
        :rtype: list[int]
        """
        sort_data.sort(key=key, reverse=reverse)
        neworder = [None] * len(sort_data)
        for newidx, (orgidx, *_) in enumerate(sort_data):
            neworder[orgidx] = newidx
        return neworder

    @classmethod
    def sort_items_by_name(cls, items, propname="name"):
        """
        Re-order items using their names (case-insensitive).

        :param items: Collection to sort.
        :type items: Sequence[Any]
        :param propname: Name of the string property on each item to sort by.
        :type propname: str
        :return: A list mapping original index to new index, or an empty list when no sorting was needed.
        :rtype: list[int]
        """
        _sort = [(idx, getattr(it, propname, "")) for idx, it in enumerate(items)]
        return cls.sort_items_helper(_sort, lambda e: e[1].lower())


class UI_MT_list_item_context_menu(bpy.types.Menu):
    """
    UI List item context menu definition. Scripts can append/prepend this to
    add own operators to the context menu. They must check context though, so
    their items only draw in a valid context and for the correct UI list.
    """

    bl_label = "List Item"
    bl_idname = "UI_MT_list_item_context_menu"

    def draw(self, _context):
        # Dummy function. This type is just for scripts to append their own
        # context menu items.
        pass


class UI_MT_button_context_menu(bpy.types.Menu):
    """
    UI button context menu definition. Scripts can append/prepend this to
    add own operators to the context menu. They must check context though, so
    their items only draw in a valid context and for the correct buttons.
    """

    bl_label = "List Item"
    bl_idname = "UI_MT_button_context_menu"

    def draw(self, _context):
        # Draw menu entries created with the legacy `WM_MT_button_context` class.
        # This is deprecated, and support will be removed in a future release.
        if hasattr(bpy.types, "WM_MT_button_context"):
            self.layout.menu_contents("WM_MT_button_context")


classes = (
    UI_UL_list,
    UI_MT_list_item_context_menu,
    UI_MT_button_context_menu,
)
