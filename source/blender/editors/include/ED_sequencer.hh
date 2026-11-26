/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * No-op stubs after Video Sequencer Editor (`space_sequencer`) removal.
 */

#pragma once

#include <optional>

#include "BKE_scene.hh"
#include "BLI_vector_set.hh"
#include "DNA_vec_types.h"

namespace blender {

enum eSeqRippleFlag : int;

struct Scene;
struct Strip;
struct SpaceSeq;
struct bContext;
struct View2D;
struct ARegion;
struct wmOperatorType;
struct Main;
struct WorkSpace;

namespace ed::vse {

enum eStripHandle {
  STRIP_HANDLE_NONE,
  STRIP_HANDLE_LEFT,
  STRIP_HANDLE_RIGHT,
};

struct StripSelection {
  Strip *strip1 = nullptr;
  Strip *strip2 = nullptr;
  eStripHandle handle = STRIP_HANDLE_NONE;
};

inline void select_strip_single(Scene * /*scene*/, Strip * /*strip*/, bool /*deselect_all*/) {}
inline bool deselect_all_strips(const Scene * /*scene*/)
{
  return false;
}

inline bool maskedit_mask_poll(bContext * /*C*/)
{
  return false;
}
inline bool check_show_maskedit(SpaceSeq * /*sseq*/, Scene * /*scene*/)
{
  return false;
}
inline bool maskedit_poll(bContext * /*C*/)
{
  return false;
}

inline bool check_show_imbuf(const SpaceSeq & /*sseq*/)
{
  return false;
}
inline bool check_show_strip(const SpaceSeq & /*sseq*/)
{
  return false;
}
inline bool has_playback_animation(const Scene * /*scene*/)
{
  return false;
}

inline void ED_operatormacros_sequencer() {}

inline Strip *special_preview_get()
{
  return nullptr;
}
inline void special_preview_set(bContext * /*C*/, const int /*mval*/[2]) {}
inline void special_preview_clear() {}

inline VectorSet<Strip *> selected_strips_from_context(bContext * /*C*/)
{
  return {};
}
inline StripSelection pick_strip_and_handle(const struct Scene * /*scene*/,
                                            const View2D * /*v2d*/,
                                            float /*mouse_co*/[2])
{
  return {};
}
inline bool can_select_handle(const Scene * /*scene*/, const Strip * /*strip*/)
{
  return false;
}
inline bool handle_is_selected(const Strip * /*strip*/, eStripHandle /*handle*/)
{
  return false;
}

inline bool is_scene_time_sync_needed(const bContext & /*C*/)
{
  return false;
}
inline bool is_scene_frame_range_from_strip_needed(const bContext & /*C*/)
{
  return false;
}
inline std::optional<ScenePlaybackRange> get_scene_strip_frame_range(const bContext & /*C*/)
{
  return std::nullopt;
}
inline void sync_active_scene_and_time_with_scene_strip(bContext & /*C*/) {}
inline void sync_sequencer_scene_strip_range(Main * /*bmain*/, WorkSpace * /*workspace*/) {}

inline rctf sequencer_clamped_view_bounds_get(const bContext * /*C*/, ARegion * /*region*/)
{
  return {0.0f, 0.0f, 0.0f, 0.0f};
}

inline void operator_properties_ripple(wmOperatorType * /*ot*/, eSeqRippleFlag /*options*/) {}

}  // namespace ed::vse
}  // namespace blender
