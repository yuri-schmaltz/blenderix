# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../io
  ../../makesrna
)

set(INC_SYS
)

set(SRC
  spacetypes.cc
)

set(LIB
  PRIVATE bf::asset_system
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  bf_editor_script
  bf_editor_space_buttons
  bf_editor_space_file
  bf_editor_space_info
  bf_editor_space_outliner
  bf_editor_space_statusbar
  bf_editor_space_topbar
  bf_editor_space_userpref
  bf_editor_space_view3d
  bf_editor_util
  PRIVATE bf::gpu
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::nodes
  PRIVATE bf::windowmanager
)

blender_add_lib(bf_editor_space_api "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
