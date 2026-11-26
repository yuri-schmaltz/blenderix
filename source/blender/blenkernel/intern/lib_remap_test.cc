/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "testing/testing.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_gtest_base.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_remap.hh"
#include "BKE_main.hh"
#include "BKE_collection.hh"
#include "BKE_scene.hh"
#include "BKE_mesh.h"
#include "BKE_object.hh"


namespace blender {

using namespace blender::bke::id;

namespace bke::tests {

namespace {
class TestData {
 public:
  Main *bmain = nullptr;
  bContext *C = nullptr;

  TestData()
  {
    if (this->bmain == nullptr) {
      this->bmain = BKE_main_new();
      G.main = this->bmain;
    }

    if (this->C == nullptr) {
      this->C = CTX_create();
      CTX_data_main_set(this->C, this->bmain);
    }
  }

  ~TestData()
  {
    if (this->bmain != nullptr) {
      BKE_main_free(this->bmain);
      this->bmain = nullptr;
      G.main = nullptr;
    }

    if (this->C != nullptr) {
      CTX_free(C);
      this->C = nullptr;
    }
  }
};

class LibRemapTest : public BlenderGTestBase {};

class EmbeddedCollectionTestData : public TestData {
 public:
  Scene *scene = nullptr;
  Collection *master_collection = nullptr;
  EmbeddedCollectionTestData()
  {
    this->scene = BKE_scene_add(this->bmain, "Scene");
    this->master_collection = this->scene->master_collection;
  }
};

class MeshTestData : public TestData {
 public:
  Mesh *mesh = nullptr;
  MeshTestData()
  {
    this->mesh = BKE_mesh_add(this->bmain, nullptr);
  }
};

class TwoMeshesTestData : public MeshTestData {
 public:
  Mesh *other_mesh = nullptr;

