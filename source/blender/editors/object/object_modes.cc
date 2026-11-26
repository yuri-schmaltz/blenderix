/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 *
 * General utils to handle mode switching,
 * actual mode switching logic is per-object type.
 */

#include "DNA_object_enums.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_time.hh"
#include "BLI_utildefines.hh"

#include "BKE_context.hh"
#include "BKE_idtype.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "BLI_string.hh"
#include "BLI_string_utf8.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "ED_outliner.hh"
#include "ED_undo.hh"
#include "ED_view3d.hh"

#include "WM_toolsystem.hh"

#include "ED_object.hh" /* own include */
#include "object_intern.hh"

namespace blender::ed::object {

/* -------------------------------------------------------------------- */
/** \name High Level Mode Operations
 * \{ */

static const char *object_mode_op_string(eObjectMode mode)
{
  return (mode == OB_MODE_EDIT) ? "OBJECT_OT_editmode_toggle" : nullptr;
}

bool mode_compat_test(const Object *ob, eObjectMode mode)
{
  return mode == OB_MODE_OBJECT || (mode == OB_MODE_EDIT && OB_TYPE_SUPPORT_EDITMODE(ob->type));
}

bool mode_compat_set(bContext *C, Object *ob, eObjectMode mode, ReportList *reports)
{
  bool ok;
  if (!ELEM(ob->mode, mode, OB_MODE_OBJECT)) {
    const char *opstring = object_mode_op_string(ob->mode);
    if (!opstring) {
      BKE_report(reports, RPT_ERROR, "Unsupported object mode");
      return false;
    }

    WM_operator_name_call(C, opstring, wm::OpCallContext::ExecRegionWin, nullptr, nullptr);
    ok = ELEM(ob->mode, mode, OB_MODE_OBJECT);
    if (!ok) {
      wmOperatorType *ot = WM_operatortype_find(opstring, false);
      BKE_reportf(reports, RPT_ERROR, "Unable to execute '%s', error changing modes", ot->name);
    }
  }
  else {
    ok = true;
  }

  return ok;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic Mode Enter/Exit
 *
 * Supports exiting a mode without it being in the current context.
 * This could be done for entering modes too if it's needed.
 *
 * \{ */

bool mode_set_ex(bContext *C, eObjectMode mode, bool use_undo, ReportList *reports)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  const Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  if (ob == nullptr) {
    return (mode == OB_MODE_OBJECT);
  }

  if (!mode_compat_test(ob, mode)) {
    return false;
  }

  if (ob->mode == mode) {
    return true;
  }

  const char *opstring = object_mode_op_string((mode == OB_MODE_OBJECT) ? ob->mode : mode);
  if (opstring == nullptr) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Unable to set invalid object mode %d",
                int((mode == OB_MODE_OBJECT) ? ob->mode : mode));
    return false;
  }
  wmOperatorType *ot = WM_operatortype_find(opstring, false);

  if (!use_undo) {
    wm->op_undo_depth++;
  }
  WM_operator_name_call_ptr(C, ot, wm::OpCallContext::ExecRegionWin, nullptr, nullptr);
  if (!use_undo) {
    wm->op_undo_depth--;
  }

  if (ob->mode != mode) {
    /* Give more specific error messages for cases that are known to fail (like linked and packed
     * object-data). */
    if (ob->data && !ID_IS_EDITABLE(ob->data)) {
      const ID &obdata_id = *ob->data;
      char obdata_idtype_name_lower[MAX_ID_NAME];
      const size_t obdata_idtype_name_lower_len = STRNCPY_UTF8_RLEN(
          obdata_idtype_name_lower, BKE_idtype_idcode_to_name(GS(obdata_id.name)));
      BLI_str_tolower_ascii(obdata_idtype_name_lower, obdata_idtype_name_lower_len);

      if (ID_IS_PACKED(ob->data)) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "The '%s' %s data-block is packed and not editable. Use \"Make Local\" to "
                    "make it editable.",
                    BKE_id_name(obdata_id),
                    obdata_idtype_name_lower);
        return false;
      }
      if (ID_IS_LINKED(ob->data)) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "The '%s' %s data-block is linked and not editable. Use \"Make Local\" to "
                    "make it editable.",
                    BKE_id_name(obdata_id),
                    obdata_idtype_name_lower);
        return false;
      }
    }

    BKE_reportf(reports, RPT_ERROR, "Unable to execute '%s', error changing modes", ot->name);
    return false;
  }

  return true;
}

bool mode_set(bContext *C, eObjectMode mode)
{
  /* Don't do undo push by default, since this may be called by lower level code. */
  return mode_set_ex(C, mode, true, nullptr);
}

void mode_generic_exit(Main *bmain, Depsgraph * /*depsgraph*/, Scene *scene, Object *ob)
{
  if (BKE_object_is_in_editmode(ob)) {
    editmode_exit_ex(bmain, scene, ob, EM_FREEDATA);
  }
}

