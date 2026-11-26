/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscr
 */

#include <cstdlib>
#include <cstring>

#include "BLI_utildefines.hh"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_screen.hh"
#include "BKE_workspace.hh"

#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "ED_object.hh"
#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "screen_intern.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Workspace API
 *
 * \brief API for managing workspaces and their data.
 * \{ */

static void workspace_exit(WorkSpace *workspace, wmWindow *win)
{
  /* Scene pinning: Store whatever scene was active when leaving the workspace. It's reactivated
   * when the workspace gets reactivated as well. */
  if (workspace->flags & WORKSPACE_USE_PIN_SCENE) {
    workspace->pin_scene = WM_window_get_active_scene(win);
  }
  else {
    /* The active scene may have been changed. So also always update the unpinned scene to the
     * latest when leaving a workspace that has no scene pinning. */
    win->unpinned_scene = WM_window_get_active_scene(win);
  }
}

/**
 * State changes (old workspace to new workspace):
 * 1) unpinned -> pinned
 *    * Store current scene as the unpinned one (done in #workspace_exit()).
 *    * Change the current scene to the pinned one.
 * 2) pinned   -> pinned
 *    * Change the current scene to the new pinned one.
 * 3) pinned   -> unpinned
 *    * Change current scene back to the unpinned one
 * 4) unpinned -> unpinned
 *    * Make sure the unpinned scene is active.
 *
 * Note that the pin scene must also be updated when leaving a workspace with a pinned scene.
 * That's done separately via workspace_exit() above.
 */
static void workspace_scene_pinning_update(WorkSpace *workspace_new,
                                           const WorkSpace *workspace_old,
                                           bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  Main *bmain = CTX_data_main(C);
  Scene *active_scene = WM_window_get_active_scene(win);

  const bool is_new_pinned = (workspace_new->flags & WORKSPACE_USE_PIN_SCENE);
  const bool is_old_pinned = (workspace_old->flags & WORKSPACE_USE_PIN_SCENE);

  /* State changes 1 and 2. */
  if (is_new_pinned) {
    if (workspace_new->pin_scene && (workspace_new->pin_scene != active_scene)) {
      WM_window_set_active_scene(bmain, C, win, workspace_new->pin_scene);
      workspace_new->pin_scene = nullptr;
    }
  }
  /* State change 3 - Changing from workspace with pinned scene to unpinned scene. */
  else if (is_old_pinned) {
    if (win->unpinned_scene) {
      WM_window_set_active_scene(bmain, C, win, win->unpinned_scene);
    }
    else {
      /* When leaving a workspace where the pinning was just enabled, the unpinned scene wasn't set
       * yet. */
      win->unpinned_scene = active_scene;
    }
  }
  else {
    /* When leaving a workspace where the pinning was just disabled, we still want to restore the
     * unpinned scene. */
    if (win->unpinned_scene) {
      WM_window_set_active_scene(bmain, C, win, win->unpinned_scene);
    }
  }

  BLI_assert(WM_window_get_active_scene(win));
}

/**
 * Changes the object mode (if needed) to the one set in \a workspace_new.
 * Object mode is still stored on object level. In future it should all be workspace level instead.
 */
static void workspace_change_update(WorkSpace *workspace_new,
                                    WorkSpace *workspace_old,
                                    bContext *C,
                                    wmWindowManager *wm)
{
  workspace_scene_pinning_update(workspace_new, workspace_old, C);
  /* needs to be done before changing mode! (to ensure right context) */
  UNUSED_VARS(wm);
#if 0
  Object *ob_act = CTX_data_active_object(C);
  eObjectMode mode_old = workspace_old->object_mode;
  eObjectMode mode_new = workspace_new->object_mode;

  if (mode_old != mode_new) {
    ed::object::mode_set(C, mode_new);
  }
#endif
}

static WorkSpaceLayout *workspace_change_get_new_layout(Main *bmain,
                                                        WorkSpace *workspace_new,
                                                        wmWindow *win)
{
  WorkSpaceLayout *layout_old = WM_window_get_active_layout(win);
  WorkSpaceLayout *layout_new;

  /* ED_workspace_duplicate may have stored a layout to activate
   * once the workspace gets activated. */
  if (win->workspace_hook->temp_layout_store) {
    layout_new = win->workspace_hook->temp_layout_store;
  }
  else {
    layout_new = BKE_workspace_active_layout_for_workspace_get(win->workspace_hook, workspace_new);
    if (!layout_new) {
      layout_new = workspace_new->layouts.first();
    }
  }

  return ED_workspace_screen_change_ensure_unused_layout(
      bmain, workspace_new, layout_new, layout_old, win);
}

bool ED_workspace_change(WorkSpace *workspace_new, bContext *C, wmWindowManager *wm, wmWindow *win)
{
  Main *bmain = CTX_data_main(C);
  WorkSpace *workspace_old = WM_window_get_active_workspace(win);
  WorkSpaceLayout *layout_new = workspace_change_get_new_layout(bmain, workspace_new, win);
  bScreen *screen_new = BKE_workspace_layout_screen_get(layout_new);
  bScreen *screen_old = BKE_workspace_active_screen_get(win->workspace_hook);

  win->workspace_hook->temp_layout_store = nullptr;
  if (workspace_old == workspace_new) {
    /* Could also return true, everything that needs to be done was done (nothing :P),
     * but nothing changed */
    return false;
  }

  workspace_exit(workspace_old, win);

  screen_change_prepare(screen_old, screen_new, bmain, C, win);

  if (screen_new == nullptr) {
    return false;
  }

  BKE_workspace_active_layout_set(win->workspace_hook, win->winid, workspace_new, layout_new);
  BKE_workspace_active_set(win->workspace_hook, workspace_new);

  /* update screen *after* changing workspace - which also causes the
   * actual screen change and updates context (including CTX_wm_workspace) */
  screen_change_update(C, win, screen_new);
  workspace_change_update(workspace_new, workspace_old, C, wm);

  BLI_assert(CTX_wm_workspace(C) == workspace_new);

  /* Automatic mode switching. */
  if (workspace_new->object_mode != workspace_old->object_mode) {
    const Object *object = nullptr;
    if (const Base *base = CTX_data_active_base(C)) {
      object = base->object;
      /* Behavior that depends on the active area is not expected in the context of workspace
       * switching, ignore the view-port even if it's available. */
      const View3D *v3d = nullptr;

      const bool base_visible = BKE_base_is_visible(v3d, base);
      if (!base_visible && object->mode == OB_MODE_OBJECT) {
        /* Set this to nullptr to indicate that the mode should not be switched. This matches
         * CTX_data_active_object behavior in the 3D Viewport. See `view3d_context` for more
         * details. */
        object = nullptr;
      }
    }

    if (object) {
      ed::object::mode_set(C, eObjectMode(workspace_new->object_mode));
    }
  }

  return true;
}

void ED_workspace_scene_data_sync(WorkSpaceInstanceHook *hook, Scene *scene)
{
  bScreen *screen = BKE_workspace_active_screen_get(hook);
  BKE_screen_view3d_scene_sync(screen, scene);
}

/** \} Workspace API */

}  // namespace blender
