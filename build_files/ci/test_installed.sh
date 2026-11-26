#!/usr/bin/env bash
set -euo pipefail

blender=$1
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT
export BLENDERIX_SMOKE_FILE="$work_dir/cube.blend"

"$blender" --background --factory-startup --python-exit-code 1 --python-expr '
import bpy
import os

bpy.ops.mesh.primitive_cube_add()
cube = bpy.context.object
assert len(cube.data.vertices) == 8
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
