/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "DNA_anim_types.h"
#include "DNA_constraint_types.h"
#include "DNA_mesh_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_function_ref.hh"
#include "BLI_kdtree_new.hh"
#include "BLI_linklist_stack.hh"
#include "BLI_listbase.hh"
#include "BLI_math_matrix_c.hh"
#include "BLI_math_vector_c.hh"

#include "BKE_constraint.h"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_modifier.hh"
#include "BKE_object_types.hh"
#include "BKE_scene.hh"

#include "ED_screen.hh"
#include "ED_screen_types.hh"

#include "WM_types.hh"

#include "DEG_depsgraph_build.hh"

#include "transform.hh"

/* Own include. */
#include "transform_convert.hh"

namespace blender::ed::transform {

bool transform_mode_use_local_origins(const TransInfo *t)
{
  return ELEM(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL);
}

void transform_around_single_fallback_ex(TransInfo *t, int data_len_all)
{
  if (data_len_all != 1) {
    return;
  }
  if (!ELEM(t->around, V3D_AROUND_CENTER_BOUNDS, V3D_AROUND_CENTER_MEDIAN, V3D_AROUND_ACTIVE)) {
    return;
  }
  if (!transform_mode_use_local_origins(t)) {
    return;
  }
  if (t->flag & T_OVERRIDE_CENTER) {
    return;
  }

  t->around = V3D_AROUND_LOCAL_ORIGINS;
}

void transform_around_single_fallback(TransInfo *t)
{
  transform_around_single_fallback_ex(t, t->data_len_all);
}

/* -------------------------------------------------------------------- */
/** \name Proportional Editing
 * \{ */

/**
 * Construct `tc->sorted_index_map` so that its indices visit `tc->{data,data_ext,data_2d}` in
 * sorted order, given the compare function.
 */
static void make_sorted_index_map(TransDataContainer *tc, FunctionRef<bool(int, int)> compare)
{
  BLI_assert(tc->sorted_index_map == nullptr);
  tc->sorted_index_map = MEM_new_array_uninitialized<int>(tc->data_len, __func__);

  const MutableSpan sorted_index_span(tc->sorted_index_map, tc->data_len);
  array_utils::fill_index_range(sorted_index_span);
  std::ranges::sort(sorted_index_span, compare);
}

/**
 * Construct an index map to visit `tc->data`, `tc->data_ext`, and `tc->data_2d` in order of
 * selection state (selected first). Unselected items are visited by either their `dist` or `rdist`
 * property, depending on a flag in `t`.
 */
static void sort_trans_data_dist_container(const TransInfo *t, TransDataContainer *tc)
{
  const bool use_dist = (t->flag & T_PROP_CONNECTED);
  const auto compare = [&](const int a, const int b) {
    /* If both selected, then they are equivalent. To keep memory access sequential (and thus more
     * predictable for pre-caching) when iterating the arrays, keep them sorted by array index. */
    const bool is_selected_a = tc->data[a].flag & TD_SELECTED;
    const bool is_selected_b = tc->data[b].flag & TD_SELECTED;
    if (is_selected_a && is_selected_b) {
      return a < b;
    }

    /* Selected comes before unselected. */
    if (is_selected_a) {
      return true;
    }
    if (is_selected_b) {
      return false;
    }

    /* If both are unselected, only then the distance matters. */
    if (use_dist) {
      return tc->data[a].dist < tc->data[b].dist;
    }
    return tc->data[a].rdist < tc->data[b].rdist;
  };

  /* The "sort by distance" is often preceded by "calculate distance", which is
   * often preceded by "sort selected first". */
  MEM_SAFE_DELETE(tc->sorted_index_map);

  make_sorted_index_map(tc, compare);
}
void sort_trans_data_dist(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    sort_trans_data_dist_container(t, tc);
  }
}

/**
 * Construct an index map to visit `tc->data`, `tc->data_ext`, and `tc->data_2d` in order of
 * selection state (selected first).
 */
