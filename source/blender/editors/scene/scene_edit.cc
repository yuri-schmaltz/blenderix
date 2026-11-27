/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscene
 */

#include "BLI_listbase.hh"
#include "BLI_math_base_c.hh"

#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "ED_render.hh"
#include "ED_scene.hh"
#include "ED_screen.hh"
#include "ED_util.hh"

#include "WM_api.hh"
#include "WM_types.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Scene Utilities
 * \{ */

bool ED_scene_replace_active_for_deletion(bContext &C, Main &bmain, Scene &scene, Scene *scene_new)
{
  BLI_assert(!scene_new || &scene != scene_new);
  if (!BKE_scene_can_be_removed(&bmain, &scene)) {
    return false;
  }

  if (!scene_new) {
    scene_new = BKE_scene_find_replacement(bmain, scene);
  }
  if (!scene_new) {
    return false;
  }

  /* NOTE: Usages of BPy_..._ALLOW_THREADS macros below are necessary because this code is also
   * called from RNA (and therefore BPY). */

  /* Cancel animation playback.
   *
   * Note that #ED_screen_animation_playing will also return a valid
   * screen when user is scrubbing, in which case `bScreen::animtimer` is `nullptr`.
   */
  bScreen *screen = ED_screen_animation_playing(CTX_wm_manager(&C));
  if (screen && screen->animtimer) {
    ScreenAnimData *sad = static_cast<ScreenAnimData *>(screen->animtimer->customdata);
    if (sad->scene == &scene) {
#ifdef WITH_PYTHON
      BPy_BEGIN_ALLOW_THREADS;
#endif
      ED_screen_animation_play(&C, 0, 0);
#ifdef WITH_PYTHON
      BPy_END_ALLOW_THREADS;
#endif
    }
  }

  /* Kill running jobs. */
  wmWindowManager *wm = bmain.wm.first();
  WM_jobs_kill_all_from_owner(wm, &scene);

  for (wmWindow &win : wm->windows) {
    if (win.parent != nullptr) { /* We only care about main windows here... */
      continue;
    }
    if (win.scene == &scene) {
#ifdef WITH_PYTHON
      BPy_BEGIN_ALLOW_THREADS;
#endif
      WM_window_set_active_scene(&bmain, &C, &win, scene_new);
#ifdef WITH_PYTHON
      BPy_END_ALLOW_THREADS;
#endif
    }
  }

  /* Update scenes used by the sequencer. */
  for (WorkSpace &workspace : bmain.workspaces) {
    if (workspace.sequencer_scene == &scene) {
      workspace.sequencer_scene = scene_new;
      WM_event_add_notifier(&C, NC_WINDOW, nullptr);
    }
  }

  /* In theory, the call to #WM_window_set_active_scene above should have handled this through
   * calls to #ED_screen_scene_change. But there can be unusual cases (e.g. on file opening in
   * background mode) where the state of available Windows may prevent this from happening. */
  if (CTX_data_scene(&C) == &scene) {
#ifdef WITH_PYTHON
    BPy_BEGIN_ALLOW_THREADS;
#endif
    CTX_data_scene_set(&C, scene_new);
#ifdef WITH_PYTHON
    BPy_END_ALLOW_THREADS;
#endif
  }

  return true;
}

void ED_scene_change_update(Main *bmain, Scene *scene, ViewLayer *layer)
{
  Depsgraph *depsgraph = BKE_scene_ensure_depsgraph(bmain, scene, layer);
  /* When switching to a scene for the first time after loading a file, the dependency graph above
   * will not be active. This can cause issues (e.g. #151159) because there might be code running
   * immediately after that expects this dependency graph to be active and then silently fail.
   * Similar to #CTX_data_depsgraph_pointer. */
  DEG_make_active(depsgraph);

  BKE_scene_set_background(bmain, scene);
  DEG_graph_relations_update(depsgraph);
  DEG_tag_on_visible_update(bmain, false);

  ED_render_engine_changed(bmain, false);
  ED_update_for_newframe(bmain, depsgraph);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Effective Playback Range
 * \{ */

ScenePlaybackRange ED_scene_get_effective_playback_range(const bContext & /*C*/, const Scene *scene)
{
  if (scene->r.flag & SCER_PRV_RANGE) {
    return {scene->r.psfra, scene->r.pefra};
  }

  return BKE_scene_get_playback_range(scene);
}

void ED_scene_frame_clamp_for_playback(const bContext &C,
                                       Scene *scene,
                                       const bool is_playing_forward)
{
  const ScenePlaybackRange range = ED_scene_get_effective_playback_range(C, scene);
  if (is_playing_forward) {
    if (scene->r.cfra > range.end_frame) {
      scene->r.cfra = range.start_frame;
    }
  }
  else {
    if (scene->r.cfra < range.start_frame) {
      scene->r.cfra = range.end_frame;
    }
  }
  if (!(scene->r.flag & SCER_ALLOW_PREROLL)) {
    scene->r.cfra = clamp_i(scene->r.cfra, range.start_frame, range.end_frame);
  }
}

/** \} */

}  // namespace blender