  TwoMeshesTestData()
  {
    this->other_mesh = BKE_mesh_add(this->bmain, nullptr);
  }
};

class MeshObjectTestData : public MeshTestData {
 public:
  Object *object;
  MeshObjectTestData()
  {
    this->object = BKE_object_add_only_object(this->bmain, OB_MESH, nullptr);
    this->object->data = id_cast<ID *>(this->mesh);
  }
};
}  // namespace

/* -------------------------------------------------------------------- */
/** \name Embedded IDs
 * \{ */

TEST_F(LibRemapTest, embedded_ids_can_not_be_remapped)
{
  EmbeddedCollectionTestData context;
  Collection *other_collection = BKE_id_new_nomain<Collection>(nullptr);

  ASSERT_NE(context.scene, nullptr);
  ASSERT_EQ(context.master_collection, context.scene->master_collection);

  BKE_libblock_remap(context.bmain, context.master_collection, other_collection, 0);

  EXPECT_EQ(context.master_collection, context.scene->master_collection);
  EXPECT_NE(context.scene->master_collection, other_collection);

  BKE_id_free(nullptr, other_collection);
}

TEST_F(LibRemapTest, embedded_ids_can_not_be_deleted)
{
  EmbeddedCollectionTestData context;

  ASSERT_NE(context.master_collection, nullptr);
  ASSERT_EQ(context.master_collection, context.scene->master_collection);

  BKE_libblock_remap(
      context.bmain, context.master_collection, nullptr, ID_REMAP_SKIP_NEVER_NULL_USAGE);

  EXPECT_EQ(context.master_collection, context.scene->master_collection);
  EXPECT_NE(context.scene->master_collection, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remap to self
 * \{ */

TEST_F(LibRemapTest, delete_when_remap_to_self_not_allowed)
{
  TwoMeshesTestData context;

  ASSERT_NE(context.mesh, nullptr);
  ASSERT_NE(context.other_mesh, nullptr);
  context.mesh->texcomesh = context.other_mesh;

  BKE_libblock_remap(context.bmain, context.other_mesh, context.mesh, 0);

  EXPECT_EQ(context.mesh->texcomesh, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name User Reference Counting
 * \{ */

TEST_F(LibRemapTest, users_are_decreased_when_not_skipping_never_null)
{
  MeshObjectTestData context;

  ASSERT_NE(context.object, nullptr);
  ASSERT_EQ(context.object->data, id_cast<ID *>(context.mesh));
  ASSERT_EQ(context.object->id.tag & ID_TAG_DOIT, 0);
  ASSERT_EQ(context.mesh->id.us, 1);

  /* This is an invalid situation, test case tests this in between value until we have a better
   * solution. */
  BKE_libblock_remap(context.bmain, context.mesh, nullptr, 0);
  EXPECT_EQ(context.mesh->id.us, 0);
  EXPECT_EQ(context.object->data, id_cast<ID *>(context.mesh));
  EXPECT_NE(context.object->data, nullptr);
  EXPECT_EQ(context.object->id.tag & ID_TAG_DOIT, 0);
}

TEST_F(LibRemapTest, users_are_same_when_skipping_never_null)
{
  MeshObjectTestData context;

  ASSERT_NE(context.object, nullptr);
  ASSERT_EQ(context.object->data, id_cast<ID *>(context.mesh));
  ASSERT_EQ(context.object->id.tag & ID_TAG_DOIT, 0);
  ASSERT_EQ(context.mesh->id.us, 1);

  BKE_libblock_remap(context.bmain, context.mesh, nullptr, ID_REMAP_SKIP_NEVER_NULL_USAGE);
  EXPECT_EQ(context.mesh->id.us, 1);
  EXPECT_EQ(context.object->data, id_cast<ID *>(context.mesh));
  EXPECT_NE(context.object->data, nullptr);
  EXPECT_EQ(context.object->id.tag & ID_TAG_DOIT, 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Never Null
 * \{ */

TEST_F(LibRemapTest, do_not_delete_when_cannot_unset)
{
  MeshObjectTestData context;

  ASSERT_NE(context.object, nullptr);
  ASSERT_EQ(context.object->data, id_cast<ID *>(context.mesh));

  BKE_libblock_remap(context.bmain, context.mesh, nullptr, ID_REMAP_SKIP_NEVER_NULL_USAGE);
  EXPECT_EQ(context.object->data, id_cast<ID *>(context.mesh));
  EXPECT_NE(context.object->data, nullptr);
}

TEST_F(LibRemapTest, force_never_null_usage)
{
  MeshObjectTestData context;

  ASSERT_NE(context.object, nullptr);
  ASSERT_EQ(context.object->data, id_cast<ID *>(context.mesh));

  BKE_libblock_remap(context.bmain, context.mesh, nullptr, ID_REMAP_FORCE_NEVER_NULL_USAGE);
  EXPECT_EQ(context.object->data, nullptr);
}

TEST_F(LibRemapTest, never_null_usage_flag_not_requested_on_delete)
{
  MeshObjectTestData context;

  ASSERT_NE(context.object, nullptr);
  ASSERT_EQ(context.object->data, id_cast<ID *>(context.mesh));
  ASSERT_EQ(context.object->id.tag & ID_TAG_DOIT, 0);

  /* Never null usage isn't requested so the flag should not be set. */
  BKE_libblock_remap(context.bmain, context.mesh, nullptr, ID_REMAP_SKIP_NEVER_NULL_USAGE);
  EXPECT_EQ(context.object->data, id_cast<ID *>(context.mesh));
  EXPECT_NE(context.object->data, nullptr);
  EXPECT_EQ(context.object->id.tag & ID_TAG_DOIT, 0);
}

TEST_F(LibRemapTest, never_null_usage_storage_requested_on_delete)
{
  MeshObjectTestData context;

  ASSERT_NE(context.object, nullptr);
  ASSERT_EQ(context.object->data, id_cast<ID *>(context.mesh));
  ASSERT_EQ(context.object->id.tag & ID_TAG_DOIT, 0);

  /* Never null usage is requested so the owner ID (the Object) should be added to the set. */
  IDRemapper remapper;
  remapper.add(&context.mesh->id, nullptr);
  BKE_libblock_remap_multiple_locked(
      context.bmain, remapper, (ID_REMAP_SKIP_NEVER_NULL_USAGE | ID_REMAP_STORE_NEVER_NULL_USAGE));

  /* Never null usages un-assignment is not enforced (no #ID_REMAP_FORCE_NEVER_NULL_USAGE),
   * so the object-data should still use the original mesh. */
  EXPECT_EQ(context.object->data, id_cast<ID *>(context.mesh));
  EXPECT_NE(context.object->data, nullptr);
  EXPECT_TRUE(remapper.never_null_users().contains(&context.object->id));
}

TEST_F(LibRemapTest, never_null_usage_flag_not_requested_on_remap)
{
  MeshObjectTestData context;
  Mesh *other_mesh = BKE_mesh_add(context.bmain, nullptr);

  ASSERT_NE(context.object, nullptr);
  ASSERT_EQ(context.object->data, id_cast<ID *>(context.mesh));
  ASSERT_EQ(context.object->id.tag & ID_TAG_DOIT, 0);

  /* Never null usage isn't requested so the flag should not be set. */
  BKE_libblock_remap(context.bmain, context.mesh, other_mesh, ID_REMAP_SKIP_NEVER_NULL_USAGE);
  EXPECT_EQ(context.object->data, id_cast<ID *>(other_mesh));
  EXPECT_EQ(context.object->id.tag & ID_TAG_DOIT, 0);
}

TEST_F(LibRemapTest, never_null_usage_storage_requested_on_remap)
{
  MeshObjectTestData context;
  Mesh *other_mesh = BKE_mesh_add(context.bmain, nullptr);

  ASSERT_NE(context.object, nullptr);
  ASSERT_EQ(context.object->data, id_cast<ID *>(context.mesh));
  ASSERT_EQ(context.object->id.tag & ID_TAG_DOIT, 0);

  /* Never null usage is requested, but the obdata is remapped to another Mesh, not to `nullptr`,
   * so the `never_null_users` set should remain empty. */
  IDRemapper remapper;
  remapper.add(&context.mesh->id, &other_mesh->id);
  BKE_libblock_remap_multiple_locked(
      context.bmain, remapper, (ID_REMAP_SKIP_NEVER_NULL_USAGE | ID_REMAP_STORE_NEVER_NULL_USAGE));
  EXPECT_EQ(context.object->data, id_cast<ID *>(other_mesh));
  EXPECT_TRUE(remapper.never_null_users().is_empty());
}

/** \} */

}  // namespace bke::tests
}  // namespace blender
