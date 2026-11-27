/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "workbench_private.hh"

#include "BLI_ghash.hh"
#include "BLI_hash_c.hh"
#include "BLI_math_color_c.hh"

#include "IMB_colormanagement.hh"

/* get_image */
#include "BKE_image_gpu.hh"
#include "BKE_node_legacy_types.hh"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
/* get_image */

namespace blender::workbench {

Material::Material(blender::Object &ob, bool random)
{
  if (random) {
    uint hash = BLI_ghashutil_strhash_p_murmur(ob.id.name);
    if (ob.id.lib) {
      hash = (hash * 13) ^ BLI_ghashutil_strhash_p_murmur(ob.id.lib->filepath);
    }
    float3 hsv = float3(BLI_hash_int_01(hash), 0.5f, 0.8f);
    hsv_to_rgb_v(hsv, base_color);
    IMB_colormanagement_rec709_to_scene_linear(base_color, base_color);
  }
  else {
    base_color = ob.color;
  }
  packed_data = Material::pack_data(0.0f, 0.4f, ob.color[3]);
}

MaterialTexture::MaterialTexture(Manager & /*manager*/, Object * /*ob*/, int /*material_index*/)
{
}

MaterialTexture::MaterialTexture(Manager &manager,
                                 blender::Image *image,
                                 ImageUser *user /* = nullptr */)
{
  gpu = BKE_image_acquire_gpu_material_texture(image, user, true, false);
  manager.hold_texture(gpu.texture);
  manager.hold_texture(gpu.tile_mapping);
  premultiplied = image->alpha_mode == IMA_ALPHA_PREMUL;
  alpha_cutoff = !ELEM(image->alpha_mode, IMA_ALPHA_IGNORE, IMA_ALPHA_CHANNEL_PACKED);
  name = image->id.name;
}

}  // namespace blender::workbench