bool mode_generic_has_data(Depsgraph * /*depsgraph*/, const Object *ob)
{
  return BKE_object_is_in_editmode(ob);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transfer Mode
 *
 * Enters the same mode of the current active object in another object,
 * leaving the mode of the current object.
 * \{ */

static bool object_transfer_mode_poll(bContext *C)
{
  if (!CTX_wm_region_view3d(C)) {
    return false;
  }
  const Object *ob = CTX_data_active_object(C);
  return ob && ob->mode == OB_MODE_EDIT;
}

constexpr float mode_transfer_flash_length = 0.55f;

static auto &mode_transfer_overlay_start_times()
{
  static Map<std::string, double> map;
  return map;
}

static float alpha_from_time_get(const float anim_time)
{
  if (anim_time < 0.0f) {
    return 0.0f;
  }
  return (1.0f - (anim_time / mode_transfer_flash_length));
}

Map<std::string, float, 1> mode_transfer_overlay_current_state()
{
  const double now = BLI_time_now_seconds();

  /* Protect against possible concurrent access from multiple renderers or viewports. */
  static Mutex mutex;
  std::scoped_lock lock(mutex);

  /* Remove finished animations form the global map. */
  Map<std::string, double> &start_times = mode_transfer_overlay_start_times();
  start_times.remove_if(
      [&](const auto &item) { return (now - item.value) > mode_transfer_flash_length; });

  Map<std::string, float, 1> factors;
  for (const auto &item : start_times.items()) {
    const float alpha = alpha_from_time_get(now - item.value);
    if (alpha > 0.0f) {
      factors.add_new(item.key, alpha);
    }
  }
  return factors;
}

static void object_overlay_mode_transfer_animation_start(bContext *C, Object *ob_dst)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob_dst_eval = DEG_get_evaluated(depsgraph, ob_dst);
  mode_transfer_overlay_start_times().add_as(ob_dst_eval->id.name, BLI_time_now_seconds());
}

static bool object_transfer_mode_to_base(bContext *C,
                                         wmOperator *op,
                                         Scene *scene,
                                         Object * /*ob_src*/,
                                         Object *ob_dst,
                                         const eObjectMode mode_dst)
{
  const Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  /* Undo is handled manually here, such that the entry in the user-visible undo history is named
   * from the expected mode toggle operator name, and not the 'Transfer Mode' operator itself.
   *
   * The undo grouping is needed to ensure that only one step is visible, even though there may be
   * two undo steps stored when executed successfully (moving source object to Object mode, and
   * then target object to the previous mode of source object). */
  ED_undo_group_begin(C);

  const bool mode_transferred = mode_set_ex(C, OB_MODE_OBJECT, true, op->reports);
  if (mode_transferred) {
    BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
    Base *base_dst = BKE_view_layer_base_find(view_layer, ob_dst);
    BKE_view_layer_base_deselect_all(*bmain, scene, view_layer);
    BKE_view_layer_base_select_and_set_active(view_layer, base_dst);

    /* Not entirely clear why, but this extra undo step (the two calls to #mode_set_ex should
     * already create their own) is required. Otherwise some mode switching does not work as
     * expected on undo/redo (see #130420 with Sculpt mode). */
    ED_undo_push(C, "Change Active");

    mode_set_ex(C, mode_dst, true, op->reports);

    if (RNA_boolean_get(op->ptr, "use_flash_on_transfer")) {
      object_overlay_mode_transfer_animation_start(C, ob_dst);
    }
  }

  ED_undo_group_end(C);

  return mode_transferred;
}

static wmOperatorStatus object_transfer_mode_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob_src = CTX_data_active_object(C);
  const eObjectMode mode_src = ob_src->mode;

  Base *base_dst = ED_view3d_give_base_under_cursor(C, event->mval);
  if (!base_dst) {
    BKE_reportf(op->reports, RPT_ERROR, "No target object to transfer the mode to");
    return OPERATOR_CANCELLED;
  }

  Object *ob_dst = base_dst->object;

  if (ob_src == ob_dst) {
    return OPERATOR_CANCELLED;
  }

  BLI_assert(ob_dst->id.orig_id == nullptr);
  if (!ID_IS_EDITABLE(ob_dst) || !ID_IS_EDITABLE(ob_src)) {
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Unable to transfer mode, the source and/or target objects are not editable");
    return OPERATOR_CANCELLED;
  }
  if (ID_IS_OVERRIDE_LIBRARY(ob_dst)) {
    BKE_reportf(
        op->reports,
        RPT_ERROR,
        "Current mode of source object '%s' is not compatible with target liboverride object '%s'",
        ob_src->id.name + 2,
        ob_dst->id.name + 2);
    return OPERATOR_CANCELLED;
  }
  if (!mode_compat_test(ob_dst, mode_src)) {
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Current mode of source object '%s' is not compatible with target object '%s'",
                ob_src->id.name + 2,
                ob_dst->id.name + 2);
    return OPERATOR_CANCELLED;
  }

  const bool mode_transferred = object_transfer_mode_to_base(
      C, op, scene, ob_src, ob_dst, mode_src);
  if (!mode_transferred) {
    /* Error report should have been set by #object_transfer_mode_to_base call here. */
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  ED_outliner_select_sync_from_object_tag(C);

  WM_toolsystem_update_from_context_view3d(C);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_transfer_mode(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Transfer Mode";
  ot->idname = "OBJECT_OT_transfer_mode";
  ot->description =
      "Switches the active object and assigns the same mode to a new one under the mouse cursor, "
      "leaving the active mode in the current one";

  /* API callbacks. */
  ot->invoke = object_transfer_mode_invoke;
  ot->poll = object_transfer_mode_poll;

  /* Undo push is handled by the operator, see #object_transfer_mode_to_base for details. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_DEPENDS_ON_CURSOR;

  ot->cursor_pending = WM_CURSOR_EYEDROPPER;

  RNA_def_boolean(ot->srna,
                  "use_flash_on_transfer",
                  true,
                  "Flash On Transfer",
                  "Flash the target object when transferring the mode");
}

/** \} */

}  // namespace blender::ed::object
