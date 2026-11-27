# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../ikplugin
  ../../makesrna
  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)
set(INC_SYS
)

set(SRC
  transform.cc
  transform_constraints.cc
  transform_convert.cc
  transform_convert_cursor.cc
  transform_convert_mesh.cc
  transform_convert_mesh_edge.cc
  transform_convert_mesh_skin.cc
  transform_convert_mesh_vert_cdata.cc
  transform_convert_object.cc
  transform_convert_object_texspace.cc
  transform_draw_cursors.cc
  transform_generics.cc
  transform_gizmo_3d.cc
  transform_gizmo_3d_cage.cc
  transform_gizmo_3d_shear.cc
  transform_gizmo_extrude_3d.cc
  transform_input.cc
  transform_mode.cc
  transform_mode_align.cc
  transform_mode_bend.cc
  transform_mode_customdata.cc
  transform_mode_edge_rotate_normal.cc
  transform_mode_edge_slide.cc
  transform_mode_mirror.cc
  transform_mode_push_pull.cc
  transform_mode_resize.cc
  transform_mode_rotate.cc
  transform_mode_rotate_quadrants.cc
  transform_mode_rotate_quadrants.hh
  transform_mode_shear.cc
  transform_mode_shrink_fatten.cc
  transform_mode_skin_resize.cc
  transform_mode_snapsource.cc
  transform_mode_tosphere.cc
  transform_mode_trackball.cc
  transform_mode_translate.cc
  transform_mode_vert_slide.cc
  transform_ops.cc
  transform_orientations.cc
  transform_snap.cc
  transform_snap_object.cc
  transform_snap_object_editmesh.cc
  transform_snap_object_mesh.cc

  transform.hh
  transform_constraints.hh
  transform_convert.hh
  transform_draw_cursors.hh
  transform_gizmo.hh
  transform_mode.hh
  transform_orientations.hh
  transform_snap.hh
  transform_snap_object.hh
)

set(LIB
  PRIVATE bf::animrig
  PRIVATE bf::blenfont
  PRIVATE bf::blentranslation
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::geometry
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::nodes
  PRIVATE bf::render
  PRIVATE bf::windowmanager
)

blender_add_lib(bf_editor_transform "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.hh
add_dependencies(bf_editor_transform bf_rna)
