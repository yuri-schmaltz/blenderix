/* SPDX-FileCopyrightText: 2015 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#include "BLI_utildefines.hh"

#include "DNA_scene_types.h"

#include "GHOST_IWindow.hh"
#include "GHOST_Types.hh"

#include "ED_screen.hh"

#include "GPU_immediate.hh"
#include "GPU_viewport.hh"

#include "WM_api.hh"
#include "WM_types.hh"
#include "wm.hh"

#include "UI_resources.hh"

namespace blender {

void wm_stereo3d_draw_sidebyside(wmWindow *win, int view)
{
  bool cross_eyed = (win->stereo3d_format->flag & S3D_SIDEBYSIDE_CROSSEYED) != 0;

  GPUVertFormat *format = immVertexFormat();
  uint texcoord = GPU_vertformat_attr_add(format, "texCoord", gpu::VertAttrType::SFLOAT_32_32);
  uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_IMAGE);

  const int2 win_size = WM_window_native_pixel_size(win);

  int soffx = win_size[0] / 2;
  if (view == STEREO_LEFT_ID) {
    if (!cross_eyed) {
      soffx = 0;
    }
  }
  else { /* #RIGHT_LEFT_ID. */
    if (cross_eyed) {
      soffx = 0;
    }
  }

  /* `wmOrtho` for the screen has this same offset. */
  const float halfx = GLA_PIXEL_OFS / win_size[0];
  const float halfy = GLA_PIXEL_OFS / win_size[1];

  /* Texture is already bound to GL_TEXTURE0 unit. */

  immBegin(GPU_PRIM_TRI_FAN, 4);

  immAttr2f(texcoord, halfx, halfy);
  immVertex2f(pos, soffx, 0.0f);

  immAttr2f(texcoord, 1.0f + halfx, halfy);
  immVertex2f(pos, soffx + (win_size[0] * 0.5f), 0.0f);

  immAttr2f(texcoord, 1.0f + halfx, 1.0f + halfy);
  immVertex2f(pos, soffx + (win_size[0] * 0.5f), win_size[1]);

  immAttr2f(texcoord, halfx, 1.0f + halfy);
  immVertex2f(pos, soffx, win_size[1]);

  immEnd();

  immUnbindProgram();
}

void wm_stereo3d_draw_topbottom(wmWindow *win, int view)
{
  GPUVertFormat *format = immVertexFormat();
  uint texcoord = GPU_vertformat_attr_add(format, "texCoord", gpu::VertAttrType::SFLOAT_32_32);
  uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_IMAGE);

  const int2 win_size = WM_window_native_pixel_size(win);

  int soffy;
  if (view == STEREO_LEFT_ID) {
    soffy = win_size[1] * 0.5f;
  }
  else { /* #STEREO_RIGHT_ID. */
    soffy = 0;
  }

  /* `wmOrtho` for the screen has this same offset. */
  const float halfx = GLA_PIXEL_OFS / win_size[0];
  const float halfy = GLA_PIXEL_OFS / win_size[1];

  /* Texture is already bound to GL_TEXTURE0 unit. */

  immBegin(GPU_PRIM_TRI_FAN, 4);

  immAttr2f(texcoord, halfx, halfy);
  immVertex2f(pos, 0.0f, soffy);

  immAttr2f(texcoord, 1.0f + halfx, halfy);
  immVertex2f(pos, win_size[0], soffy);

  immAttr2f(texcoord, 1.0f + halfx, 1.0f + halfy);
  immVertex2f(pos, win_size[0], soffy + (win_size[1] * 0.5f));

  immAttr2f(texcoord, halfx, 1.0f + halfy);
  immVertex2f(pos, 0.0f, soffy + (win_size[1] * 0.5f));

  immEnd();

  immUnbindProgram();
}

static bool wm_stereo3d_is_fullscreen_required(eStereoDisplayMode stereo_display)
{
  return ELEM(stereo_display, S3D_DISPLAY_SIDEBYSIDE, S3D_DISPLAY_TOPBOTTOM);
}

bool WM_stereo3d_enabled(wmWindow *win, bool skip_stereo3d_check)
{
  const bScreen *screen = WM_window_get_active_screen(win);
  const Scene *scene = WM_window_get_active_scene(win);
  const GHOST_IWindow *ghost_window = static_cast<GHOST_IWindow *>(win->runtime->ghostwin);

  /* Some 3d methods change the window arrangement, thus they shouldn't
   * toggle on/off just because there is no 3d elements being drawn. */
  if (wm_stereo3d_is_fullscreen_required(eStereoDisplayMode(win->stereo3d_format->display_mode))) {
    return ghost_window->getState() == GHOST_kWindowStateFullScreen;
  }

  if ((skip_stereo3d_check == false) && (ED_screen_stereo3d_required(screen, scene) == false)) {
    return false;
  }

  /* Some 3d methods change the window arrangement, thus they shouldn't
   * toggle on/off just because there is no 3d elements being drawn. */
  if (wm_stereo3d_is_fullscreen_required(eStereoDisplayMode(win->stereo3d_format->display_mode))) {
    return ghost_window->getState() == GHOST_kWindowStateFullScreen;
  }

  return true;
}

void wm_stereo3d_mouse_offset_apply(wmWindow *win, int r_mouse_xy[2])
{
  if (!WM_stereo3d_enabled(win, false)) {
    return;
  }

  if (win->stereo3d_format->display_mode == S3D_DISPLAY_SIDEBYSIDE) {
    const int half_x = WM_window_native_pixel_x(win) / 2;
    /* Right half of the screen. */
    if (r_mouse_xy[0] > half_x) {
      r_mouse_xy[0] -= half_x;
    }
    r_mouse_xy[0] *= 2;
  }
  else if (win->stereo3d_format->display_mode == S3D_DISPLAY_TOPBOTTOM) {
    const int half_y = WM_window_native_pixel_y(win) / 2;
    /* Upper half of the screen. */
    if (r_mouse_xy[1] > half_y) {
      r_mouse_xy[1] -= half_y;
    }
    r_mouse_xy[1] *= 2;
  }
}

}  // namespace blender
