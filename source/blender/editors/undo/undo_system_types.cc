/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edundo
 */

#include <cstring>

#include "ED_armature.hh"
#include "ED_curve.hh"
#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_lattice.hh"
#include "ED_mball.hh"
#include "ED_mesh.hh"
#include "ED_paint.hh"
#include "ED_particle.hh"
#include "ED_pointcloud.hh"
#include "ED_sculpt.hh"
#include "ED_undo.hh"

#include "undo_intern.hh"

/* Keep last */
#include "BKE_undo_system.hh"

namespace blender {

void ED_undosys_type_init()
{
  /* Edit Modes */
  BKE_undosys_type_append(ED_mesh_undosys_type);

  /* Keep global undo last (as a fallback). */
  BKE_UNDOSYS_TYPE_MEMFILE = BKE_undosys_type_append(ED_memfile_undosys_type);
}

void ED_undosys_type_free()
{
  BKE_undosys_type_free_all();
}

}  // namespace blender
