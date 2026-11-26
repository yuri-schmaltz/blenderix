#include "BKE_mesh.h"
#include "BLI_listbase.hh"
/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "tests/blendfile_loading_base_test.h"

#include "BKE_appdir.hh"

#include "BLI_fileops.hh"
#include "BLI_string.hh"

#include "DEG_depsgraph.hh"

#include "MEM_guardedalloc.h"

#include "BKE_collection.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_scene.hh"
#include "DEG_depsgraph_build.hh"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "IO_stl.hh"
#include "stl_export.hh"

namespace blender::io::stl {

/* Set this true to keep comparison-failing test output in temp file directory. */
constexpr bool save_failing_test_output = false;

static std::string read_temp_file_in_string(const std::string &file_path)
{
  std::string res;
  size_t buffer_len;
  char *buffer = BLI_file_read_text_as_mem(file_path.c_str(), 0, &buffer_len);
  if (buffer != nullptr) {
    res.assign(buffer, buffer_len);
    MEM_delete(buffer);
  }
  return res;
}

class STLExportTest : public BlendfileLoadingBaseTest {
 public:
  bool load_file_and_depsgraph(const std::string &filepath)
  {
    if (!blendfile_load(filepath.c_str())) {
      return false;
    }
    depsgraph_create(DAG_EVAL_VIEWPORT);
    return true;
  }

 protected:
  STLExportTest()
  {
    _params.ascii_format = true;
  }
  void SetUp() override
  {
    BlendfileLoadingBaseTest::SetUp();
    BKE_tempdir_init(nullptr);
  }

  void TearDown() override
  {
    BlendfileLoadingBaseTest::TearDown();
    BKE_tempdir_session_purge();
  }

  static std::string get_temp_filename(const std::string &filename)
  {
    return std::string(BKE_tempdir_base()) + SEP_STR + filename;
  }

  /**
   * Export the given blend file with the given parameters and
   * test to see if it matches a golden file (ignoring any difference in Blender version number).
   * \param blendfile: input, relative to "tests" directory.
   * \param golden_stl: expected output, relative to "tests" directory.
   */
  void compare_to_golden(const std::string &blendfile, const std::string &golden_stl)
  {
    if (!load_file_and_depsgraph(blendfile)) {
      return;
    }

    std::string out_file_path = get_temp_filename(BLI_path_basename(golden_stl.c_str()));
    STRNCPY(_params.filepath, out_file_path.c_str());
    std::string golden_file_path = tests::flags_test_asset_dir() + SEP_STR + golden_stl;
    export_frame(depsgraph, 1.0f, _params);
    std::string output_str = read_temp_file_in_string(out_file_path);

    std::string golden_str = read_temp_file_in_string(golden_file_path);
    bool are_equal = output_str == golden_str;
    if (save_failing_test_output && !are_equal) {
      printf("failing test output in %s\n", out_file_path.c_str());
    }
    ASSERT_TRUE(are_equal);
    if (!save_failing_test_output || are_equal) {
      BLI_delete(out_file_path.c_str(), false, false);
    }
  }

  STLExportParams _params;
};

TEST_F(STLExportTest, all_tris)
{
  compare_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "all_tris.blend",
                    "io_tests" SEP_STR "stl" SEP_STR "all_tris.stl");
}

TEST_F(STLExportTest, all_quads)
{
  compare_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "all_quads.blend",
                    "io_tests" SEP_STR "stl" SEP_STR "all_quads.stl");
}

TEST_F(STLExportTest, non_uniform_scale)
{
  compare_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "non_uniform_scale.blend",
                    "io_tests" SEP_STR "stl" SEP_STR "non_uniform_scale.stl");
}

TEST_F(STLExportTest, cubes_positioned)
{
  compare_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "cubes_positioned.blend",
                    "io_tests" SEP_STR "stl" SEP_STR "cubes_positioned.stl");
}

TEST_F(STLExportTest, roundtrip_quad_mesh_ascii_and_binary)
{
  Main *bmain = BKE_main_new();
  Scene *scene = BKE_scene_add(bmain, "STLScene");
  ViewLayer *view_layer = scene->view_layers.first();

  Mesh *mesh = BKE_mesh_add(bmain, "STLQuad");
  Mesh *nomain_mesh = BKE_mesh_new_nomain(4, 4, 1, 4);
  nomain_mesh->vert_positions_for_write().copy_from(
      {float3(0, 0, 0), float3(2, 0, 0), float3(2, 2, 0), float3(0, 2, 0)});
  nomain_mesh->edges_for_write().copy_from({int2(0, 1), int2(1, 2), int2(2, 3), int2(3, 0)});
  nomain_mesh->face_offsets_for_write().copy_from({0, 4});
  nomain_mesh->corner_verts_for_write().copy_from({0, 1, 2, 3});
  nomain_mesh->corner_edges_for_write().copy_from({0, 1, 2, 3});
  BKE_mesh_nomain_to_mesh(nomain_mesh, mesh, nullptr);

  Object *ob = BKE_object_add_only_object(bmain, OB_MESH, "STLObject");
  ob->data = id_cast<ID *>(mesh);
  BKE_collection_object_add(bmain, scene->master_collection, ob);

  Depsgraph *local_depsgraph = DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_VIEWPORT);
  DEG_graph_build_from_view_layer(local_depsgraph);
  BKE_scene_graph_update_tagged(local_depsgraph, bmain);

  for (const bool ascii : {true, false}) {
    const std::string out_path = get_temp_filename(ascii ? "quad_ascii.stl" : "quad_bin.stl");
    STLExportParams export_params{};
    STRNCPY(export_params.filepath, out_path.c_str());
    export_params.ascii_format = ascii;
    export_frame(local_depsgraph, 1.0f, export_params);

    if (ascii) {
      const std::string text = read_temp_file_in_string(out_path);
      EXPECT_NE(text.find("facet normal"), std::string::npos);
    }

    STLImportParams import_params{};
    STRNCPY(import_params.filepath, out_path.c_str());
    Mesh *imported = STL_import_mesh(&import_params);
    ASSERT_NE(imported, nullptr);
    EXPECT_EQ(imported->verts_num, 4);
    EXPECT_EQ(imported->faces_num, 2);
    BKE_id_free(nullptr, imported);
    BLI_delete(out_path.c_str(), false, false);
  }

  DEG_graph_free(local_depsgraph);
  BKE_main_free(bmain);
}

}  // namespace blender::io::stl
