/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#include <algorithm>

#include "DNA_userdef_types.h"

#include "BLI_utildefines.hh"

#include "BKE_blender_branding.hh"
#include "BKE_blender_project.hh"

#include "BLT_translation.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm.hh"

namespace blender {

#ifndef WITH_HEADLESS
/* Composite the Blender logo over a project splash image. */
static void wm_project_splash_logo_add(ImBuf *ibuf)
{
  /* Magic numbers match logo positioning and size in default splash image. */
  ImBuf *logo = ui::svg_icon_bitmap(ICON_BLENDER_LOGO_LARGE, ibuf->x * 0.127f, false);
  if (logo == nullptr) {
    return;
  }

  /* The monochrome logo has alpha in all channels, unpremultiply to get white with alpha. */
  IMB_unpremultiply_alpha(logo);

  /* Rows are stored bottom to top. */
  const int x = ibuf->x * 0.025f;
  const int y = ibuf->y - int(ibuf->x * 0.02f) - logo->y;
  IMB_rectblend(ibuf,
                ibuf->byte_data_for_write(),
                nullptr,
                ibuf,
                logo,
                nullptr,
                nullptr,
                nullptr,
                0.0f,
                x,
                y,
                x,
                y,
                0,
                0,
                logo->x,
                logo->y,
                IMB_BLEND_MIX,
                false);

  IMB_freeImBuf(logo);
}
#endif /* !WITH_HEADLESS */

std::string WM_project_splash_image_filepath(const Main *bmain)
{
  return BKE_blender_project_read_callback(
      bmain, [](const bke::BlenderProject *project) -> std::string {
        if (project == nullptr || project->get_splash_image_path().is_empty()) {
          return "";
        }
        return BKE_blender_project_path_resolve(project->get_splash_image_path(), *project);
      });
}

ImBuf *WM_project_splash_image_load(const StringRefNull filepath,
                                    const int width,
                                    const bool add_logo)
{
  if (filepath.is_empty() || width <= 0) {
    return nullptr;
  }

  ImBuf *ibuf = IMB_load_image_from_filepath(filepath.c_str(), ImBufFlags::ByteData);
  if (ibuf == nullptr) {
    return nullptr;
  }

  /* Convert float to byte image buffer. */
  if (ibuf->byte_data() == nullptr) {
    IMB_byte_from_float(ibuf);
    IMB_free_float_pixels(ibuf);
  }
  if (ibuf->byte_data() == nullptr || ibuf->x <= 0 || ibuf->y <= 0) {
    IMB_freeImBuf(ibuf);
    return nullptr;
  }

  /* Add an alpha channel if missing. */
  ibuf->color_mode = ImColorMode::RGBA;
  ibuf->channels = 4;

  /* Crop to 2:1 around the center. */
  const int2 size(ibuf->x, ibuf->y);
  const int2 crop_size(std::min(size.x, size.y * 2), std::max(std::min(size.y, size.x / 2), 1));
  if (crop_size != size) {
    IMB_crop(ibuf, (size - crop_size) / 2, crop_size);
  }

  /* Scale to fit specified width. */
  const int height = std::max(width / 2, 1);
  if (width != ibuf->x || height != ibuf->y) {
    IMB_scale(ibuf, width, height, IMBScaleFilter::Box, false);
  }

#ifndef WITH_HEADLESS
  if (add_logo) {
    wm_project_splash_logo_add(ibuf);
  }
#else
  UNUSED_VARS(add_logo);
#endif

  return ibuf;
}

/* -------------------------------------------------------------------- */
/** \name About Dialog
 * \{ */

static ui::Block *wm_block_about_create(bContext *C, ARegion *region, void * /*arg*/)
{
  const uiStyle *style = ui::style_get_dpi();
  const int dialog_width = style->widget.points * 42 * UI_SCALE_FAC;

  ui::Block *block = block_begin(C, region, "about", ui::EmbossType::Emboss);

  block_flag_enable(block, ui::BLOCK_KEEP_OPEN | ui::BLOCK_LOOP | ui::BLOCK_NO_WIN_CLIP);
  block_theme_style_set(block, ui::BLOCK_THEME_STYLE_POPUP);

  ui::Layout &layout = ui::block_layout(block,
                                        ui::LayoutDirection::Vertical,
                                        ui::LayoutType::Panel,
                                        0,
                                        0,
                                        dialog_width,
                                        0,
                                        0,
                                        style);

#ifndef WITH_HEADLESS
  if (ImBuf *icon = ui::svg_icon_bitmap(ICON_BLENDERIX, dialog_width * 0.14f, true)) {
    layout.row(false).separator(0.2f);
    layout.row(false).alignment_set(ui::LayoutAlign::Left);
    uiDefButImage(block, icon, 0, U.widget_unit, icon->x, icon->y, nullptr);
    layout.row(false).separator(1.2f);
  }
#endif /* !WITH_HEADLESS */

  ui::Layout &col = layout.column(true);

  uiItemL_ex(&col, BLENDERIX_NAME " " BLENDERIX_VERSION, ICON_NONE, true, false);

  MenuType *mt = WM_menutype_find("WM_MT_splash_about", true);
  if (mt) {
    ui::menutype_draw(C, mt, &col);
  }

  block_bounds_set_centered(block, 22 * UI_SCALE_FAC);

  return block;
}

static wmOperatorStatus wm_splash_about_invoke(bContext *C,
                                               wmOperator * /*op*/,
                                               const wmEvent * /*event*/)
{
  ui::popup_block_invoke(C, wm_block_about_create, nullptr, nullptr);

  return OPERATOR_FINISHED;
}

void WM_OT_splash_about(wmOperatorType *ot)
{
  ot->name = "About " BLENDERIX_NAME;
  ot->idname = "WM_OT_splash_about";
  ot->description = "Open a window with information about " BLENDERIX_NAME;

  ot->invoke = wm_splash_about_invoke;
  ot->poll = WM_operator_winactive;
}

/** \} */

}  // namespace blender
