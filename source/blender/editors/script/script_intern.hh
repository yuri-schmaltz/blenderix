/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscript
 */

#pragma once

namespace blender {

/* internal exports only */

struct wmOperatorType;

/* `script_edit.cc` */

void SCRIPT_OT_reload(wmOperatorType *ot);

}  // namespace blender
