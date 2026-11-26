/* SPDX-FileCopyrightText: 2009 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.hh"

#include "DNA_scene_types.h"

#include "BKE_paint.hh"
#include "BKE_paint_types.hh"

namespace blender {

void BKE_paint_settings_foreach_mode(ToolSettings *ts, FunctionRef<void(Paint &paint)> fn)
{
  if (ts->vpaint) {
    fn(*reinterpret_cast<Paint *>(ts->vpaint));
  }
  if (ts->wpaint) {
    fn(*reinterpret_cast<Paint *>(ts->wpaint));
  }
  if (ts->sculpt) {
    fn(*reinterpret_cast<Paint *>(ts->sculpt));
  }
  if (ts->gp_paint) {
    fn(*reinterpret_cast<Paint *>(ts->gp_paint));
  }
  if (ts->gp_vertexpaint) {
    fn(*reinterpret_cast<Paint *>(ts->gp_vertexpaint));
  }
  if (ts->gp_sculptpaint) {
    fn(*reinterpret_cast<Paint *>(ts->gp_sculptpaint));
  }
  if (ts->gp_weightpaint) {
    fn(*reinterpret_cast<Paint *>(ts->gp_weightpaint));
  }
  if (ts->curves_sculpt) {
    fn(*reinterpret_cast<Paint *>(ts->curves_sculpt));
  }
  fn(*reinterpret_cast<Paint *>(&ts->imapaint));
}

void BKE_paint_foreach_asset_weak_reference(Paint &paint,
                                            FunctionRef<void(AssetWeakReference &weak_ref)> fn)
{
  const auto foreach_reference = [&](AssetWeakReference *weak_ref) {
    if (weak_ref) {
      fn(*weak_ref);
    }
  };

  foreach_reference(paint.brush_asset_reference);
  if (paint.runtime) {
    foreach_reference(paint.runtime->previous_active_brush_reference);
  }

  foreach_reference(paint.tool_brush_bindings.main_brush_asset_reference);
  for (NamedBrushAssetReference &brush_binding :
       paint.tool_brush_bindings.active_brush_per_brush_type)
  {
    foreach_reference(brush_binding.brush_asset_reference);
  }
}

}  // namespace blender
