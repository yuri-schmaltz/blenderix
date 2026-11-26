/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * No-op stubs after Movie Clip Editor (`space_clip`) removal.
 */

#pragma once

struct ARegion;
struct ImBuf;
struct Main;
struct Mask;
struct MovieClip;
struct SpaceClip;
struct bContext;
struct bScreen;

namespace blender {

inline bool ED_space_clip_poll(bContext * /*C*/)
{
  return false;
}
inline bool ED_space_clip_view_clip_poll(bContext * /*C*/)
{
  return false;
}
inline bool ED_space_clip_view_clip_with_region_poll(bContext * /*C*/)
{
  return false;
}
inline bool ED_space_clip_tracking_poll(bContext * /*C*/)
{
  return false;
}
inline bool ED_space_clip_maskedit_poll(bContext * /*C*/)
{
  return false;
}
inline bool ED_space_clip_maskedit_visible_splines_poll(bContext * /*C*/)
{
  return false;
}
inline bool ED_space_clip_maskedit_mask_poll(bContext * /*C*/)
{
  return false;
}
inline bool ED_space_clip_maskedit_mask_visible_splines_poll(bContext * /*C*/)
{
  return false;
}

inline void ED_space_clip_get_size(const SpaceClip * /*sc*/, int *r_width, int *r_height)
{
  if (r_width) {
    *r_width = 0;
  }
  if (r_height) {
    *r_height = 0;
  }
}
inline void ED_space_clip_get_size_fl(const SpaceClip * /*sc*/, float r_size[2])
{
  r_size[0] = 0.0f;
  r_size[1] = 0.0f;
}
inline void ED_space_clip_get_zoom(const SpaceClip * /*sc*/,
                                   const ARegion * /*region*/,
                                   float *r_zoomx,
                                   float *r_zoomy)
{
  if (r_zoomx) {
    *r_zoomx = 1.0f;
  }
  if (r_zoomy) {
    *r_zoomy = 1.0f;
  }
}
inline void ED_space_clip_get_aspect(const SpaceClip * /*sc*/, float *r_aspx, float *r_aspy)
{
  if (r_aspx) {
    *r_aspx = 1.0f;
  }
  if (r_aspy) {
    *r_aspy = 1.0f;
  }
}
inline void ED_space_clip_get_aspect_dimension_aware(const SpaceClip * /*sc*/,
                                                     float *r_aspx,
                                                     float *r_aspy)
{
  if (r_aspx) {
    *r_aspx = 1.0f;
  }
  if (r_aspy) {
    *r_aspy = 1.0f;
  }
}

inline int ED_space_clip_get_clip_frame_number(const SpaceClip * /*sc*/)
{
  return 0;
}

inline ImBuf *ED_space_clip_get_buffer(const SpaceClip * /*sc*/)
{
  return nullptr;
}
inline ImBuf *ED_space_clip_get_stable_buffer(const SpaceClip * /*sc*/,
                                              float /*loc*/[2],
                                              float * /*scale*/,
                                              float * /*angle*/)
{
  return nullptr;
}

inline bool ED_space_clip_get_position(const SpaceClip * /*sc*/,
                                       const ARegion * /*region*/,
                                       const int /*mval*/[2],
                                       float r_fpos[2])
{
  r_fpos[0] = 0.0f;
  r_fpos[1] = 0.0f;
  return false;
}
inline bool ED_space_clip_color_sample(const SpaceClip * /*sc*/,
                                       const ARegion * /*region*/,
                                       const int /*mval*/[2],
                                       float /*r_col*/[3])
{
  return false;
}

inline void ED_clip_update_frame(const Main * /*mainp*/, int /*cfra*/) {}
inline bool ED_clip_view_selection(const bContext * /*C*/,
                                   const ARegion * /*region*/,
                                   bool /*fit*/)
{
  return false;
}

inline void ED_clip_select_all(const SpaceClip * /*sc*/, int /*action*/, bool *r_has_selection)
{
  if (r_has_selection) {
    *r_has_selection = false;
  }
}
inline bool ED_clip_can_select(bContext * /*C*/)
{
  return false;
}

inline void ED_clip_point_undistorted_pos(const SpaceClip * /*sc*/,
                                          const float co[2],
                                          float r_co[2])
{
  r_co[0] = co[0];
  r_co[1] = co[1];
}
inline void ED_clip_point_stable_pos(
    const SpaceClip * /*sc*/, const ARegion * /*region*/, float x, float y, float *xr, float *yr)
{
  if (xr) {
    *xr = x;
  }
  if (yr) {
    *yr = y;
  }
}
inline void ED_clip_point_stable_pos__reverse(const SpaceClip * /*sc*/,
                                              const ARegion * /*region*/,
                                              const float co[2],
                                              float r_co[2])
{
  r_co[0] = co[0];
  r_co[1] = co[1];
}
inline void ED_clip_mouse_pos(const SpaceClip * /*sc*/,
                              const ARegion * /*region*/,
                              const int /*mval*/[2],
                              float r_co[2])
{
  r_co[0] = 0.0f;
  r_co[1] = 0.0f;
}

inline bool ED_space_clip_check_show_trackedit(const SpaceClip * /*sc*/)
{
  return false;
}
inline bool ED_space_clip_check_show_maskedit(const SpaceClip * /*sc*/)
{
  return false;
}

inline MovieClip *ED_space_clip_get_clip(const SpaceClip * /*sc*/)
{
  return nullptr;
}
inline void ED_space_clip_set_clip(bContext * /*C*/,
                                   bScreen * /*screen*/,
                                   SpaceClip * /*sc*/,
                                   MovieClip * /*clip*/)
{
}

inline Mask *ED_space_clip_get_mask(const SpaceClip * /*sc*/)
{
  return nullptr;
}
inline void ED_space_clip_set_mask(bContext * /*C*/, SpaceClip * /*sc*/, Mask * /*mask*/) {}

struct ClipViewLockState {
  float offset_x, offset_y;
  float lock_offset_x, lock_offset_y;
  float zoom;
};

inline void ED_clip_view_lock_state_store(const bContext * /*C*/, ClipViewLockState * /*state*/) {}
inline void ED_clip_view_lock_state_restore_no_jump(const bContext * /*C*/,
                                                    const ClipViewLockState * /*state*/)
{
}

inline void ED_operatormacros_clip() {}

}  // namespace blender
