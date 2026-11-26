#!/usr/bin/env bash
set -euo pipefail

blender=$1
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT
export BLENDERIX_SMOKE_FILE="$work_dir/cube.blend"

"$blender" --background --factory-startup --python-exit-code 1 --python-expr '
import bpy
import math
import os

bpy.ops.mesh.primitive_cube_add()
cube = bpy.context.object
assert len(cube.data.vertices) == 8

assert bpy.ops.transform.translate(value=(1, 2, 3)) == {"FINISHED"}
assert tuple(cube.location) == (1, 2, 3)
assert bpy.ops.transform.rotate(value=math.pi / 2, orient_axis="Z") == {"FINISHED"}
assert abs(cube.rotation_euler.z - math.pi / 2) < 1e-5
assert bpy.ops.transform.resize(value=(2, 2, 2)) == {"FINISHED"}
assert all(abs(scale - 2) < 1e-5 for scale in cube.scale)
cube.location = (0, 0, 0)
cube.rotation_euler = (0, 0, 0)
cube.scale = (1, 1, 1)
print("Mesh object transforms passed")

dup_scene = bpy.context.scene.copy()
assert len(bpy.data.scenes) == 2
bpy.data.batch_remove([dup_scene])
assert len(bpy.data.scenes) == 1
print("Scene creation and duplication passed")

mesh = cube.data
colors = mesh.color_attributes.new(name="Working space test", type="FLOAT_COLOR", domain="POINT")
colors.data[0].color = (0.8, 0.2, 0.1, 1.0)
before = tuple(colors.data[0].color)
image = bpy.data.images.new("Unsaved image", 2, 2)
image.pixels[0] = 0.25
assert image.is_dirty
assert bpy.ops.wm.set_working_color_space(working_space="Linear Rec.2020", convert_colors=True) == {"CANCELLED"}
bpy.data.images.remove(image)
assert bpy.ops.wm.set_working_color_space(working_space="Linear Rec.2020", convert_colors=True) == {"FINISHED"}
assert tuple(colors.data[0].color) != before
assert bpy.ops.wm.set_working_color_space(working_space="Linear Rec.709", convert_colors=True) == {"FINISHED"}
assert all(abs(a - b) < 1e-5 for a, b in zip(before, colors.data[0].color))
print("Working color space conversion and unsaved-image guard passed")

bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.subdivide(number_cuts=1)
bpy.ops.object.mode_set(mode="OBJECT")
assert len(cube.data.vertices) == 26
cube.name = "BlenderiX CI Cube"
assert bpy.ops.wm.save_as_mainfile(filepath=os.environ["BLENDERIX_SMOKE_FILE"]) == {"FINISHED"}

remesh = cube.modifiers.new(name="DualCon Smoke Test", type="REMESH")
remesh.mode = "SHARP"
assert bpy.ops.object.modifier_apply(modifier=remesh.name) == {"FINISHED"}
assert len(cube.data.vertices) > 26
print("DualCon Remesh passed")
'

"$blender" --background --factory-startup --python-exit-code 1 --python-expr '
import bpy
import os

assert bpy.ops.wm.open_mainfile(filepath=os.environ["BLENDERIX_SMOKE_FILE"]) == {"FINISHED"}
assert len(bpy.data.objects["BlenderiX CI Cube"].data.vertices) == 26
print("Subdivision and .blend round trip passed")
'