static void sort_trans_data_selected_first_container(TransDataContainer *tc)
{
  BLI_assert_msg(tc->sorted_index_map == nullptr,
                 "Expected sorting by selection state to only happen once");

  const auto compare = [&](const int a, const int b) {
    /* If the selection state is the same, they are equivalent. To keep memory
     * access sequential (and thus more predictable for pre-caching) when
     * iterating the arrays, keep them sorted by array index. */
    const bool is_selected_a = tc->data[a].flag & TD_SELECTED;
    const bool is_selected_b = tc->data[b].flag & TD_SELECTED;
    if (is_selected_a == is_selected_b) {
      return a < b;
    }

    /* If A is selected, a comes before b, so return true.
     * If B is selected, a comes after b, so return false. */
    return is_selected_a;
  };
  make_sorted_index_map(tc, compare);
}
static void sort_trans_data_selected_first(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    sort_trans_data_selected_first_container(tc);
  }
}

static float3 prop_dist_loc_get(const TransDataContainer *tc,
                                const TransData *td,
                                const bool use_island,
                                const float proj_vec[3])
{
  float3 vec;

  if (use_island) {
    if (tc->use_local_mat) {
      mul_v3_m4v3(vec, tc->mat, td->iloc);
    }
    else {
      mul_v3_m3v3(vec, td->mtx, td->iloc);
    }
  }
  else {
    if (tc->use_local_mat) {
      mul_v3_m4v3(vec, tc->mat, td->center);
    }
    else {
      mul_v3_m3v3(vec, td->mtx, td->center);
    }
  }

  if (proj_vec) {
    float vec_p[3];
    project_v3_v3v3(vec_p, vec, proj_vec);
    sub_v3_v3(vec, vec_p);
  }

  return vec;
}

/**
 * Distance calculated from not-selected vertex to nearest selected vertex.
 * If the #transdata_check_local_islands() check succeeds, this will also change
 * the TransData center and axismtx of unselected points to the center and axismtx of the closest
 * point found (for proportional editing around individual origins).
 */
