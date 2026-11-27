# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Panel

from rna_prop_ui import PropertyPanel


class SceneButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "scene"


class SCENE_PT_unit(SceneButtonsPanel, Panel):
    bl_label = "Units"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        unit = context.scene.unit_settings

        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column()
        col.prop(unit, "scale_length")
        col.prop(unit, "use_separate")

        col = layout.column()
        col.prop(unit, "system_rotation", text="Rotation")
        col.prop(unit, "length_unit", text="Length")
        col.prop(unit, "mass_unit", text="Mass")


class SCENE_PT_rigid_body_world(SceneButtonsPanel, Panel):
    bl_label = "Rigid Body World"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        scene = context.scene
        rbw = scene.rigidbody_world
        if rbw is not None:
            self.layout.prop(rbw, "enabled", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        rbw = scene.rigidbody_world



class RigidBodySubPanel(SceneButtonsPanel):
    bl_parent_id = "SCENE_PT_rigid_body_world"

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and scene.rigidbody_world


class SCENE_PT_rigid_body_world_settings(RigidBodySubPanel, Panel):
    bl_label = "Settings"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        rbw = scene.rigidbody_world

        if rbw:
            flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

            col = flow.column()
            col.active = rbw.enabled

            col = col.column()
            col.prop(rbw, "collection")
            col.prop(rbw, "constraints")

            col = col.column()
            col.prop(rbw, "time_scale", text="Speed")

            col = flow.column()
            col.active = rbw.enabled
            col.prop(rbw, "use_split_impulse")

            col = col.column()
            col.prop(rbw, "substeps_per_frame")
            col.prop(rbw, "solver_iterations")


class SCENE_PT_rigid_body_cache(RigidBodySubPanel, Panel):
    bl_label = "Cache"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        scene = context.scene
        rbw = scene.rigidbody_world

        point_cache_ui(self, rbw.point_cache, rbw.point_cache.is_baked is False and rbw.enabled, 'RIGID_BODY')


class SCENE_PT_rigid_body_field_weights(RigidBodySubPanel, Panel):
    bl_label = "Field Weights"
    bl_parent_id = "SCENE_PT_rigid_body_world"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        scene = context.scene
        rbw = scene.rigidbody_world

        effector_weights_ui(self, rbw.effector_weights, 'RIGID_BODY')


class SCENE_PT_eevee_light_probes(SceneButtonsPanel, Panel):
    bl_label = "Light Probes"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        props = scene.eevee

        col = layout.column()

        # TODO(fclem): Move to probe
        col.prop(props, "gi_cubemap_resolution", text="Spheres Resolution")

        row = col.row(align=True)




class SCENE_PT_custom_props(SceneButtonsPanel, PropertyPanel, Panel):
    _context_path = "scene"
    _property_type = bpy.types.Scene


classes = (
    SCENE_PT_unit,
    SCENE_PT_eevee_light_probes,
    SCENE_PT_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
