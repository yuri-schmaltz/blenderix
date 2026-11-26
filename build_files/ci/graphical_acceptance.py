"""Exercise modeling and file I/O in a graphical BlenderiX session."""

import os
import traceback
from pathlib import Path

import bpy


def check_flows():
    window = bpy.context.window_manager.windows[0]
    screen = window.screen
    view = next(area for area in screen.areas if area.type == "VIEW_3D")
    region = next(region for region in view.regions if region.type == "WINDOW")
    assert window.width > 0 and window.height > 0 and region.width > 0
    with bpy.context.temp_override(window=window, area=view, region=region):
        bpy.ops.wm.redraw_timer(type="DRAW_WIN_SWAP", iterations=2)
        bpy.ops.object.select_all(action="SELECT")
        bpy.ops.object.delete()
        assert bpy.ops.mesh.primitive_cube_add() == {"FINISHED"}
        cube = bpy.context.object
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.mesh.subdivide()
        bpy.ops.object.mode_set(mode="OBJECT")
        assert len(cube.data.vertices) == 26
        modifier = cube.modifiers.new("Acceptance Triangulate", "TRIANGULATE")
        assert bpy.ops.object.modifier_apply(modifier=modifier.name) == {"FINISHED"}
        assert len(cube.data.polygons) == 48
        directory = Path(os.environ["BLENDERIX_GUI_DIR"]).resolve()
        directory.mkdir(exist_ok=True)
        blend = directory / "acceptance.blend"
        assert bpy.ops.wm.save_as_mainfile(filepath=str(blend)) == {"FINISHED"}
        for extension in ("obj", "stl", "ply"):
            path = directory / f"acceptance.{extension}"
            assert getattr(bpy.ops.wm, f"{extension}_export")(filepath=str(path)) == {"FINISHED"}
            assert path.stat().st_size > 0
            bpy.ops.object.select_all(action="SELECT")
            bpy.ops.object.delete()
            assert getattr(bpy.ops.wm, f"{extension}_import")(filepath=str(path)) == {"FINISHED"}
            restored = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
            assert len(restored) == 1 and len(restored[0].data.polygons) == 48, extension


def run():
    try:
        if os.environ.get("BLENDERIX_GUI_REOPEN") == "1":
            assert len(bpy.context.scene.objects) == 1
            assert len(bpy.context.scene.objects[0].data.polygons) == 48
        else:
            check_flows()
        print("Graphical acceptance passed", flush=True)
        bpy.ops.wm.quit_blender()
    except Exception:
        traceback.print_exc()
        os._exit(1)


bpy.app.timers.register(run, first_interval=2)
