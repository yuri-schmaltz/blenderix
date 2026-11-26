/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscript
 */

#include "WM_api.hh"

#include "ED_script.hh"

#include "script_intern.hh" /* own include */

namespace blender {

/* ************************** registration **********************************/

void ED_operatortypes_script()
{
  WM_operatortype_append(SCRIPT_OT_reload);
}

}  // namespace blender