static void set_prop_dist(TransInfo *t, const bool with_dist)
{
  float _proj_vec[3];
  const float *proj_vec = nullptr;

  /* Support for face-islands. */
  const bool use_island = transdata_check_local_islands(t, t->around);

  if (t->flag & T_PROP_PROJECTED) {
    if (t->spacetype == SPACE_VIEW3D && t->region && t->region->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3d = static_cast<RegionView3D *>(t->region->regiondata);
      normalize_v3_v3(_proj_vec, rv3d->viewinv[2]);
      proj_vec = _proj_vec;
    }
  }

  /* Count number of selected. */
  int td_table_len = 0;
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    tc->foreach_index_selected([&](const int /*i*/) { td_table_len++; });
  }

  /* Pointers to selected's #TransData.
   * Used to find #TransData from the index returned by #KDTree::find_nearest. */
  TransData **td_table = MEM_new_array_uninitialized<TransData *>(td_table_len, __func__);

  /* Selected's positions - in global or proj_vec space. */
  Array<float3> td_positions(td_table_len);

  int td_table_index = 0;
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    tc->foreach_index_selected([&](const int i) {
      TransData *td = &tc->data[i];
      /* Initialize, it was malloced. */
      td->rdist = 0.0f;

      const float3 vec = prop_dist_loc_get(tc, td, use_island, proj_vec);

      td_positions[td_table_index] = vec;
      td_table[td_table_index++] = td;
    });
  }
  BLI_assert(td_table_index == td_table_len);

  const KDTree<float3> td_tree(td_positions);

  /* For each non-selected vertex, find distance to the nearest selected vertex. */
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    tc->foreach_index([&](const int i) {
      TransData *td = &tc->data[i];
      if (td->flag & TD_SELECTED) {
        return true;
      }

      const float3 vec = prop_dist_loc_get(tc, td, use_island, proj_vec);

      float distance_sq;
      const int td_index = td_tree.find_nearest(
          vec, std::numeric_limits<float>::max(), &distance_sq);

      td->rdist = -1.0f;
      if (td_index != -1) {
        td->rdist = std::sqrt(distance_sq);
        if (use_island) {
          /* Use center and axismtx of closest point found. */
          copy_v3_v3(td->center, td_table[td_index]->center);
          copy_m3_m3(td->axismtx, td_table[td_index]->axismtx);
        }
      }

      if (with_dist) {
        td->dist = td->rdist;
      }
      return true;
    });
  }

  MEM_delete(td_table);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Utilities
 * \{ */

bool constraints_list_needinv(TransInfo *t, ListBaseT<bConstraint> *list)
{
  /* Loop through constraints, checking if there's one of the mentioned
   * constraints needing special crazy-space corrections. */
  if (list) {
    for (bConstraint &con : *list) {
      /* Only consider constraint if it is enabled, and has influence on result. */
      if (!BKE_constraint_has_influence(&con)) {
        continue;
      }

      /* Affirmative: returns for specific constraints here. */
      /* Constraints that require this regardless. */
      if (ELEM(con.type,
               CONSTRAINT_TYPE_FOLLOWPATH,
               CONSTRAINT_TYPE_CLAMPTO,
               CONSTRAINT_TYPE_ARMATURE,
               CONSTRAINT_TYPE_OBJECTSOLVER,
               CONSTRAINT_TYPE_FOLLOWTRACK))
      {
        return true;
      }

      /* Constraints that require this only under special conditions. */
      if (con.type == CONSTRAINT_TYPE_CHILDOF) {
        /* ChildOf constraint only works when using all location components, see #42256. */
        bChildOfConstraint *data = static_cast<bChildOfConstraint *>(con.data);

        if ((data->flag & CHILDOF_LOCX) && (data->flag & CHILDOF_LOCY) &&
            (data->flag & CHILDOF_LOCZ))
        {
          return true;
        }
      }
      else if (con.type == CONSTRAINT_TYPE_ROTLIKE) {
        /* CopyRot constraint only does this when rotating, and offset is on. */
        bRotateLikeConstraint *data = static_cast<bRotateLikeConstraint *>(con.data);

        if (ELEM(data->mix_mode, ROTLIKE_MIX_OFFSET, ROTLIKE_MIX_BEFORE) &&
            ELEM(t->mode, TFM_ROTATION))
        {
          return true;
        }
      }
      else if (con.type == CONSTRAINT_TYPE_TRANSLIKE) {
        /* Copy Transforms constraint only does this in the Before mode. */
        bTransLikeConstraint *data = static_cast<bTransLikeConstraint *>(con.data);

        if (ELEM(data->mix_mode, TRANSLIKE_MIX_BEFORE, TRANSLIKE_MIX_BEFORE_FULL) &&
            ELEM(t->mode, TFM_ROTATION, TFM_TRANSLATION))
        {
          return true;
        }
        if (ELEM(data->mix_mode, TRANSLIKE_MIX_BEFORE_SPLIT) && ELEM(t->mode, TFM_ROTATION)) {
          return true;
        }
      }
      else if (con.type == CONSTRAINT_TYPE_ACTION) {
        /* The Action constraint only does this in the Before mode. */
        bActionConstraint *data = static_cast<bActionConstraint *>(con.data);

        if (ELEM(data->mix_mode, ACTCON_MIX_BEFORE, ACTCON_MIX_BEFORE_FULL) &&
            ELEM(t->mode, TFM_ROTATION, TFM_TRANSLATION))
        {
          return true;
        }
        if (ELEM(data->mix_mode, ACTCON_MIX_BEFORE_SPLIT) && ELEM(t->mode, TFM_ROTATION)) {
          return true;
        }
      }
      else if (con.type == CONSTRAINT_TYPE_TRANSFORM) {
        /* Transform constraint needs it for rotation at least (r.57309),
         * but doing so when translating may also mess things up, see: #36203. */
        bTransformConstraint *data = static_cast<bTransformConstraint *>(con.data);

        if (data->to == TRANS_ROTATION) {
          if (t->mode == TFM_ROTATION && data->mix_mode_rot == TRANS_MIXROT_BEFORE) {
            return true;
          }
        }
      }
    }
  }

  /* No appropriate candidates found. */
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (After-Transform Update)
 * \{ */

void special_aftertrans_update(bContext *C, TransInfo *t)
{
  /* NOTE: Sequencer freeing has its own function now because of a conflict
   * with transform's order of freeing (campbell).
   * Order changed, the sequencer stuff should go back in here. */

  /* Early out when nothing happened. */
  if (t->data_len_all == 0 || t->mode == TFM_DUMMY) {
    return;
  }

  if (!t->data_type || !t->data_type->special_aftertrans_update) {
    return;
  }

  BLI_assert(CTX_data_main(t->context) == CTX_data_main(C));
  t->data_type->special_aftertrans_update(C, t);
}

int special_transform_moving(TransInfo *t)
{
  if (t->options & CTX_CURSOR) {
    return G_TRANSFORM_CURSOR;
  }
  if (t->spacetype == SPACE_SEQ) {
    return G_TRANSFORM_SEQ;
  }
  if (t->spacetype == SPACE_GRAPH) {
    return G_TRANSFORM_FCURVES;
  }
  if ((t->flag & T_EDIT) || (t->options & CTX_POSE_BONE)) {
    return G_TRANSFORM_EDIT;
  }
  if (t->options & (CTX_OBJECT | CTX_TEXTURE_SPACE)) {
    return G_TRANSFORM_OBJ;
  }

  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Data Create
 * \{ */

static int countAndCleanTransDataContainer(TransInfo *t)
{
  BLI_assert(ELEM(t->data_len_all, 0, -1));
  t->data_len_all = 0;
  int data_container_len_orig = t->data_container_len;
  for (TransDataContainer *th_end = t->data_container - 1,
                          *tc = &t->data_container[t->data_container_len - 1];
       tc != th_end;
       tc--)
  {
    if (tc->data_len == 0) {
      uint index = tc - t->data_container;
      if (index + 1 != t->data_container_len) {
        std::swap(t->data_container[index], t->data_container[t->data_container_len - 1]);
      }
      t->data_container_len -= 1;
    }
    else {
      t->data_len_all += tc->data_len;
    }
  }
  if (data_container_len_orig != t->data_container_len) {
    t->data_container = static_cast<TransDataContainer *>(MEM_realloc_uninitialized(
        t->data_container, sizeof(*t->data_container) * t->data_container_len));
  }
  return t->data_len_all;
}

static void init_proportional_edit(TransInfo *t)
{
  if (!ELEM(t->data_type,
            &TransConvertType_Mesh,
            &TransConvertType_MeshEdge,
            &TransConvertType_MeshSkin,
            &TransConvertType_MeshVertCData,
            &TransConvertType_Object))
  {
    /* Disable proportional editing. */
    t->options |= CTX_NO_PET;
    t->flag &= ~T_PROP_EDIT_ALL;
    return;
  }

  if (t->data_len_all && (t->flag & T_PROP_EDIT)) {
    if (t->data_type == &TransConvertType_Object) {
      /* Selected objects are already first, no need to presort. */
    }
    else {
      sort_trans_data_selected_first(t);
    }

    if (ELEM(t->data_type,
             &TransConvertType_Mesh,
             &TransConvertType_MeshSkin,
             &TransConvertType_MeshVertCData))
    {
      if (t->flag & T_PROP_CONNECTED) {
        /* Already calculated by #transform_convert_mesh_connectivity_distance. */
      }
      else {
        set_prop_dist(t, false);
      }
    }
    else {
      set_prop_dist(t, true);
    }

    sort_trans_data_dist(t);
  }
}

/* For multi object editing. */
static void init_TransDataContainers(TransInfo *t, Object *obact, Span<Object *> objects)
{
  if (!ELEM(t->data_type,
            &TransConvertType_Mesh,
            &TransConvertType_MeshEdge,
            &TransConvertType_MeshSkin,
            &TransConvertType_MeshVertCData))
  {
    /* Does not support Multi object editing. */
    return;
  }

  const eObjectMode object_mode = obact ? obact->mode : OB_MODE_OBJECT;

  if (object_mode & OB_MODE_EDIT) {
    if (t->data_container) {
      MEM_delete(t->data_container);
    }

    Vector<Object *> local_objects;
    if (objects.is_empty()) {
      ObjectsInModeParams params = {0};
      params.object_mode = object_mode;
      params.no_dup_data = true;
      local_objects = BKE_view_layer_array_from_objects_in_mode_params(
          *t->bmain,
          t->scene,
          t->view_layer,
          static_cast<const View3D *>((t->spacetype == SPACE_VIEW3D) ? t->view : nullptr),
          &params);
      objects = local_objects;
    }

    t->data_container = MEM_new_array_zeroed<TransDataContainer>(objects.size(), __func__);
    t->data_container_len = objects.size();

    for (int i = 0; i < objects.size(); i++) {
      TransDataContainer *tc = &t->data_container[i];
      if (!(t->flag & T_NO_MIRROR) && (objects[i]->type == OB_MESH)) {
        tc->use_mirror_axis_x = ((id_cast<Mesh *>(objects[i]->data))->symmetry & ME_SYMMETRY_X) !=
                                0;
        tc->use_mirror_axis_y = ((id_cast<Mesh *>(objects[i]->data))->symmetry & ME_SYMMETRY_Y) !=
                                0;
        tc->use_mirror_axis_z = ((id_cast<Mesh *>(objects[i]->data))->symmetry & ME_SYMMETRY_Z) !=
                                0;
      }

      if (object_mode & OB_MODE_EDIT) {
        tc->obedit = objects[i];
        if ((t->flag & T_2D_EDIT) == 0) {
          tc->use_local_mat = true;
        }
      }

      if (tc->use_local_mat) {
        BLI_assert((t->flag & T_2D_EDIT) == 0);
        copy_m4_m4(tc->mat, objects[i]->object_to_world().ptr());
        copy_m3_m4(tc->mat3, tc->mat);
        /* For non-invertible scale matrices, #invert_m4_m4_fallback()
         * can still provide a valid pivot. */
        invert_m4_m4_fallback(tc->imat, tc->mat);
        invert_m3_m3(tc->imat3, tc->mat3);
        normalize_m3_m3(tc->mat3_unit, tc->mat3);
      }
      /* Otherwise leave as zero. */
    }
  }
}

static TransConvertTypeInfo *convert_type_get(const TransInfo *t)
{
  /* If tests must match recalc_data for correct updates. */
  if (t->options & CTX_CURSOR) {
    if (t->spacetype == SPACE_VIEW3D) {
      return &TransConvertType_Cursor3D;
    }
    return nullptr;
  }
  if (t->options & CTX_TEXTURE_SPACE) {
    return &TransConvertType_ObjectTexSpace;
  }
  if (t->options & CTX_EDGE_DATA) {
    return &TransConvertType_MeshEdge;
  }
  if (t->spacetype != SPACE_VIEW3D) {
    return nullptr;
  }
  if (t->obedit_type != -1) {
    if (t->obedit_type == OB_MESH) {
      if (t->mode == TFM_SKIN_RESIZE) {
        return &TransConvertType_MeshSkin;
      }
      if (ELEM(t->mode, TFM_BWEIGHT, TFM_VERT_CREASE)) {
        return &TransConvertType_MeshVertCData;
      }
      return &TransConvertType_Mesh;
    }
    return nullptr;
  }
  ViewLayer *view_layer = t->view_layer;
  BKE_view_layer_synced_ensure(*t->bmain, t->scene, t->view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  if (ob && ob->mode != OB_MODE_OBJECT) {
    return nullptr;
  }
  return &TransConvertType_Object;
}

void create_trans_data(bContext *C, TransInfo *t)
{
  t->data_len_all = -1;

  t->data_type = convert_type_get(t);
  if (t->data_type == nullptr) {
    BLI_assert(t->data_len_all == -1);
    t->data_len_all = 0;
    return;
  }

  t->flag |= eTFlag(t->data_type->flags);

  BKE_view_layer_synced_ensure(*t->bmain, t->scene, t->view_layer);
  Object *ob = BKE_view_layer_active_object_get(t->view_layer);
  init_TransDataContainers(t, ob, {});

  if (t->data_type == &TransConvertType_Object) {
    t->options |= CTX_OBJECT;

    /* Needed for correct Object.obmat after duplication, see: #62135. */
    BKE_scene_graph_evaluated_ensure(t->depsgraph, CTX_data_main(t->context));

    if ((t->settings->transform_flag & SCE_XFORM_DATA_ORIGIN) != 0) {
      t->options |= CTX_OBMODE_XFORM_OBDATA;
    }
    if ((t->settings->transform_flag & SCE_XFORM_SKIP_CHILDREN) != 0) {
      t->options |= CTX_OBMODE_XFORM_SKIP_CHILDREN;
    }
    TransConvertType_Object.create_trans_data(C, t);
    /* Check if we're transforming the camera from the camera. */
    if ((t->spacetype == SPACE_VIEW3D) && (t->region->regiontype == RGN_TYPE_WINDOW)) {
      View3D *v3d = static_cast<View3D *>(t->view);
      RegionView3D *rv3d = static_cast<RegionView3D *>(t->region->regiondata);
      if ((rv3d->persp == RV3D_CAMOB) && v3d->camera) {
        /* We could have a flag to easily check an object is being transformed. */
        if (v3d->camera->id.tag & ID_TAG_DOIT) {
          t->options |= CTX_CAMERA;
        }
      }
      else if (v3d->ob_center && v3d->ob_center->id.tag & ID_TAG_DOIT) {
        t->options |= CTX_CAMERA;
      }
    }
  }
  else {
    t->data_type->create_trans_data(C, t);
  }

  countAndCleanTransDataContainer(t);

  init_proportional_edit(t);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Data Recalc/Flush
 * \{ */

void transform_convert_clip_mirror_modifier_apply(TransDataContainer *tc)
{
  Object *ob = tc->obedit;
  ModifierData *md = ob->modifiers.first();
  tc->has_mirror_clipping = false;

  for (; md; md = md->next) {
    if ((md->type == eModifierType_Mirror) && (md->mode & eModifierMode_Realtime)) {
      MirrorModifierData *mmd = reinterpret_cast<MirrorModifierData *>(md);

      if ((mmd->flag & MOD_MIR_CLIPPING) == 0) {
        continue;
      }

      if ((mmd->flag & (MOD_MIR_AXIS_X | MOD_MIR_AXIS_Y | MOD_MIR_AXIS_Z)) == 0) {
        continue;
      }

      float mtx[4][4], imtx[4][4];

      if (mmd->mirror_ob) {
        float obinv[4][4];

        invert_m4_m4(obinv, mmd->mirror_ob->object_to_world().ptr());
        mul_m4_m4m4(mtx, obinv, ob->object_to_world().ptr());
        invert_m4_m4(imtx, mtx);
      }

      TransData *td = tc->data;
      for (int i = 0; i < tc->data_len; i++, td++) {
        float loc[3], iloc[3];

        if (td->loc == nullptr) {
          break;
        }

        if (td->flag & TD_SKIP) {
          continue;
        }

        copy_v3_v3(loc, td->loc);
        copy_v3_v3(iloc, td->iloc);

        if (mmd->mirror_ob) {
          mul_m4_v3(mtx, loc);
          mul_m4_v3(mtx, iloc);
        }

        bool is_clipping = false;
        if (mmd->flag & MOD_MIR_AXIS_X) {
          if (fabsf(iloc[0]) <= mmd->tolerance || loc[0] * iloc[0] < 0.0f) {
            loc[0] = 0.0f;
            is_clipping = true;
          }
        }

        if (mmd->flag & MOD_MIR_AXIS_Y) {
          if (fabsf(iloc[1]) <= mmd->tolerance || loc[1] * iloc[1] < 0.0f) {
            loc[1] = 0.0f;
            is_clipping = true;
          }
        }
        if (mmd->flag & MOD_MIR_AXIS_Z) {
          if (fabsf(iloc[2]) <= mmd->tolerance || loc[2] * iloc[2] < 0.0f) {
            loc[2] = 0.0f;
            is_clipping = true;
          }
        }

        if (is_clipping) {
          if (mmd->mirror_ob) {
            mul_m4_v3(imtx, loc);
          }
          copy_v3_v3(td->loc, loc);
          tc->has_mirror_clipping = true;
        }
      }
    }
  }
}

void recalc_data(TransInfo *t)
{
  if (!t->data_type || !t->data_type->recalc_data) {
    return;
  }
  t->data_type->recalc_data(t);
}

/** \} */

}  // namespace blender::ed::transform
