# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  ../editors/include
  ../makesrna

  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  ${CMAKE_SOURCE_DIR}/release/datafiles/userdef/userdef_default_theme.c
  intern/blend_validate.cc
  intern/readblenentry.cc
  intern/readfile.cc
  intern/readfile_tempload.cc
  intern/undofile.cc
  intern/versioning_common.cc
  intern/versioning_defaults.cc
  intern/versioning_dna.cc
  intern/versioning_userdef.cc
  intern/writefile.cc

  BLO_blend_validate.hh
  BLO_read_write.hh
  BLO_readfile.hh
  BLO_undofile.hh
  BLO_userdef_default.h
  BLO_writefile.hh
  versioning_common.hh
  intern/readfile.hh
  intern/writefile.hh
)

set(LIB
  PRIVATE bf::animrig
  PRIVATE bf::asset_system
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PUBLIC bf::blenloader_core
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::draw
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::imbuf::movie
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::intern::memutil
  PRIVATE bf::nodes
  PRIVATE bf::render
  PRIVATE bf::windowmanager
  PRIVATE bf::extern::xxhash
  PRIVATE bf::dependencies::zstd
  PRIVATE bf::sequencer
)

if(WITH_BUILDINFO)
  add_definitions(-DWITH_BUILDINFO)
endif()

if(WITH_ALEMBIC)
  add_definitions(-DWITH_ALEMBIC)
endif()

blender_add_lib(bf_blenloader "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::blenloader ALIAS bf_blenloader)

# RNA_prototypes.hh
add_dependencies(bf_blenloader bf_rna)

if(WITH_GTESTS)
  # Utility functions for test also used by other tests.
  set(TEST_UTIL_SRC
    tests/blendfile_loading_base_test.cc
    tests/blendfile_loading_base_test.h
  )
  set(TEST_UTIL_INC
    ${INC}
    ../../../intern/ghost
    ${CMAKE_SOURCE_DIR}/tests/gtests
  )
  set(TEST_UTIL_INC_SYS
    ${INC_SYS}
  )
  set(TEST_UTIL_LIB
    ${LIB}
    PRIVATE bf::blenfont
    bf_blenloader
    PRIVATE bf::dependencies::gflags
    PRIVATE bf::dependencies::glog
  )
  blender_add_lib(bf_blenloader_test_util "${TEST_UTIL_SRC}" "${TEST_UTIL_INC}" "${TEST_UTIL_INC_SYS}" "${TEST_UTIL_LIB}")

  # Actual `blenloader` tests.
  set(TEST_SRC
    tests/blendfile_load_test.cc
    tests/sdna_encode_subset_test.cc
  )
  set(TEST_LIB
    ${LIB}
    bf_blenloader
    bf_blenloader_test_util
  )
  blender_add_test_suite_lib(blenloader "${TEST_SRC}" "${INC}" "${INC_SYS}" "${TEST_LIB}")
endif()

if(WITH_EXPERIMENTAL_FEATURES)
  add_definitions(-DWITH_EXPERIMENTAL_FEATURES)
endif()
