"""Generate legacy .blend fixtures with upstream Blender or check them in BlenderiX."""

import argparse
import itertools
import math
import sys
from pathlib import Path

import bpy


def create_cube():
    bpy.ops.mesh.primitive_cube_add(size=2)
    cube = bpy.context.object
    cube.name = "Legacy Mesh"
    cube["fixture_marker"] = 42
    return cube


def create_fixtures(directory):
    directory.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    cube = create_cube()
    bpy.ops.wm.save_as_mainfile(filepath=str(directory / "mesh.blend"), compress=False)

    modifier = cube.modifiers.new("Legacy Bevel", "BEVEL")
    modifier.width = 0.125
    modifier.segments = 2
    bpy.ops.wm.save_as_mainfile(filepath=str(directory / "modifier.blend"), compress=False)

    cube.keyframe_insert(data_path="location", frame=1)
    bpy.ops.object.camera_add(location=(4, 4, 4))
    bpy.context.scene.camera = bpy.context.object
    bpy.ops.wm.save_as_mainfile(filepath=str(directory / "mixed.blend"), compress=False)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    with bpy.data.libraries.load(str(directory / "mesh.blend"), link=True) as (_, target):
        target.objects = ["Legacy Mesh"]
    bpy.context.scene.collection.objects.link(target.objects[0])
    bpy.ops.wm.save_as_mainfile(
        filepath=str(directory / "linked.blend"), compress=False, relative_remap=True
    )

    if bpy.app.version < (4, 1):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        mesh = create_cube().data
        mesh.use_auto_smooth = True
        mesh.auto_smooth_angle = math.radians(30)
        for face in mesh.polygons:
            face.use_smooth = True
        bpy.ops.wm.save_as_mainfile(filepath=str(directory / "auto-smooth.blend"), compress=False)


def check_fixture(case, path):
    assert bpy.ops.wm.open_mainfile(filepath=str(path)) == {"FINISHED"}
    cube = bpy.data.objects["Legacy Mesh"]
    mesh = cube.data
    assert {tuple(vertex.co) for vertex in mesh.vertices} == set(
        itertools.product((-1, 1), repeat=3)
    )
    assert len(mesh.edges) == 12 and len(mesh.polygons) == 6
    assert all(len(set(polygon.vertices)) == 4 for polygon in mesh.polygons)
    assert cube["fixture_marker"] == 42
    if case in {"modifier", "mixed"}:
        bevel = cube.modifiers["Legacy Bevel"]
        assert bevel.type == "BEVEL" and bevel.segments == 2 and bevel.width == 0.125
        evaluated = cube.evaluated_get(bpy.context.evaluated_depsgraph_get())
        assert len(evaluated.data.vertices) > len(mesh.vertices)
    if case == "auto-smooth":
        assert all(face.use_smooth for face in mesh.polygons)
        if hasattr(mesh, "use_auto_smooth"):
            assert mesh.use_auto_smooth
        else:
            sharp_edges = mesh.attributes["sharp_edge"]
            assert len(sharp_edges.data) == 12 and all(edge.value for edge in sharp_edges.data)
    if case == "linked":
        assert cube.library and Path(bpy.path.abspath(cube.library.filepath)).is_file()
    if case == "mixed":
        camera = bpy.context.scene.camera
        print(
            "Feature data observed:",
            f"camera={bool(camera and camera.type == 'CAMERA')}",
            f"animation={bool(cube.animation_data and cube.animation_data.action)}",
            flush=True,
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--case", choices=("mesh", "modifier", "linked", "mixed", "auto-smooth"))
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1 :])
    directory = args.directory.resolve()
    if args.case is None:
        create_fixtures(directory)
        return
    check_fixture(args.case, directory / f"{args.case}.blend")
    output = directory / f"{args.case}.blenderix.blend"
    assert bpy.ops.wm.save_as_mainfile(filepath=str(output), copy=True) == {"FINISHED"}
    check_fixture(args.case, output)
    print(f"Legacy fixture passed: {args.case}", flush=True)


if __name__ == "__main__":
    main()
