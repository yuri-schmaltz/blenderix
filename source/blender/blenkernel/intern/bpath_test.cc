/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "testing/testing.h"

#include "BKE_bpath.hh"
#include "BKE_gtest_base.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"

#include "MEM_guardedalloc.h"

#include "DNA_image_types.h"
#include "DNA_listBase.h"

#include "BLI_listbase.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"

namespace blender {

struct PathStore;

namespace bke::tests {

#ifdef WIN32
#  define ABSOLUTE_ROOT "C:" SEP_STR
#else
#  define ABSOLUTE_ROOT SEP_STR
#endif

#define RELATIVE_ROOT "//"
#define BASE_DIR ABSOLUTE_ROOT "blendfiles" SEP_STR
#define REBASE_DIR BASE_DIR "rebase" SEP_STR

#define BLENDFILE_NAME "bpath.blend"
#define BLENDFILE_PATH BASE_DIR BLENDFILE_NAME

#define IMAGE_A_PATH_ITEM "textures" SEP_STR "albedo.png"
#define IMAGE_A_PATH_ABSOLUTE ABSOLUTE_ROOT IMAGE_A_PATH_ITEM
#define IMAGE_A_PATH_RELATIVE RELATIVE_ROOT IMAGE_A_PATH_ITEM
#define IMAGE_A_PATH_RELATIVE_MADE_ABSOLUTE BASE_DIR IMAGE_A_PATH_ITEM

#define IMAGE_B_PATH_ITEM "textures" SEP_STR "normal.png"
#define IMAGE_B_PATH_ABSOLUTE ABSOLUTE_ROOT IMAGE_B_PATH_ITEM
#define IMAGE_B_PATH_ABSOLUTE_MADE_RELATIVE RELATIVE_ROOT ".." SEP_STR IMAGE_B_PATH_ITEM
#define IMAGE_B_PATH_RELATIVE RELATIVE_ROOT IMAGE_B_PATH_ITEM
#define IMAGE_B_PATH_RELATIVE_MADE_ABSOLUTE BASE_DIR IMAGE_B_PATH_ITEM

class BPathTest : public BlenderGTestBase {
 public:
  void SetUp() override
  {
    bmain = BKE_main_new();
    STRNCPY(bmain->filepath, BLENDFILE_PATH);

    image_a = BKE_id_new<Image>(bmain, "ImageA");
    image_a->source = IMA_SRC_FILE;
    image_b = BKE_id_new<Image>(bmain, "ImageB");
    image_b->source = IMA_SRC_FILE;
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }

  Main *bmain = nullptr;
  Image *image_a = nullptr;
  Image *image_b = nullptr;
};

TEST_F(BPathTest, rebase_on_relative)
{
  STRNCPY(image_a->filepath, IMAGE_A_PATH_RELATIVE);
  STRNCPY(image_b->filepath, IMAGE_B_PATH_RELATIVE);

  BKE_bpath_relative_rebase(bmain, BASE_DIR, REBASE_DIR, nullptr);

  EXPECT_STREQ(image_a->filepath, RELATIVE_ROOT ".." SEP_STR IMAGE_A_PATH_ITEM);
  EXPECT_STREQ(image_b->filepath, RELATIVE_ROOT ".." SEP_STR IMAGE_B_PATH_ITEM);
}

TEST_F(BPathTest, rebase_on_absolute)
{
  STRNCPY(image_a->filepath, IMAGE_A_PATH_ABSOLUTE);
  STRNCPY(image_b->filepath, IMAGE_B_PATH_ABSOLUTE);

  BKE_bpath_relative_rebase(bmain, BASE_DIR, REBASE_DIR, nullptr);

  EXPECT_STREQ(image_a->filepath, IMAGE_A_PATH_ABSOLUTE);
  EXPECT_STREQ(image_b->filepath, IMAGE_B_PATH_ABSOLUTE);
}

TEST_F(BPathTest, convert_to_relative)
{
  STRNCPY(image_a->filepath, IMAGE_A_PATH_RELATIVE);
  STRNCPY(image_b->filepath, IMAGE_B_PATH_ABSOLUTE);

  BKE_bpath_relative_convert(bmain, BASE_DIR, nullptr);

  EXPECT_STREQ(image_a->filepath, IMAGE_A_PATH_RELATIVE);
  EXPECT_STREQ(image_b->filepath, IMAGE_B_PATH_ABSOLUTE_MADE_RELATIVE);
}

TEST_F(BPathTest, convert_to_absolute)
{
  STRNCPY(image_a->filepath, IMAGE_A_PATH_RELATIVE);
  STRNCPY(image_b->filepath, IMAGE_B_PATH_ABSOLUTE);

  BKE_bpath_absolute_convert(bmain, BASE_DIR, nullptr);

  EXPECT_STREQ(image_a->filepath, IMAGE_A_PATH_RELATIVE_MADE_ABSOLUTE);
  EXPECT_STREQ(image_b->filepath, IMAGE_B_PATH_ABSOLUTE);
}

TEST_F(BPathTest, list_backup_restore)
{
  STRNCPY(image_a->filepath, IMAGE_A_PATH_RELATIVE);
  STRNCPY(image_b->filepath, IMAGE_B_PATH_ABSOLUTE);

  void *path_list_handle = BKE_bpath_list_backup(bmain, static_cast<eBPathForeachFlag>(0));

  ListBaseT<PathStore> *path_list = static_cast<ListBaseT<PathStore> *>(path_list_handle);
  EXPECT_EQ(path_list->count(), 2);

  STRNCPY(image_a->filepath, IMAGE_A_PATH_ABSOLUTE);
  STRNCPY(image_b->filepath, IMAGE_B_PATH_RELATIVE);

  BKE_bpath_list_restore(bmain, static_cast<eBPathForeachFlag>(0), path_list_handle);

  EXPECT_STREQ(image_a->filepath, IMAGE_A_PATH_RELATIVE);
  EXPECT_STREQ(image_b->filepath, IMAGE_B_PATH_ABSOLUTE);
  EXPECT_EQ(path_list->count(), 0);

  BKE_bpath_list_free(path_list_handle);
}

}  // namespace bke::tests
}  // namespace blender
