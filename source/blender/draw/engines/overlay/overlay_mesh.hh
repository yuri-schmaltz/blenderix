/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include <string>

#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_image_gpu.hh"
#include "BKE_mask.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_types.hh"
#include "BKE_paint.hh"
#include "BKE_subdiv_modifier.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_brush_types.h"
#include "DNA_mask_types.h"
#include "DNA_mesh_types.h"
#include "DNA_userdef_types.h"

#include "ED_view3d.hh"

#include "GPU_capabilities.hh"

#include "draw_cache.hh"
#include "draw_cache_impl.hh"
#include "draw_manager_text.hh"
#include "overlay_base.hh"

namespace blender::draw::overlay {

constexpr int overlay_edit_text = V3D_OVERLAY_EDIT_EDGE_LEN | V3D_OVERLAY_EDIT_FACE_AREA |
                                  V3D_OVERLAY_EDIT_FACE_ANG | V3D_OVERLAY_EDIT_EDGE_ANG |
                                  V3D_OVERLAY_EDIT_INDICES;

/**
 * Draw edit mesh overlays.
 */
class Meshes : Overlay {
 private:
  PassSimple edit_mesh_normals_ps_ = {"Normals"};
  PassSimple::Sub *face_normals_ = nullptr;
  PassSimple::Sub *loop_normals_ = nullptr;
  PassSimple::Sub *vert_normals_ = nullptr;

  PassSimple edit_mesh_analysis_ps_ = {"Mesh Analysis"};
  PassSimple edit_mesh_weight_ps_ = {"Edit Weight"};

  PassSimple edit_mesh_edges_ps_ = {"Edges"};
  PassSimple edit_mesh_faces_ps_ = {"Faces"};
  PassSimple edit_mesh_cages_ps_ = {"Cages"}; /* Same as faces but with a different offset. */
  PassSimple edit_mesh_verts_ps_ = {"Verts"};
  PassSimple edit_mesh_facedots_ps_ = {"FaceDots"};
  PassSimple edit_mesh_skin_roots_ps_ = {"SkinRoots"};

  /* Depth pre-pass to cull edit cage in case the object is not opaque. */
  PassSimple edit_mesh_prepass_ps_ = {"Prepass"};

  bool xray_enabled_ = false;
  bool xray_flag_enabled_ = false;

  bool show_retopology_ = false;
  bool show_mesh_analysis_ = false;
  bool show_face_overlay_ = false;
  bool show_weight_ = false;

  bool select_vert_ = false;
  bool select_edge_ = false;
  bool select_face_ = false;
  bool select_face_dots_ = false;

  /**
   * Depth offsets applied in screen space to different edit overlay components.
   * This is multiplied by a factor based on zoom level computed by `GPU_polygon_offset_calc`.
   */
  static constexpr float cage_ndc_offset_ = 0.5f;
  static constexpr float edge_ndc_offset_ = 1.0f;
  static constexpr float vert_ndc_offset_ = 1.5f;

  /* TODO(fclem): This is quite wasteful and expensive, prefer in shader Z modification like the
   * retopology offset. */
  View view_edit_cage_ = {"view_edit_cage"};
  View::OffsetData offset_data_;

 public:
  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = state.is_space_v3d();

    if (!enabled_) {
      return;
    }

    offset_data_ = state.offset_data_get();
    xray_enabled_ = state.xray_enabled;
    xray_flag_enabled_ = state.xray_flag_enabled;

    const int edit_flag = state.v3d->overlay.edit_flag;

    const ToolSettings *tsettings = state.scene->toolsettings;
    select_vert_ = (tsettings->selectmode & SCE_SELECT_VERTEX);
    select_edge_ = (tsettings->selectmode & SCE_SELECT_EDGE);
    select_face_ = (tsettings->selectmode & SCE_SELECT_FACE);
    select_face_dots_ = ((edit_flag & V3D_OVERLAY_EDIT_FACE_DOT) || state.xray_flag_enabled) &
                        select_face_;

