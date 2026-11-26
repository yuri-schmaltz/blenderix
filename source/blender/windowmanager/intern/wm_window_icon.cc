/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Window icon generation for X11 and Wayland.
 * Rasterizes the full-color application PNG for a window icon.
 */

#include <algorithm>
#include <cstring>

#include "GHOST_Types.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "wm_window_icon.hh"

using namespace blender;

extern "C" const char datatoc_icon_png[];
extern "C" const int datatoc_icon_png_size;

static void wm_ghost_icon_generate(const GHOST_IconGenerator * /*icon_generator*/,
                                   GHOST_IWindow * /*window*/,
                                   uint8_t *pixels,
                                   int icon_size)
{
  const size_t buffer_size = size_t(icon_size) * icon_size * 4;
  memset(pixels, 0, buffer_size);

  ImBuf *image = IMB_load_image_from_memory(
      reinterpret_cast<const unsigned char *>(datatoc_icon_png),
      datatoc_icon_png_size,
      ImBufFlags::ByteData,
      "application icon");
  if (image == nullptr || image->byte_buffer.data == nullptr) [[unlikely]] {
    if (image != nullptr) {
      IMB_freeImBuf(image);
    }
    return;
  }

  const float scale = float(icon_size) / std::max(image->x, image->y);
  const int width = std::max(1, int(image->x * scale));
  const int height = std::max(1, int(image->y * scale));
  if (!IMB_scale(image, width, height, IMBScaleFilter::Box, false)) [[unlikely]] {
    IMB_freeImBuf(image);
    memset(pixels, 0, buffer_size);
    return;
  }

  const int offset_x = (icon_size - width) / 2;
  const int offset_y = (icon_size - height) / 2;
  for (int y = 0; y < height; y++) {
    const unsigned char *source = image->byte_buffer.data + size_t(y) * width * 4;
    unsigned char *destination =
        pixels + (size_t(y + offset_y) * icon_size + offset_x) * 4;
    memcpy(destination, source, size_t(width) * 4);
  }

  IMB_freeImBuf(image);
}

const GHOST_IconGenerator wm_ghost_icon_generator = {
    /*generate_fn*/ wm_ghost_icon_generate,
    /*user_data*/ nullptr,
};
