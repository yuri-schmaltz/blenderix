# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  ../makesrna
  ../../../intern/eigen
  ../draw/intern
  ../gpu
)

set(INC_SYS
)

set(SRC
  intern/extract_elements.cc
  intern/foreach_geometry.cc
  intern/join_geometries.cc
  intern/mesh_bevel.cc
  intern/mesh_boolean.cc
  intern/mesh_boolean_manifold.cc
  intern/mesh_copy_selection.cc
  intern/mesh_merge_verts.cc
  intern/mesh_primitive_cuboid.cc
  intern/mesh_primitive_cylinder_cone.cc
  intern/mesh_primitive_grid.cc
  intern/mesh_primitive_line.cc
  intern/mesh_primitive_uv_sphere.cc
  intern/mesh_selection.cc
  intern/mesh_split_edges.cc
  intern/mesh_triangulate.cc
  intern/mix_geometries.cc
  intern/randomize.cc
  intern/realize_instances.cc
  intern/reorder.cc
  intern/reverse_uv_sampler.cc
  intern/transform.cc
  intern/uv_pack.cc
  intern/uv_parametrizer.cc

  GEO_extract_elements.hh
  GEO_foreach_geometry.hh
  GEO_join_geometries.hh
  GEO_mesh_bevel.hh
  GEO_mesh_boolean.hh
  GEO_mesh_copy_selection.hh
  GEO_mesh_merge_verts.hh
  GEO_mesh_primitive_cuboid.hh
  GEO_mesh_primitive_cylinder_cone.hh
  GEO_mesh_primitive_grid.hh
  GEO_mesh_primitive_line.hh
  GEO_mesh_primitive_uv_sphere.hh
  GEO_mesh_selection.hh
  GEO_mesh_split_edges.hh
  GEO_mesh_triangulate.hh
  GEO_mix_geometries.hh
  GEO_randomize.hh
  GEO_realize_instances.hh
  GEO_reorder.hh
  GEO_reverse_uv_sampler.hh
  GEO_transform.hh
  GEO_uv_pack.hh
  GEO_uv_parametrizer.hh

  intern/mesh_boolean_intern.hh
  intern/mesh_boolean_manifold.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::dna
  PRIVATE bf::functions
  PRIVATE bf::nodes
  PRIVATE bf::intern::atomic
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::intern::profile
  PRIVATE bf::extern::curve_fit_nd
  PRIVATE bf::dependencies::optional::manifold
  PRIVATE bf::dependencies::optional::gmp
)

if(WITH_UV_SLIM)
  add_definitions(-DWITH_UV_SLIM)
endif()

if(WITH_MANIFOLD)
  add_definitions(-DWITH_MANIFOLD)
endif()

blender_add_lib(bf_geometry "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::geometry ALIAS bf_geometry)

