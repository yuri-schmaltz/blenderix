/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstring>
#include <string>

#include "BLI_set.hh"
#include "BLI_utildefines.hh"

#include "DNA_ID.h"
#include "DNA_image_types.h"

#include "BKE_context.hh"
#include "BKE_image.hh"
#include "BKE_image_save.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "ED_image.hh"

namespace blender {

static bool image_should_be_saved_when_modified(Image *ima)
{
  return !ELEM(ima->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE);
}

static bool image_should_be_saved(Image *ima, bool *r_is_format_writable)
{
  if (BKE_image_is_dirty_writable(ima, r_is_format_writable) &&
      ELEM(ima->source, IMA_SRC_FILE, IMA_SRC_GENERATED, IMA_SRC_TILED))
  {
    return image_should_be_saved_when_modified(ima);
  }
  return false;
}

static bool image_has_valid_path(Image *ima)
{
  return strchr(ima->filepath, '\\') || strchr(ima->filepath, '/');
}

static bool image_should_pack_during_save_all(const Image *ima)
{
  /* Images without a filepath (implied with IMA_SRC_GENERATED) should
   * be packed during a save_all operation. */
  return (ima->source == IMA_SRC_GENERATED) ||
         (ima->source == IMA_SRC_TILED && !BKE_image_has_filepath(ima));
}

bool ED_image_should_save_modified(const Main *bmain)
{
  ReportList reports;
  BKE_reports_init(&reports, RPT_STORE);

  uint modified_images_count = ED_image_save_all_modified_info(bmain, &reports);
  bool should_save = modified_images_count || !reports.list.is_empty();

  BKE_reports_free(&reports);

  return should_save;
}

int ED_image_save_all_modified_info(const Main *bmain, ReportList *reports)
{
  Set<std::string> unique_paths;

  int num_saveable_images = 0;

  for (Image *ima = bmain->images.first(); ima; ima = static_cast<Image *>(ima->id.next)) {
    bool is_format_writable;

    if (image_should_be_saved(ima, &is_format_writable)) {
      if (BKE_image_has_packedfile(ima) || image_should_pack_during_save_all(ima)) {
        if (ID_IS_EDITABLE(ima)) {
          num_saveable_images++;
        }
        else {
          BKE_reportf(reports,
                      RPT_WARNING,
                      "Packed library image cannot be saved: \"%s\" from \"%s\"",
                      ima->id.name + 2,
                      ima->id.lib->filepath);
        }
      }
      else if (!is_format_writable) {
        BKE_reportf(reports,
                    RPT_WARNING,
                    "Image cannot be saved, use a different file format: \"%s\"",
                    ima->id.name + 2);
      }
      else {
        if (image_has_valid_path(ima)) {
          num_saveable_images++;
          if (unique_paths.contains_as(ima->filepath)) {
            BKE_reportf(reports,
                        RPT_WARNING,
                        "Multiple images cannot be saved to an identical path: \"%s\"",
                        ima->filepath);
          }
          else {
            unique_paths.add(ima->filepath);
          }
        }
        else {
          BKE_reportf(reports,
                      RPT_WARNING,
                      "Image cannot be saved, no valid file path: \"%s\"",
                      ima->filepath);
        }
      }
    }
  }

  return num_saveable_images;
}

bool ED_image_save_all_modified(const bContext *C, ReportList *reports)
{
  Main *bmain = CTX_data_main(C);

  ED_image_save_all_modified_info(bmain, reports);

  bool ok = true;

  for (Image *ima = bmain->images.first(); ima; ima = static_cast<Image *>(ima->id.next)) {
    bool is_format_writable;

    if (image_should_be_saved(ima, &is_format_writable)) {
      if (BKE_image_has_packedfile(ima) || image_should_pack_during_save_all(ima)) {
        BKE_image_memorypack(ima);
      }
      else if (is_format_writable) {
        if (image_has_valid_path(ima)) {
          ImageSaveOptions opts;
          Scene *scene = CTX_data_scene(C);
          if (BKE_image_save_options_init(&opts, bmain, scene, ima, nullptr, false, false)) {
            bool saved_successfully = BKE_image_save(reports, bmain, ima, nullptr, &opts);
            ok = ok && saved_successfully;
          }
          BKE_image_save_options_free(&opts);
        }
      }
    }
  }
  return ok;
}

void ED_image_internal_autosave_flush(const Main *bmain)
{
  for (Image *ima = bmain->images.first(); ima; ima = static_cast<Image *>(ima->id.next)) {
    bool is_format_writable;

    if (image_should_be_saved(ima, &is_format_writable)) {
      if (BKE_image_has_packedfile(ima) || image_should_pack_during_save_all(ima) ||
          (is_format_writable && image_has_valid_path(ima)))
      {
        BKE_image_autosave_memorypack(ima);
      }
    }
  }
}

}  // namespace blender
