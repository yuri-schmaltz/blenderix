/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_material.hh"
#include "BKE_pointcache.h"
#include "DEG_depsgraph_query.hh"
#include "DNA_material_types.h"
#include "DNA_particle_types.h"
#include "ED_particle.hh"

#include "draw_cache.hh"
#include "draw_cache_impl.hh"
#include "overlay_base.hh"

namespace blender::draw::overlay {

/**
 * Display particle system overlays.
 * Covers particle edit and the legacy hair system.
 */
class Particles : Overlay {
 private:
  PassMain particle_ps_ = {"particle_ps_"};
  PassMain::Sub *dot_ps_ = nullptr;
  PassMain::Sub *shape_ps_ = nullptr;
  PassMain::Sub *hair_ps_ = nullptr;

  PassSimple edit_particle_ps_ = {"edit_particle_ps_"};
  PassSimple::Sub *edit_vert_ps_ = nullptr;
  PassSimple::Sub *edit_edge_ps_ = nullptr;

  bool show_weight_ = false;
  bool show_point_inner_ = false;
  bool show_point_tip_ = false;

 public:
  void begin_sync(Resources & /*res*/, const State & /*state*/) final
  {
    enabled_ = false;
  }

  void edit_object_sync(Manager &manager,
                        const ObjectRef &ob_ref,
                        Resources & /*res*/,
                        const State &state) final
  {
    if (!enabled_) {
      return;
    }

    /* Usually the edit structure is created by Particle Edit Mode Toggle
     * operator, but sometimes it's invoked after tagging hair as outdated
     * (for example, when toggling edit mode). That makes it impossible to
     * create edit structure for until after next dependency graph evaluation.
     *
     * Ideally, the edit structure will be created here already via some
     * dependency graph callback or so, but currently trying to make it nicer
     * only causes bad level calls and breaks design from the past.
     */
    Object *object_eval = ob_ref.object;
    Object *object_orig = DEG_get_original(object_eval);
    Scene *scene_orig = const_cast<Scene *>(DEG_get_original(state.scene));
    PTCacheEdit *edit = PE_create_current(state.depsgraph, scene_orig, object_orig);
    if (edit == nullptr) {
      /* Happens when trying to edit particles in EMITTER mode without having them cached. */
      return;
    }

    auto find_active_evaluated_psys =
        [&](ListBaseWrapper<ParticleSystem> particle_systems_orig,
            ListBaseWrapper<ParticleSystem> particle_systems_eval) -> ParticleSystem * {
      int psys_index = 0;
      for (ParticleSystem *psys_orig : particle_systems_orig) {
        if (PE_get_current_from_psys(psys_orig) == edit) {
          return particle_systems_eval.get(psys_index);
        }
        psys_index++;
      }
      return nullptr;
    };

    ParticleSystem *psys = find_active_evaluated_psys(&object_orig->particlesystem,
                                                      &object_eval->particlesystem);
    if (psys == nullptr) {
      printf("Error getting evaluated particle system for edit.\n");
      return;
    }

    Object *ob = ob_ref.object;

    ResourceHandleRange handle = manager.resource_handle_for_psys(ob_ref,
                                                                  ob_ref.particles_matrix());

    {
      Batch *geom = DRW_cache_particles_get_edit_strands(ob, psys, edit, show_weight_);
      edit_edge_ps_->draw(geom, handle);
    }
    if (show_point_inner_) {
      Batch *geom = DRW_cache_particles_get_edit_inner_points(ob, psys, edit);
      edit_vert_ps_->draw(geom, handle);
    }
    if (show_point_tip_) {
      Batch *geom = DRW_cache_particles_get_edit_tip_points(ob, psys, edit);
      edit_vert_ps_->draw(geom, handle);
    }
  }

  void object_sync(Manager &manager,
                   const ObjectRef &ob_ref,
                   Resources &res,
                   const State &state) final
  {
    if (!enabled_) {
      return;
    }

    Object *ob = ob_ref.object;

    ResourceHandleRange handle = {};

    for (ParticleSystem *psys : ListBaseWrapper<ParticleSystem>(&ob->particlesystem)) {
      if (!DRW_object_is_visible_psys_in_active_context(ob, psys)) {
        continue;
      }

      const ParticleSettings *part = psys->part;
      const int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;

      if (ELEM(draw_as, PART_DRAW_NOT, PART_DRAW_OB, PART_DRAW_GR)) {
        /* Skip early, before we try to create a handle, since we can't create these for ObjectRefs
         * with instances. Objects and groups are realized by the Depsgraph as regular instances
         * and therefore support instancing optimizations, but don't need to be handled here. */
        continue;
      }

      if (!handle.is_valid()) {
        handle = manager.resource_handle_for_psys(ob_ref, ob_ref.particles_matrix());
      }

      auto set_color = [&](PassMain::Sub &sub) {
        /* NOTE(fclem): Is color even useful in our modern context? */
        Material *ma = BKE_object_material_get_eval(ob, part->omat);
        sub.push_constant("ucolor", float4(ma ? float3(&ma->r) : float3(0.6f), part->draw_size));
      };

      Batch *geom = nullptr;
      switch (draw_as) {
        case PART_DRAW_PATH:
          if ((state.is_wireframe_mode == false) && (part->draw_as == PART_DRAW_REND)) {
            /* Render engine should have rendered it already. */
            break;
          }
          geom = DRW_cache_particles_get_hair(ob, psys, nullptr);
          hair_ps_->push_constant("use_coloring", true); /* TODO */
          hair_ps_->draw(geom, handle, res.select_id(ob_ref).get());
          break;
        case PART_DRAW_NOT:
          /* Nothing to draw. */
          break;
        case PART_DRAW_OB:
        case PART_DRAW_GR:
          /* Instances are realized by Depsgraph and rendered as a regular object instance. */
          break;
        default:
          /* Eventually, would be good to assert. But there are many other draw type that could be
           * set and they need to revert to PART_DRAW_DOT. */
          // BLI_assert_unreachable();
        case PART_DRAW_DOT:
          geom = DRW_cache_particles_get_dots(ob, psys);
          set_color(*dot_ps_);
          dot_ps_->draw(geom, handle, res.select_id(ob_ref).get());
          break;
        case PART_DRAW_AXIS:
          geom = DRW_cache_particles_get_dots(ob, psys);
          set_color(*shape_ps_);
          shape_ps_->push_constant("shape_type", int(PART_SHAPE_AXIS));
          shape_ps_->draw_expand(geom, GPU_PRIM_LINES, 3, 1, handle, res.select_id(ob_ref).get());
          break;
        case PART_DRAW_CIRC:
          geom = DRW_cache_particles_get_dots(ob, psys);
          set_color(*shape_ps_);
          shape_ps_->push_constant("shape_type", int(PART_SHAPE_CIRCLE));
          shape_ps_->draw_expand(geom,
                                 GPU_PRIM_LINES,
                                 PARTICLE_SHAPE_CIRCLE_RESOLUTION,
                                 1,
                                 handle,
                                 res.select_id(ob_ref).get());
          break;
        case PART_DRAW_CROSS:
          geom = DRW_cache_particles_get_dots(ob, psys);
          set_color(*shape_ps_);
          shape_ps_->push_constant("shape_type", int(PART_SHAPE_CROSS));
          shape_ps_->draw_expand(geom, GPU_PRIM_LINES, 3, 1, handle, res.select_id(ob_ref).get());
          break;
      }
    }
  }

  void pre_draw(Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    manager.generate_commands(particle_ps_, view);
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit_only(particle_ps_, view);
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit(edit_particle_ps_, view);
  }
};
}  // namespace blender::draw::overlay
