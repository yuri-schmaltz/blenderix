/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "BLI_math_vector_c.hh"
#include "BLI_utildefines.hh"

#include "BKE_context.hh"
#include "BKE_screen.hh"
#include "BKE_view3d_runtime.hh"

#include "ED_gizmo_library.hh"
#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "view3d_intern.hh" /* own include */

namespace blender {

/* -------------------------------------------------------------------- */
/** \name View3D Navigation Gizmo Group
 * \{ */

/* Size of main icon. */
#define GIZMO_SIZE U.gizmo_size_navigate_v3d

/* Main gizmo offset from screen edges in unscaled pixels. */
#define GIZMO_OFFSET 10.0f

namespace {

enum {
  GZ_INDEX_ROTATE = 0,

  GZ_INDEX_TOTAL = 1,
};

struct NavigateGizmoInfo {
  const char *opname;
  const char *gizmo;
  uint icon;
  void (*op_prop_fn)(PointerRNA *ptr);
};

struct NavigateWidgetGroup {
  wmGizmo *gz_array[GZ_INDEX_TOTAL];
  /* Store the view state to check for changes. */
  struct {
    rcti rect_visible;
    struct {
      eRegionView3D_ViewLock viewlock;
    } rv3d;
  } state;
};

}  // namespace

static NavigateGizmoInfo g_navigate_params[GZ_INDEX_TOTAL] = {
    {
        "VIEW3D_OT_rotate",
        "VIEW3D_GT_navigate_rotate",
        ICON_NONE,
        nullptr,
    },
};

static bool WIDGETGROUP_navigate_poll(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  View3D *v3d = CTX_wm_view3d(C);
  if ((U.mini_axis_type != USER_MINI_AXIS_TYPE_GIZMO) ||
      (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_NAVIGATE)))
  {
    return false;
  }
  return true;
}

static void WIDGETGROUP_navigate_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
  NavigateWidgetGroup *navgroup = MEM_new<NavigateWidgetGroup>(__func__);

  wmOperatorType *ot_view_axis = WM_operatortype_find("VIEW3D_OT_view_axis", true);

  for (int i = 0; i < GZ_INDEX_TOTAL; i++) {
    const NavigateGizmoInfo *info = &g_navigate_params[i];
    navgroup->gz_array[i] = WM_gizmo_new(info->gizmo, gzgroup, nullptr);
    wmGizmo *gz = navgroup->gz_array[i];
    gz->flag |= WM_GIZMO_MOVE_CURSOR | WM_GIZMO_DRAW_MODAL;

    gz->color[3] = 0.0f;
    copy_v3_fl(gz->color_hi, 0.5f);
    gz->color_hi[3] = 0.5f;

    wmOperatorType *ot = WM_operatortype_find(info->opname, true);
#ifndef WITH_PYTHON
    if (ot != nullptr)
#endif
    {
      PointerRNA *ptr = WM_gizmo_operator_set(gz, 0, ot, nullptr);
      if (info->op_prop_fn != nullptr) {
        info->op_prop_fn(ptr);
      }
    }
  }

  {
    wmGizmo *gz = navgroup->gz_array[GZ_INDEX_ROTATE];
    gz->scale_basis = GIZMO_SIZE / 2.0f;
    const char mapping[6] = {
        RV3D_VIEW_LEFT,
        RV3D_VIEW_RIGHT,
        RV3D_VIEW_FRONT,
        RV3D_VIEW_BACK,
        RV3D_VIEW_BOTTOM,
        RV3D_VIEW_TOP,
    };

    for (int part_index = 0; part_index < 6; part_index += 1) {
      PointerRNA *ptr = WM_gizmo_operator_set(gz, part_index + 1, ot_view_axis, nullptr);
      RNA_enum_set(ptr, "type", mapping[part_index]);
    }

    /* When dragging an axis, use this instead. */
    wmWindowManager *wm = CTX_wm_manager(C);
    gz->keymap = WM_gizmo_keymap_generic_press_drag(wm);
    gz->drag_part = 0;
  }

  gzgroup->customdata = navgroup;
}

static void WIDGETGROUP_navigate_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  NavigateWidgetGroup *navgroup = static_cast<NavigateWidgetGroup *>(gzgroup->customdata);
  ARegion *region = CTX_wm_region(C);
  const RegionView3D *rv3d = static_cast<const RegionView3D *>(region->regiondata);

  for (int i = 0; i < 3; i++) {
    copy_v3_v3(navgroup->gz_array[GZ_INDEX_ROTATE]->matrix_offset[i], rv3d->viewmat[i]);
  }

  const rcti *rect_visible = ED_region_visible_rect(region);

  /* Ensure types match so bits are never lost on assignment. */
  CHECK_TYPE_PAIR(navgroup->state.rv3d.viewlock, rv3d->viewlock);

  if ((navgroup->state.rect_visible.xmax == rect_visible->xmax) &&
      (navgroup->state.rect_visible.ymax == rect_visible->ymax) &&
      (navgroup->state.rv3d.viewlock == bke::region_view3d_lock_flags_get(rv3d)))
  {
    return;
  }

  navgroup->state.rect_visible = *rect_visible;
  navgroup->state.rv3d.viewlock = bke::region_view3d_lock_flags_get(rv3d);

  const bool show_rotate_gizmo = (region->alignment != RGN_ALIGN_QSPLIT ||
                                  region->runtime->quadview_index ==
                                      bke::ARegionQuadviewIndex::TopRight);
  const float icon_offset = UI_SCALE_FAC * (GIZMO_OFFSET + GIZMO_SIZE / 2.0f);
  const float co_rotate[2] = {
      rect_visible->xmax - icon_offset,
      rect_visible->ymax - icon_offset,
  };

  wmGizmo *gz = navgroup->gz_array[GZ_INDEX_ROTATE];
  gz->matrix_basis[3][0] = roundf(co_rotate[0]);
  gz->matrix_basis[3][1] = roundf(co_rotate[1]);
  WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, !show_rotate_gizmo);
}

void VIEW3D_GGT_navigate(wmGizmoGroupType *gzgt)
{
  gzgt->name = "View3D Navigate";
  gzgt->idname = "VIEW3D_GGT_navigate";

  gzgt->flag |= (WM_GIZMOGROUPTYPE_PERSISTENT | WM_GIZMOGROUPTYPE_SCALE |
                 WM_GIZMOGROUPTYPE_DRAW_MODAL_ALL);

  gzgt->poll = WIDGETGROUP_navigate_poll;
  gzgt->setup = WIDGETGROUP_navigate_setup;
  gzgt->draw_prepare = WIDGETGROUP_navigate_draw_prepare;
  gzgt->draw_background = ED_gizmo_button2d_group_background;
}

/** \} */

}  // namespace blender