    show_retopology_ = (edit_flag & V3D_OVERLAY_EDIT_RETOPOLOGY) && !state.xray_enabled;
    show_mesh_analysis_ = (edit_flag & V3D_OVERLAY_EDIT_STATVIS);
    show_face_overlay_ = (edit_flag & V3D_OVERLAY_EDIT_FACES);
    show_weight_ = (edit_flag & V3D_OVERLAY_EDIT_WEIGHT);

    const bool show_face_nor = (edit_flag & V3D_OVERLAY_EDIT_FACE_NORMALS);
    const bool show_loop_nor = (edit_flag & V3D_OVERLAY_EDIT_LOOP_NORMALS);
    const bool show_vert_nor = (edit_flag & V3D_OVERLAY_EDIT_VERT_NORMALS);

    const bool do_smooth_wire = (U.gpu_flag & USER_GPU_FLAG_NO_EDIT_MODE_SMOOTH_WIRE) == 0;
    const bool is_wire_shading_mode = (state.v3d->shading.type == OB_WIRE);

    uint4 data_mask = data_mask_get(edit_flag);

    float backwire_opacity = (state.xray_flag_enabled) ? 0.5f : 1.0f;
    float face_alpha = (show_face_overlay_) ? 1.0f : 0.0f;
    float retopology_offset = state.is_depth_only_drawing ? 0.0f : RETOPOLOGY_OFFSET(state.v3d);
    /* Cull back-faces for retopology face pass. This makes it so back-faces are not drawn.
     * Doing so lets us distinguish back-faces from front-faces. */
    DRWState face_culling = (show_retopology_) ? DRW_STATE_CULL_BACK : DRWState(0);

    gpu::Texture **depth_tex = (state.xray_flag_enabled) ? &res.depth_tx : &res.dummy_depth_tx;

