#include "BKE_mesh.h"
#include "BLI_listbase.hh"
/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "blendfile_loading_base_test.h"

#include "BKE_appdir.hh"
#include "BKE_collection.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_scene.hh"

#include "BLI_fileops.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"

#include "BLO_readfile.hh"
#include "BLO_writefile.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

namespace blender {

class BlendfileLoadingTest : public BlendfileLoadingBaseTest {};

TEST_F(BlendfileLoadingTest, CanaryTest)
{
  /* Load the smallest blend file we have in the tests/files directory. */
  if (!blendfile_load("modifier_stack" SEP_STR "array_test.blend")) {
    return;
  }
  depsgraph_create(DAG_EVAL_RENDER);
  EXPECT_NE(nullptr, this->depsgraph);
}

TEST_F(BlendfileLoadingTest, RoundTripMeshBlendAndDepsgraph)
{
  BKE_tempdir_init(nullptr);
  char filepath[FILE_MAX];
  BLI_path_join(filepath, BKE_tempdir_base(), "blenderix_gtest_roundtrip.blend");

  {
    Main *write_main = BKE_main_new();
    STRNCPY(write_main->filepath, filepath);
    Scene *scene = BKE_scene_add(write_main, "RoundTripScene");
    Mesh *mesh = BKE_mesh_add(write_main, "RoundTripQuad");
    Mesh *nomain_mesh = BKE_mesh_new_nomain(4, 4, 1, 4);
    nomain_mesh->vert_positions_for_write().copy_from(
        {float3(-1, -1, 0), float3(1, -1, 0), float3(1, 1, 0), float3(-1, 1, 0)});
    nomain_mesh->edges_for_write().copy_from({int2(0, 1), int2(1, 2), int2(2, 3), int2(3, 0)});
    nomain_mesh->face_offsets_for_write().copy_from({0, 4});
    nomain_mesh->corner_verts_for_write().copy_from({0, 1, 2, 3});
    nomain_mesh->corner_edges_for_write().copy_from({0, 1, 2, 3});
    BKE_mesh_nomain_to_mesh(nomain_mesh, mesh, nullptr);

    Object *ob = BKE_object_add_only_object(write_main, OB_MESH, "RoundTripOb");
    ob->data = id_cast<ID *>(mesh);
    BKE_collection_object_add(write_main, scene->master_collection, ob);

    BlendFileWriteParams write_params{};
    ASSERT_TRUE(BLO_write_file(write_main, filepath, 0, &write_params, nullptr));
    BKE_main_free(write_main);
  }

  BlendFileReadReport read_reports{};
  this->bfile = BLO_read_from_file(filepath, BLO_READ_SKIP_NONE, &read_reports);
  ASSERT_NE(this->bfile, nullptr);
  ASSERT_NE(this->bfile->curscene, nullptr);
  for (ViewLayer &view_layer : this->bfile->curscene->view_layers) {
    BKE_view_layer_synced_ensure(*this->bfile->main, this->bfile->curscene, &view_layer);
  }
  if (this->bfile->cur_view_layer == nullptr) {
    this->bfile->cur_view_layer = this->bfile->curscene->view_layers.first();
  }
  depsgraph_create(DAG_EVAL_VIEWPORT);
  ASSERT_NE(this->depsgraph, nullptr);

  Mesh *loaded_mesh = this->bfile->main->meshes.first();
  ASSERT_NE(loaded_mesh, nullptr);
  EXPECT_EQ(loaded_mesh->verts_num, 4);
  EXPECT_EQ(loaded_mesh->faces_num, 1);

  BLI_delete(filepath, false, false);
}

}  // namespace blender
