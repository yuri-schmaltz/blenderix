/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_material.hh"
#include "BKE_paint.hh"

#include "DNA_particle_types.h"

#include "draw_sculpt.hh"

#include "overlay_base.hh"

namespace blender::draw::overlay {

/* Add prepass which will write to the depth buffer so that the
 * alpha-under overlays (alpha checker) will draw correctly for external engines.
 * NOTE: Use the same Z-depth value as in the regular image drawing engine. */
class ImagePrepass : Overlay {
 private:
  PassSimple ps_ = {"ImagePrepass"};

 public:
  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = state.is_space_image() && state.is_image_valid && !res.is_selection();

    if (!enabled_) {
      return;
    }

    ps_.init();
    ps_.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
    ps_.shader_set(res.shaders->mesh_edit_depth.get());
    ps_.push_constant("retopology_offset", 0.0f);
    ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    ps_.draw(res.shapes.image_quad.get());
  }

  void draw_on_render(gpu::FrameBuffer *framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);
  }
};

/**
 * A depth pass that write surface depth when it is needed.
 * It is also used for selecting non overlay-only objects.
 */
class Prepass : Overlay {
 private:
  PassMain ps_ = {"prepass"};
  PassMain::Sub *mesh_ps_ = nullptr;
  PassMain::Sub *mesh_flat_ps_ = nullptr;
  PassMain::Sub *hair_ps_ = nullptr;
  PassMain::Sub *curves_ps_ = nullptr;
  PassMain::Sub *pointcloud_ps_ = nullptr;
  PassMain::Sub *gsplat_ps_ = nullptr;
  PassMain::Sub *grease_pencil_ps_ = nullptr;

  bool use_material_slot_selection_ = false;

 public:
  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = state.is_space_v3d() && (!state.xray_enabled || res.is_selection());

    if (!enabled_) {
      /* Not used. But release the data. */
      ps_.init();
      mesh_ps_ = nullptr;
      curves_ps_ = nullptr;
      pointcloud_ps_ = nullptr;
      gsplat_ps_ = nullptr;
      return;
    }

    use_material_slot_selection_ = state.is_material_select;

    bool use_cull = res.globals_buf.backface_culling;
    DRWState backface_cull_state = use_cull ? DRW_STATE_CULL_BACK : DRWState(0);

