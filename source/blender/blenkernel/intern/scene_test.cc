/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_collection.hh"
#include "BKE_mesh.h"
#include "BKE_object.hh"
#include "BKE_gtest_base.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"

#include "DNA_defs.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "testing/testing.h"

namespace blender::bke::tests {

class SceneTest : public ::testing::Test {
 public:
  Main *bmain;

  static void SetUpTestSuite()
  {
    bke::gtest_setup();
    /* #BKE_scene_duplicate() uses #U::dupflag. */
    U = dna::shallow_copy(UserDef());
  }

  static void TearDownTestSuite()
  {
    bke::gtest_teardown();
  }

  void SetUp() override
  {
    bmain = BKE_main_new();
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }
};

TEST_F(SceneTest, frame_snap_by_seconds)
{
  Scene fake_scene = {};

  /* Regular 24 FPS snapping. */
  fake_scene.r.frs_sec = 24;
  fake_scene.r.frs_sec_base = 1.0;
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 47));
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 49));
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 59));
  EXPECT_FLOAT_EQ(72.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 60));
  EXPECT_FLOAT_EQ(9984.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 2.0, 10000.0));

  /* 12 FPS snapping by incrementing the base. */
  fake_scene.r.frs_sec = 24;
  fake_scene.r.frs_sec_base = 2.0;
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 47));
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 49));
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 53));
  EXPECT_FLOAT_EQ(60.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 54));
  EXPECT_FLOAT_EQ(9996.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 10000.0));

  /* 0.1 FPS snapping to 2-second intervals. */
  fake_scene.r.frs_sec = 1;
  fake_scene.r.frs_sec_base = 10.0;
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 2.0, 48.0));
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 2.0, 48.1));
  EXPECT_FLOAT_EQ(48.2, BKE_scene_frame_snap_by_seconds(&fake_scene, 2.0, 48.2));
  EXPECT_FLOAT_EQ(10000.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 2.0, 10000.0));
}

TEST_F(SceneTest, full_copy_mesh_object_id_remapping)
{
  Scene *scene_src = BKE_scene_add(bmain, "Scene_source");
  Mesh *mesh_src = BKE_mesh_add(bmain, "Scene_source_mesh");
  Object *ob_src = BKE_object_add_only_object(bmain, OB_MESH, "Scene_source_ob");
  ob_src->data = id_cast<ID *>(mesh_src);
  BKE_collection_object_add(bmain, scene_src->master_collection, ob_src);

  constexpr StringRef idp_scene2scene_name = "scene2scene";
  constexpr StringRef idp_scene2mesh_name = "scene2mesh";
  constexpr StringRef idp_mesh2scene_name = "mesh2scene";
  constexpr StringRef idp_mesh2mesh_name = "mesh2mesh";

  IDProperty *scene_idgroup_src = IDP_EnsureProperties(&scene_src->id);
  IDP_AddToGroup(scene_idgroup_src,
                 bke::idprop::create(idp_scene2scene_name, &scene_src->id).release());
  IDP_AddToGroup(scene_idgroup_src,
                 bke::idprop::create(idp_scene2mesh_name, &mesh_src->id).release());

  IDProperty *mesh_idgroup_src = IDP_EnsureProperties(&mesh_src->id);
  IDP_AddToGroup(mesh_idgroup_src,
                 bke::idprop::create(idp_mesh2scene_name, &scene_src->id).release());
  IDP_AddToGroup(mesh_idgroup_src,
                 bke::idprop::create(idp_mesh2mesh_name, &mesh_src->id).release());

  Scene *scene_copy = BKE_scene_duplicate(
      bmain,
      scene_src,
      SCE_COPY_FULL,
      static_cast<eDupli_ID_Flags>(U.dupflag | USER_DUP_OBJECT | USER_DUP_MESH),
      LIB_ID_DUPLICATE_IS_ROOT_ID);

  ASSERT_NE(scene_copy, nullptr);
  EXPECT_NE(scene_copy, scene_src);

  IDProperty *scene_idgroup_copy = IDP_GetProperties(&scene_copy->id);
  ASSERT_NE(scene_idgroup_copy, nullptr);
  EXPECT_EQ(IDP_ID_get(IDP_GetPropertyFromGroup(scene_idgroup_copy, idp_scene2scene_name)),
            &scene_copy->id);
  ID *mesh_copy_id = IDP_ID_get(IDP_GetPropertyFromGroup(scene_idgroup_copy, idp_scene2mesh_name));
  ASSERT_NE(mesh_copy_id, nullptr);
  EXPECT_NE(mesh_copy_id, &mesh_src->id);

  IDProperty *mesh_idgroup_copy = IDP_GetProperties(mesh_copy_id);
  ASSERT_NE(mesh_idgroup_copy, nullptr);
  EXPECT_EQ(IDP_ID_get(IDP_GetPropertyFromGroup(mesh_idgroup_copy, idp_mesh2scene_name)),
            &scene_copy->id);
  EXPECT_EQ(IDP_ID_get(IDP_GetPropertyFromGroup(mesh_idgroup_copy, idp_mesh2mesh_name)),
            mesh_copy_id);
}

}  // namespace blender::bke::tests
