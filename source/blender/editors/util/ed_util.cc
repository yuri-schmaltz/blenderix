/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edutil
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "BLI_listbase.hh"
#include "BLI_path_utils.hh"
#include "BLI_string_utf8.hh"

#include "BLT_translation.hh"

#include "BKE_collection.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_remap.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_packedFile.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_undo_system.hh"

#include "DEG_depsgraph.hh"

#include "ED_asset.hh"
#include "ED_gpencil_legacy.hh"
#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_screen_types.hh"
#include "ED_space_api.hh"
#include "ED_util.hh"
#include "ED_view3d.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

namespace blender {

/* ********* general editor util functions, not BKE stuff please! ********* */

void ED_editors_init_for_undo(Main *bmain)
{
  wmWindowManager *wm = bmain->wm.first();
  for (wmWindow &win : wm->windows) {
    /* Stop animation from playing.
     * TODO: There might be a way to keep the animation from playing, but sad->scene and
     * sad->view_layer pointers are outdated and would need to be updated somehow. */
    bScreen *animscreen = ED_screen_animation_playing(wm);
    if (animscreen && animscreen->animtimer) {
      WM_event_timer_remove(wm, &win, animscreen->animtimer);
      animscreen->animtimer = nullptr;
    }

    /* UI Updates. */
    /* Flag local View3D's to check and exit if they are empty. */
    for (bScreen &screen : bmain->screens) {
      for (ScrArea &area : screen.areabase) {
        for (SpaceLink &sl : area.spacedata) {
          if (sl.spacetype == SPACE_VIEW3D) {
            View3D *v3d = reinterpret_cast<View3D *>(&sl);
            if (v3d->localvd) {
              v3d->localvd->runtime.flag |= V3D_RUNTIME_LOCAL_MAYBE_EMPTY;
            }
          }
        }
      }
    }
  }
}

void ED_editors_init(bContext *C)
{
  using namespace blender::ed;
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  wmWindowManager *wm = CTX_wm_manager(C);

  /* This is called during initialization, so we don't want to store any reports */
  ReportList *reports = CTX_wm_reports(C);
  int reports_flag_prev = reports->flag & ~RPT_STORE;

  std::swap(reports->flag, reports_flag_prev);

  /* Don't do undo pushes when calling an operator. */
  wm->op_undo_depth++;

  /* Restore Edit Mode for visible, editable objects of the active type. */
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  for (Object &ob : bmain->objects) {
    if (ob.mode == OB_MODE_OBJECT) {
      continue;
    }
    if (ob.mode != OB_MODE_EDIT) {
      ob.mode = OB_MODE_OBJECT;
      DEG_id_tag_update(&ob.id, ID_RECALC_SYNC_TO_EVAL);
      continue;
    }
    if (BKE_object_has_mode_data(&ob, OB_MODE_EDIT)) {
      /* For multi-edit mode we may already have mode data. */
      continue;
    }

    /* Reset object to Object mode, so that code below can properly re-switch it to its
     * previous mode if possible, re-creating its mode data, etc. */
    ID *ob_data = ob.data;
    ob.mode = OB_MODE_OBJECT;
    DEG_id_tag_update(&ob.id, ID_RECALC_SYNC_TO_EVAL);

    /* Object mode is enforced if there is no active object, or if the active object's type is
     * different. */
    if (obact == nullptr || ob.type != obact->type) {
      continue;
    }
    /* Object mode is enforced for non-editable data (or their obdata). */
    if (!BKE_id_is_editable(bmain, &ob.id) ||
        (ob_data != nullptr && !BKE_id_is_editable(bmain, ob_data)))
    {
      continue;
    }

    if (!BKE_collection_has_object_recursive(scene->master_collection, &ob) ||
        !BKE_scene_has_object(*bmain, scene, &ob) || (ob.visibility_flag & OB_HIDE_VIEWPORT) != 0)
    {
      continue;
    }

    object::editmode_enter_ex(bmain, scene, &ob, 0);
  }

  /* Enforce a full redraw for the first time areas/regions get drawn. Further region init/refresh
   * just triggers non-rebuild redraws (#RGN_DRAW_NO_REBUILD). Usually a full redraw would be
   * triggered by a `NC_WM | ND_FILEREAD` notifier, but if a startup script calls an operator that
   * redraws the window, notifiers are not handled before the operator runs. See #98461. */
  for (wmWindow &win : wm->windows) {
    const bScreen *screen = WM_window_get_active_screen(&win);

    ED_screen_areas_iter (&win, screen, area) {
      ED_area_tag_redraw(area);
    }
  }

  asset::list::storage_tag_main_data_dirty();

  std::swap(reports->flag, reports_flag_prev);
  wm->op_undo_depth--;
}

void ED_editors_exit(Main *bmain, bool do_undo_system)
{
  using namespace blender::ed;
  if (!bmain) {
    return;
  }

  /* Frees all edit-mode undo-steps. */
  if (do_undo_system && G_MAIN->wm.first_) {
    wmWindowManager *wm = G_MAIN->wm.first();
    /* normally we don't check for null undo stack,
     * do here since it may run in different context. */
    if (wm->runtime->undo_stack) {
      BKE_undosys_stack_destroy(wm->runtime->undo_stack);
      wm->runtime->undo_stack = nullptr;
    }
  }

  /* On undo, tag for update so the depsgraph doesn't use stale edit-mode data,
   * this is possible when mixing edit-mode and memory-file undo.
   *
   * By convention, objects are not left in edit-mode - so this isn't often problem in practice,
   * since exiting edit-mode will tag the objects too.
   *
   * However there is no guarantee the active object _never_ changes while in edit-mode.
   * Python for example can do this, some callers to #object::base_activate
   * don't handle modes either (doing so isn't always practical).
   *
   * To reproduce the problem where stale data is used, see: #84920. */
  for (Object &ob : bmain->objects) {
    if (object::editmode_free_ex(bmain, &ob)) {
      if (do_undo_system == false) {
        DEG_id_tag_update(&ob.id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
        DEG_id_tag_update(ob.data, ID_RECALC_GEOMETRY);
      }
    }
  }

  /* global in meshtools... */
  ED_mesh_mirror_spatial_table_end(nullptr);
  ED_mesh_mirror_topo_table_end(nullptr);
}

bool ED_editors_flush_edits_for_object_ex(Main *bmain, Object *ob, bool check_needs_flush)
{
  if (!(ob->mode & OB_MODE_EDIT)) {
    return false;
  }

  char *needs_flush_ptr = BKE_object_data_editmode_flush_ptr_get(ob->data);
  if (needs_flush_ptr) {
    if (check_needs_flush && !*needs_flush_ptr) {
      return false;
    }
    *needs_flush_ptr = 0;
  }

  ed::object::editmode_load(bmain, ob);
  return true;
}

bool ED_editors_flush_edits_for_object(Main *bmain, Object *ob)
{
  return ED_editors_flush_edits_for_object_ex(bmain, ob, false);
}

bool ED_editors_flush_edits_ex(Main *bmain, bool check_needs_flush)
{
  bool has_edited = false;

  /* The active object may be unavailable, and multiple objects can be in Edit Mode. */
  for (Object &ob : bmain->objects) {
    has_edited |= ED_editors_flush_edits_for_object_ex(bmain, &ob, check_needs_flush);
  }

  bmain->is_memfile_undo_flush_needed = false;

  return has_edited;
}

bool ED_editors_flush_edits(Main *bmain)
{
  return ED_editors_flush_edits_ex(bmain, false);
}

/* ***** XXX: functions are using old blender names, cleanup later ***** */

void apply_keyb_grid(
    bool shift, bool ctrl, float *val, float fac1, float fac2, float fac3, int invert)
{
  /* fac1 is for 'nothing', fac2 for CTRL, fac3 for SHIFT */
  if (invert) {
    ctrl = !ctrl;
  }

  if (ctrl && shift) {
    if (fac3 != 0.0f) {
      *val = fac3 * floorf(*val / fac3 + 0.5f);
    }
  }
  else if (ctrl) {
    if (fac2 != 0.0f) {
      *val = fac2 * floorf(*val / fac2 + 0.5f);
    }
  }
  else {
    if (fac1 != 0.0f) {
      *val = fac1 * floorf(*val / fac1 + 0.5f);
    }
  }
}

void unpack_menu(bContext *C,
                 const char *opname,
                 const char *id_name,
                 const char *abs_name,
                 const char *folder,
                 PackedFile *pf)
{
  Main *bmain = CTX_data_main(C);
  char line[FILE_MAX + 100];
  wmOperatorType *ot = WM_operatortype_find(opname, true);
  const char *blendfile_path = BKE_main_blendfile_path(bmain);

  ui::PopupMenu *pup = ui::popup_menu_begin(C, IFACE_("Unpack File"), ICON_NONE);
  ui::Layout &layout = *popup_menu_layout(pup);

  PointerRNA props_ptr = layout.op(
      ot, IFACE_("Remove Pack"), ICON_NONE, wm::OpCallContext::ExecDefault, UI_ITEM_NONE);
  RNA_enum_set(&props_ptr, "method", PF_REMOVE);
  RNA_string_set(&props_ptr, "id", id_name);

  if (blendfile_path[0] != '\0') {
    char local_name[FILE_MAXDIR + FILE_MAX], fi[FILE_MAX];

    BLI_path_split_file_part(abs_name, fi);
    BLI_path_join(local_name, "//", folder, fi);
    if (!STREQ(abs_name, local_name)) {
      switch (BKE_packedfile_compare_to_file(blendfile_path, local_name, pf)) {
        case PF_CMP_NOFILE:
          SNPRINTF_UTF8(line, IFACE_("Create %s"), local_name);
          props_ptr = layout.op(ot, line, ICON_NONE, wm::OpCallContext::ExecDefault, UI_ITEM_NONE);
          RNA_enum_set(&props_ptr, "method", PF_WRITE_LOCAL);
          RNA_string_set(&props_ptr, "id", id_name);

          break;
        case PF_CMP_EQUAL:
          SNPRINTF_UTF8(line, IFACE_("Use %s (identical)"), local_name);
          props_ptr = layout.op(ot, line, ICON_NONE, wm::OpCallContext::ExecDefault, UI_ITEM_NONE);
          RNA_enum_set(&props_ptr, "method", PF_USE_LOCAL);
          RNA_string_set(&props_ptr, "id", id_name);

          break;
        case PF_CMP_DIFFERS:
          SNPRINTF_UTF8(line, IFACE_("Use %s (differs)"), local_name);
          props_ptr = layout.op(ot, line, ICON_NONE, wm::OpCallContext::ExecDefault, UI_ITEM_NONE);
          RNA_enum_set(&props_ptr, "method", PF_USE_LOCAL);
          RNA_string_set(&props_ptr, "id", id_name);

          SNPRINTF_UTF8(line, IFACE_("Overwrite %s"), local_name);
          props_ptr = layout.op(ot, line, ICON_NONE, wm::OpCallContext::ExecDefault, UI_ITEM_NONE);
          RNA_enum_set(&props_ptr, "method", PF_WRITE_LOCAL);
          RNA_string_set(&props_ptr, "id", id_name);
          break;
      }
    }
  }

  switch (BKE_packedfile_compare_to_file(blendfile_path, abs_name, pf)) {
    case PF_CMP_NOFILE:
      SNPRINTF_UTF8(line, IFACE_("Create %s"), abs_name);
      props_ptr = layout.op(ot, line, ICON_NONE, wm::OpCallContext::ExecDefault, UI_ITEM_NONE);
      RNA_enum_set(&props_ptr, "method", PF_WRITE_ORIGINAL);
      RNA_string_set(&props_ptr, "id", id_name);
      break;
    case PF_CMP_EQUAL:
      SNPRINTF_UTF8(line, IFACE_("Use %s (identical)"), abs_name);
      props_ptr = layout.op(ot, line, ICON_NONE, wm::OpCallContext::ExecDefault, UI_ITEM_NONE);
      RNA_enum_set(&props_ptr, "method", PF_USE_ORIGINAL);
      RNA_string_set(&props_ptr, "id", id_name);
      break;
    case PF_CMP_DIFFERS:
      SNPRINTF_UTF8(line, IFACE_("Use %s (differs)"), abs_name);
      props_ptr = layout.op(ot, line, ICON_NONE, wm::OpCallContext::ExecDefault, UI_ITEM_NONE);
      RNA_enum_set(&props_ptr, "method", PF_USE_ORIGINAL);
      RNA_string_set(&props_ptr, "id", id_name);

      SNPRINTF_UTF8(line, IFACE_("Overwrite %s"), abs_name);
      props_ptr = layout.op(ot, line, ICON_NONE, wm::OpCallContext::ExecDefault, UI_ITEM_NONE);
      RNA_enum_set(&props_ptr, "method", PF_WRITE_ORIGINAL);
      RNA_string_set(&props_ptr, "id", id_name);
      break;
  }

  popup_menu_end(C, pup);
}

void ED_spacedata_id_remap(ScrArea *area, SpaceLink *sl, const bke::id::IDRemapper &mappings)
{
  SpaceType *st = BKE_spacetype_from_id(sl->spacetype);
  if (st && st->id_remap) {
    st->id_remap(area, sl, mappings);
  }
}

void ED_spacedata_id_remap_single(ScrArea *area, SpaceLink *sl, ID *old_id, ID *new_id)
{
  SpaceType *st = BKE_spacetype_from_id(sl->spacetype);

  if (st && st->id_remap) {
    bke::id::IDRemapper mappings;
    mappings.add(old_id, new_id);
    st->id_remap(area, sl, mappings);
  }
}

}  // namespace blender