    ps_.init();
    ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    ps_.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
    ps_.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | backface_cull_state,
                  state.clipping_plane_count);
    res.select_bind(ps_);
    {
      auto &sub = ps_.sub("Mesh");
      sub.shader_set(res.is_selection() ? res.shaders->depth_mesh_conservative.get() :
                                          res.shaders->depth_mesh.get());
      mesh_ps_ = &sub;
    }
    {
      auto &sub = ps_.sub("MeshFlat");
      sub.shader_set(res.shaders->depth_mesh.get());
      mesh_flat_ps_ = &sub;
    }
    {
      auto &sub = ps_.sub("Hair");
      sub.shader_set(res.shaders->depth_mesh.get());
      hair_ps_ = &sub;
    }
    {
      auto &sub = ps_.sub("Curves");
      sub.shader_set(res.shaders->depth_curves.get());
      curves_ps_ = &sub;
    }
    {
      auto &sub = ps_.sub("PointCloud");
      sub.shader_set(res.shaders->depth_pointcloud.get());
      pointcloud_ps_ = &sub;
    }
    {
      auto &sub = ps_.sub("GSplat");
      sub.shader_set(res.shaders->depth_gsplat.get());
      gsplat_ps_ = &sub;
    }
  }

  void sculpt_sync(Manager &manager, const ObjectRef &ob_ref, Resources &res)
  {
    ResourceHandleRange handle = manager.unique_handle_for_sculpt(ob_ref);

    for (SculptBatch &batch : sculpt_batches_get(ob_ref.object, SCULPT_BATCH_DEFAULT)) {
      select::ID select_id = use_material_slot_selection_ ?
                                 res.select_id(ob_ref, (batch.material_slot + 1) << 16) :
                                 res.select_id(ob_ref);

      if (res.is_selection()) {
        /* Conservative shader needs expanded draw-call. */
        mesh_ps_->draw_expand(batch.batch, GPU_PRIM_TRIS, 1, 1, handle, select_id.get());
      }
      else {
        mesh_ps_->draw(batch.batch, handle, select_id.get());
      }
    }
  }

  void object_sync(Manager &manager,
                   const ObjectRef &ob_ref,
                   Resources &res,
                   const State &state) final
  {
    bool is_solid = ob_ref.object->dt >= OB_SOLID ||
                    (state.v3d->shading.type == OB_RENDER &&
                     !(ob_ref.object->visibility_flag & OB_HIDE_CAMERA));

    if (!enabled_ || !is_solid) {
      return;
    }

    const bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob_ref.object, state.rv3d) &&
                                 !state.is_image_render;

    if (use_sculpt_pbvh) {
      sculpt_sync(manager, ob_ref, res);
      return;
    }

    /* At most one of these is set. geom_list is only filled for material slot selection. */
    Batch *geom_single = nullptr;
    Span<Batch *> geom_list;

    ResourceHandleRange res_handle = manager.unique_handle(ob_ref);

    PassMain::Sub *pass = nullptr;
    switch (ob_ref.object->type) {
      case OB_MESH:
        if (use_material_slot_selection_) {
          /* TODO(fclem): Improve the API. */
          const int materials_len = BKE_object_material_used_with_fallback_eval(*ob_ref.object);
          Array<GPUMaterial *> materials(materials_len, nullptr);
          geom_list = DRW_cache_mesh_surface_shaded_get(ob_ref.object, materials);
        }
        else {
          geom_single = DRW_cache_mesh_surface_get(ob_ref.object);

          if (res.is_selection() && !use_material_slot_selection_ &&
              FlatObjectRef::flat_axis_index_get(ob_ref) != -1)
          {
            /* Avoid losing flat objects when in ortho views (see #56549) */
            mesh_flat_ps_->draw(DRW_cache_mesh_all_edges_get(ob_ref.object),
                                manager.unique_handle(ob_ref),
                                res.select_id(ob_ref).get());
          }
        }
        pass = mesh_ps_;
        break;
      default:
        break;
    }

    if (pass == nullptr) {
      return;
    }

    if (!geom_list.is_empty()) {
      for (int material_id : geom_list.index_range()) {
        /* Meshes with more than 16 materials can have nullptr in the geometry list as materials
         * are not filled for unused materials indices. We should actually use
         * `material_indices_used` but these are only available for meshes. */
        if (geom_list[material_id] == nullptr) {
          continue;
        }

        select::ID select_id = res.select_id(ob_ref, (material_id + 1) << 16);
        if (res.is_selection() && (pass == mesh_ps_)) {
          /* Conservative shader needs expanded draw-call. */
          pass->draw_expand(
              geom_list[material_id], GPU_PRIM_TRIS, 1, 1, res_handle, select_id.get());
        }
        else {
          pass->draw(geom_list[material_id], res_handle, select_id.get());
        }
      }
    }
    else if (geom_single != nullptr) {
      select::ID select_id = res.select_id(ob_ref);
      if (res.is_selection() && (pass == mesh_ps_)) {
        /* Conservative shader needs expanded draw-call. */
        pass->draw_expand(geom_single, GPU_PRIM_TRIS, 1, 1, res_handle, select_id.get());
      }
      else {
        pass->draw(geom_single, res_handle, select_id.get());
      }
    }
  }

  void pre_draw(Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    manager.generate_commands(ps_, view);
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }
    /* Should be fine to use the line buffer since the prepass only writes to the depth buffer. */
    GPU_framebuffer_bind(framebuffer);
    manager.submit_only(ps_, view);
  }
};

}  // namespace blender::draw::overlay