    {
      auto &pass = edit_mesh_prepass_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | face_culling,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders->mesh_edit_depth.get());
      pass.push_constant("retopology_offset", retopology_offset);
      pass.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
      pass.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
    }
    {
      /* Normals */
      const bool use_screen_size = (edit_flag & V3D_OVERLAY_EDIT_CONSTANT_SCREEN_SIZE_NORMALS);
      const bool use_hq_normals = (state.scene->r.perf_flag & SCE_PERF_HQ_NORMALS) ||
                                  GPU_use_hq_normals_workaround();

      DRWState pass_state = DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR |
                            DRW_STATE_DEPTH_LESS_EQUAL;
      if (state.xray_flag_enabled) {
        pass_state |= DRW_STATE_BLEND_ALPHA;
      }

      auto &pass = edit_mesh_normals_ps_;
      pass.init();
      pass.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
      pass.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
      pass.state_set(pass_state, state.clipping_plane_count);

      auto shader_pass = [&](gpu::Shader *shader, const char *name) {
        auto &sub = pass.sub(name);
        sub.shader_set(shader);
        sub.bind_texture("depth_tx", depth_tex);
        sub.push_constant("alpha", backwire_opacity);
        sub.push_constant("is_constant_screen_size_normals", use_screen_size);
        sub.push_constant("normal_size", state.overlay.normals_length);
        sub.push_constant("normal_screen_size", state.overlay.normals_constant_screen_size);
        sub.push_constant("retopology_offset", retopology_offset);
        sub.push_constant("hq_normals", use_hq_normals);
        return &sub;
      };

      face_normals_ = loop_normals_ = vert_normals_ = nullptr;

      if (show_face_nor) {
        face_normals_ = shader_pass(res.shaders->mesh_face_normal.get(), "FaceNor");
      }
      if (show_loop_nor) {
        loop_normals_ = shader_pass(res.shaders->mesh_loop_normal.get(), "LoopNor");
      }
      if (show_vert_nor) {
        vert_normals_ = shader_pass(res.shaders->mesh_vert_normal.get(), "VertexNor");
      }
    }
    {
      /* Support masked transparency in Workbench.
       * EEVEE can't be supported since depth won't match. */
      const bool shadeless = eDrawType(state.v3d->shading.type) == OB_WIRE;

      auto &pass = edit_mesh_weight_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WRITE_DEPTH,
                     state.clipping_plane_count);
      pass.shader_set(shadeless ? res.shaders->paint_weight.get() :
                                  res.shaders->paint_weight_fake_shading.get());
      pass.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
      pass.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
      pass.bind_texture("colorramp", &res.weight_ramp_tx);
      pass.push_constant("draw_contours", false);
      pass.push_constant("opacity", state.overlay.weight_paint_mode_opacity);
      if (!shadeless) {
        /* Arbitrary light to give a hint of the geometry behind the weights. */
        pass.push_constant("light_dir", math::normalize(float3(0.0f, 0.5f, 0.86602f)));
      }
    }
    {
      auto &pass = edit_mesh_analysis_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders->mesh_analysis.get());
      pass.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
      pass.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
      pass.bind_texture("weight_tx", res.weight_ramp_tx);
    }

    auto mesh_edit_common_resource_bind = [&](PassSimple &pass, float alpha, float ndc_offset) {
      pass.bind_texture("depth_tx", depth_tex);
      /* TODO(fclem): UBO. */
      pass.push_constant("wire_shading", is_wire_shading_mode);
      pass.push_constant("select_face", select_face_);
      pass.push_constant("select_edge", select_edge_);
      pass.push_constant("alpha", alpha);
      pass.push_constant("retopology_offset", retopology_offset);
      pass.push_constant("ndc_offset_factor", &state.ndc_offset_factor);
      pass.push_constant("ndc_offset", ndc_offset);
      pass.push_constant("data_mask", int4(data_mask));
      pass.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
      pass.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
    };

    {
      auto &pass = edit_mesh_edges_ps_;
      pass.init();
      /* Change first vertex convention to match blender loop structure. */
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                         DRW_STATE_FIRST_VERTEX_CONVENTION,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders->mesh_edit_edge.get());
      pass.push_constant("do_smooth_wire", do_smooth_wire);
      pass.push_constant("use_vertex_selection", select_vert_);
      mesh_edit_common_resource_bind(pass, backwire_opacity, edge_ndc_offset_);
    }
    {
      auto &pass = edit_mesh_faces_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                         face_culling,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders->mesh_edit_face.get());
      mesh_edit_common_resource_bind(pass, face_alpha, 0.0f);
    }
    {
      auto &pass = edit_mesh_cages_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders->mesh_edit_face.get());
      mesh_edit_common_resource_bind(pass, face_alpha, cage_ndc_offset_);
    }
    {
      auto &pass = edit_mesh_verts_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                         DRW_STATE_WRITE_DEPTH,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders->mesh_edit_vert.get());
      mesh_edit_common_resource_bind(pass, backwire_opacity, vert_ndc_offset_);
    }
    {
      auto &pass = edit_mesh_facedots_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                         DRW_STATE_WRITE_DEPTH,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders->mesh_edit_facedot.get());
      mesh_edit_common_resource_bind(pass, backwire_opacity, vert_ndc_offset_);
    }
    {
      auto &pass = edit_mesh_skin_roots_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                         DRW_STATE_WRITE_DEPTH,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders->mesh_edit_skin_root.get());
      pass.push_constant("retopology_offset", retopology_offset);
      pass.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
      pass.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
    }
  }

  void edit_object_sync(Manager &manager,
                        const ObjectRef &ob_ref,
                        Resources & /*res*/,
                        const State &state) final
  {
    if (!enabled_) {
      return;
    }

    ResourceHandleRange res_handle = manager.unique_handle(ob_ref);

    Object *ob = ob_ref.object;
    Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(*ob);
    const bool draw_as_solid = (ob->dt > OB_WIRE) && !state.xray_enabled;
    const bool has_edit_cage = mesh_has_edit_cage(ob);

    if (show_retopology_) {
      Batch *geom = DRW_mesh_batch_cache_get_edit_triangles(mesh);
      edit_mesh_prepass_ps_.draw(geom, res_handle);
    }
    if (draw_as_solid && !state.is_render_depth_available) {
      Batch *geom = DRW_cache_mesh_surface_get(ob);
      edit_mesh_prepass_ps_.draw(geom, res_handle);
    }

    if (show_mesh_analysis_) {
      Batch *geom = DRW_cache_mesh_surface_mesh_analysis_get(ob);
      edit_mesh_analysis_ps_.draw(geom, res_handle);
    }

    if (show_weight_) {
      Batch *geom = DRW_cache_mesh_surface_weights_get(ob);
      edit_mesh_weight_ps_.draw(geom, res_handle);
    }

    if (face_normals_) {
      Batch *geom = DRW_mesh_batch_cache_get_edit_facedots(mesh);
      face_normals_->draw_expand(geom, GPU_PRIM_LINES, 1, 1, res_handle);
    }
    if (loop_normals_) {
      Batch *geom = DRW_mesh_batch_cache_get_edit_loop_normals(mesh);
      loop_normals_->draw_expand(geom, GPU_PRIM_LINES, 1, 1, res_handle);
    }
    if (vert_normals_) {
      Batch *geom = DRW_mesh_batch_cache_get_edit_vert_normals(mesh);
      vert_normals_->draw_expand(geom, GPU_PRIM_LINES, 1, 1, res_handle);
    }

    {
      Batch *geom = DRW_mesh_batch_cache_get_edit_edges(mesh);
      edit_mesh_edges_ps_.draw_expand(geom, GPU_PRIM_TRIS, 2, 1, res_handle);
    }
    {
      Batch *geom = DRW_mesh_batch_cache_get_edit_triangles(mesh);
      (has_edit_cage ? &edit_mesh_cages_ps_ : &edit_mesh_faces_ps_)->draw(geom, res_handle);
    }
    if (select_vert_) {
      Batch *geom = DRW_mesh_batch_cache_get_edit_vertices(mesh);
      edit_mesh_verts_ps_.draw(geom, res_handle);
    }
    if (select_face_dots_) {
      Batch *geom = DRW_mesh_batch_cache_get_edit_facedots(mesh);
      edit_mesh_facedots_ps_.draw(geom, res_handle);
    }

    if (mesh_has_skin_roots(ob)) {
      Batch *geom = DRW_mesh_batch_cache_get_edit_skin_roots(mesh);
      edit_mesh_skin_roots_ps_.draw_expand(geom, GPU_PRIM_LINES, 15, 1, res_handle);
    }
    if (state.show_text && (state.overlay.edit_flag & overlay_edit_text)) {
      DRW_text_edit_mesh_measure_stats(state.region, state.v3d, ob, state.scene->unit, state.dt);
    }
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    GPU_debug_group_begin("Mesh Edit");

    GPU_framebuffer_bind(framebuffer);
    manager.submit(edit_mesh_prepass_ps_, view);
    manager.submit(edit_mesh_analysis_ps_, view);
    manager.submit(edit_mesh_weight_ps_, view);

    if (!xray_enabled_) {
      /* Still use depth-testing for selected faces when X-Ray flag is enabled but transparency is
       * off (X-Ray Opacity == 1.0 or in Preview/Render mode) (See #135325). */
      manager.submit(edit_mesh_faces_ps_, view);
      manager.submit(edit_mesh_cages_ps_, view);
    }

    if (xray_flag_enabled_) {
      GPU_debug_group_end();
      return;
    }

    manager.submit(edit_mesh_normals_ps_, view);
    manager.submit(edit_mesh_edges_ps_, view);
    manager.submit(edit_mesh_verts_ps_, view);
    manager.submit(edit_mesh_skin_roots_ps_, view);
    manager.submit(edit_mesh_facedots_ps_, view);

    GPU_debug_group_end();
  }

  void draw_line_only(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    if (xray_enabled_) {
      /* Still use depth-testing for selected faces when X-Ray flag is enabled but transparency is
       * off (X-Ray Opacity == 1.0 or in Preview/Render mode) (See #135325). */
      GPU_framebuffer_bind(framebuffer);
      manager.submit(edit_mesh_faces_ps_, view);
      manager.submit(edit_mesh_cages_ps_, view);
    }

    if (!xray_flag_enabled_) {
      return;
    }

    GPU_debug_group_begin("Mesh Edit Line Only");

    GPU_framebuffer_bind(framebuffer);
    manager.submit(edit_mesh_normals_ps_, view);
    manager.submit(edit_mesh_edges_ps_, view);
    manager.submit(edit_mesh_verts_ps_, view);
    manager.submit(edit_mesh_skin_roots_ps_, view);
    manager.submit(edit_mesh_facedots_ps_, view);

    GPU_debug_group_end();
  }

  static bool mesh_has_edit_cage(const Object *ob)
  {
    BLI_assert(ob->type == OB_MESH);
    Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(*ob);
    if (mesh.runtime->edit_mesh != nullptr) {
      const Mesh *editmesh_eval_final = BKE_object_get_editmesh_eval_final(ob);
      const Mesh *editmesh_eval_cage = BKE_object_get_editmesh_eval_cage(ob);

      return (editmesh_eval_cage != nullptr) && (editmesh_eval_cage != editmesh_eval_final);
    }
    return false;
  }

 private:
  uint4 data_mask_get(const int flag)
  {
    uint4 mask = {0xFF, 0xFF, 0x00, 0x00};
    SET_FLAG_FROM_TEST(mask[0], flag & V3D_OVERLAY_EDIT_FACES, VFLAG_FACE_SELECTED);
    SET_FLAG_FROM_TEST(mask[0], flag & V3D_OVERLAY_EDIT_FREESTYLE_FACE, VFLAG_FACE_FREESTYLE);
    SET_FLAG_FROM_TEST(mask[1], flag & V3D_OVERLAY_EDIT_FREESTYLE_EDGE, VFLAG_EDGE_FREESTYLE);
    SET_FLAG_FROM_TEST(mask[1], flag & V3D_OVERLAY_EDIT_SEAMS, VFLAG_EDGE_SEAM);
    SET_FLAG_FROM_TEST(mask[1], flag & V3D_OVERLAY_EDIT_SHARP, VFLAG_EDGE_SHARP);
    SET_FLAG_FROM_TEST(mask[2], flag & V3D_OVERLAY_EDIT_CREASES, 0xFF);
    SET_FLAG_FROM_TEST(mask[3], flag & V3D_OVERLAY_EDIT_BWEIGHTS, 0xFF);
    return mask;
  }

  static bool mesh_has_skin_roots(const Object *ob)
  {
    /* The skin data is edited on the original mesh, the evaluated mesh may not have it. */
    const Mesh *mesh_orig = BKE_object_get_pre_modified_mesh(ob);
    if (!mesh_orig) {
      return false;
    }
    if (const BMesh *bm = BKE_editmesh_bmesh_get(mesh_orig)) {
      if (CustomData_get_offset_named(&bm->vdata, CD_PROP_FLOAT2, "skin_modifier_radius") == -1) {
        return false;
      }
      if (CustomData_get_offset_named(&bm->vdata, CD_PROP_BOOL, "skin_modifier_root") == -1) {
        return false;
      }
      return true;
    }
    return false;
  }
};

}  // namespace blender::draw::overlay
