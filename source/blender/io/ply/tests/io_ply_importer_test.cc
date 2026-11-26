/* SPDX-FileCopyrightText: 2023-2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "BKE_appdir.hh"
#include "BLI_fileops.hh"
#include "BLI_path_utils.hh"

#include "CLG_log.h"

#include "IO_ply.hh"
#include "ply_export_data.hh"
#include "ply_export_header.hh"
#include "ply_file_buffer_ascii.hh"
#include "ply_file_buffer_binary.hh"
#include "ply_import.hh"
#include "ply_import_buffer.hh"
#include "ply_import_data.hh"

namespace blender {

static CLG_LogRef LOG = {"io.ply"};

namespace io::ply {

TEST(ply_import, BufferRefillTest)
{
  std::string ply_path_a;
  std::string ply_path_b;
  std::string cleanup_dir;

  std::pair<int, int> exp_edges[] = {{2, 0},
                                     {0, 1},
                                     {1, 3},
                                     {3, 2},
                                     {6, 2},
                                     {3, 7},
                                     {7, 6},
                                     {4, 6},
                                     {7, 5},
                                     {5, 4},
                                     {0, 4},
                                     {5, 1}};

  if (tests::has_test_asset_dir()) {
    ply_path_a = tests::flags_test_asset_dir() + SEP_STR "io_tests" SEP_STR "ply" SEP_STR +
                 "ASCII_wireframe_cube.ply";
    ply_path_b = tests::flags_test_asset_dir() + SEP_STR "io_tests" SEP_STR "ply" SEP_STR +
                 "wireframe_cube.ply";
  }
  else {
    BKE_tempdir_init(nullptr);
    cleanup_dir = std::string(BKE_tempdir_base()) + SEP_STR + "ply_refill_test";
    BLI_dir_create_recursive(cleanup_dir.c_str());
    ply_path_a = cleanup_dir + SEP_STR + "ASCII_wireframe_cube.ply";
    ply_path_b = cleanup_dir + SEP_STR + "wireframe_cube.ply";

    PlyData cube;
    cube.vertices = {{-1, -1, -1}, {-1, -1, 1}, {-1, 1, -1}, {-1, 1, 1},
                     {1, -1, -1},  {1, -1, 1},  {1, 1, -1},  {1, 1, 1}};
    cube.edges.extend(Span(exp_edges, 12));

    PLYExportParams ascii_params{};
    ascii_params.ascii_format = true;
    FileBufferAscii buf_a(ply_path_a.c_str());
    write_header(buf_a, cube, ascii_params);
    write_vertices(buf_a, cube);
    write_edges(buf_a, cube);
    buf_a.close_file();

    PLYExportParams bin_params{};
    bin_params.ascii_format = false;
    FileBufferBinary buf_b(ply_path_b.c_str());
    write_header(buf_b, cube, bin_params);
    write_vertices(buf_b, cube);
    write_edges(buf_b, cube);
    buf_b.close_file();
  }

  /* Use a small read buffer size to test buffer refilling behavior. */
  constexpr size_t buffer_size = 50;
  PlyReadBuffer infile_a(ply_path_a.c_str(), buffer_size);
  PlyReadBuffer infile_b(ply_path_b.c_str(), buffer_size);
  PlyHeader header_a, header_b;
  const char *header_err_a = read_header(infile_a, header_a);
  const char *header_err_b = read_header(infile_b, header_b);
  if (header_err_a != nullptr || header_err_b != nullptr) {
    CLOG_ERROR(&LOG, "Failed to read PLY header");
    ADD_FAILURE();
    if (!cleanup_dir.empty()) {
      BLI_delete(cleanup_dir.c_str(), true, false);
    }
    return;
  }
  std::unique_ptr<PlyData> data_a = import_ply_data(infile_a, header_a);
  std::unique_ptr<PlyData> data_b = import_ply_data(infile_b, header_b);
  if (!data_a->error.empty() || !data_b->error.empty()) {
    CLOG_ERROR(&LOG, "Failed to read PLY data");
    ADD_FAILURE();
    if (!cleanup_dir.empty()) {
      BLI_delete(cleanup_dir.c_str(), true, false);
    }
    return;
  }

  EXPECT_EQ_SPAN<std::pair<int, int>>(Span(exp_edges, 12), data_a->edges);
  EXPECT_EQ_SPAN<std::pair<int, int>>(Span(exp_edges, 12), data_b->edges);

  if (!cleanup_dir.empty()) {
    BLI_delete(cleanup_dir.c_str(), true, false);
  }
}

}  // namespace io::ply
}  // namespace blender
