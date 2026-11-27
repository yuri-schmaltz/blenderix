/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <optional>

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLT_translation.hh"

#include "BLI_array.hh"
#include "BLI_array_utils_c.hh"
#include "BLI_bit_vector.hh"
#include "BLI_bounds.hh"
#include "BLI_listbase.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_c.hh"
#include "BLI_math_rotation_c.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_c.hh"
#include "BLI_string_utf8.hh"
#include "BLI_utildefines.hh"
#include "BLI_vector.hh"

#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"
#include "BKE_library.hh"
#include "BKE_mesh_types.hh"
#include "BKE_object.hh"
#include "BKE_object_deform.h"
#include "BKE_object_types.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_object_vgroup.hh"
#include "ED_screen.hh"
#include "ED_transform.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "view3d_intern.hh" /* own include */

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Transform Panel Types
 * \{ */

enum {
  B_TRANSFORM_PANEL_MEDIAN = 1008,
  B_TRANSFORM_PANEL_OBJECT_DIMS = 1009,
  /**
   * Dimensions of selected elements.
   * Needed because scaling geometry requires normals to be re-calculated.
   */
  B_TRANSFORM_PANEL_ELEM_DIMS = 1010,
};

/* All must start w/ location */

struct TransformMedian_Generic {
  float location[3];
};

struct TransformMedian_Mesh {
  float location[3], bv_weight, v_crease, be_weight, skin[2], e_crease;
};

union TransformMedian {
  TransformMedian_Generic generic;
  TransformMedian_Mesh mesh;
};

/**
 * Coordinates of the selected elements when a dimensions drag began.
 *
 * Scaling in place would seem simpler, however a dimension set to zero loses the size with no
 * way to scale back up. Only allocated once a dimension is edited.
 */
struct TransformCoordsOrig {
  /* For the drag as a whole. */

  /** Selected coordinates, in apply-loop visit order. */
  Array<float3> coords;
  /** Size & pivot when the drag began, every scale is relative to these. */
  float3 dims;
  float3 pivot;

  /* For the update in progress. */

  /** Advanced by #apply, reset at the start of every update. */
  int coords_index = 0;
  /** False until the first update has filled `coords`. */
  bool is_stored = false;

  /**
   * Scale `co` from its original, storing it on the first update and reading it back on
   * later ones.
   *
   * Callers must visit coordinates in the same order every update.
   */
  void apply(const float4x4 &mat, float3 &co);
};

/* temporary struct for storing transform properties */

struct TransformProperties {
  float ob_obmat_orig[4][4];
  float ob_dims_orig[3];
  float ob_scale_orig[3];
  float ob_dims[3];
  Vector<float> vertex_weights;

  /* Floats only (treated as an array). */
  TransformMedian ve_median, median;

  /* Selection size, in the same space as `ve_median`. */
  float3 ve_dims;
  /**
   * The size the panel last drew, editing a dimension button is the only way `ve_dims` differs.
   *
   * Comparing the button against a freshly measured size would seem simpler, however the
   * measurement isn't stable: the panel may not redraw between two updates of a median drag
   * and translating coordinates shifts the size by rounding, which would read as a dimension
   * edit and scale the selection about the pivot instead of moving it.
   */
  float3 ve_dims_prev;
  /* Only set while dragging a dimension. */
  std::optional<TransformCoordsOrig> coords_orig;

  bool tag_for_update;
};

#define TRANSFORM_MEDIAN_ARRAY_LEN (sizeof(TransformMedian) / sizeof(float))

/** \} */

static TransformProperties *v3d_transform_props_ensure(View3D *v3d);

/* -------------------------------------------------------------------- */
/** \name Panel Interaction
 *
 * Active while dragging for edit-mesh updates, freeing stored coordinates on exit.
 * \{ */

/** The view may be gone when the interaction ends, hence the null check. */
static void coords_orig_free(const bContext *C)
{
  if (View3D *v3d = CTX_wm_view3d(C)) {
    v3d_transform_props_ensure(v3d)->coords_orig.reset();
  }
}

static void *transform_panel_interaction_begin_fn(bContext *C,
                                                  const ui::BlockInteraction_Params *params,
                                                  void *arg1)
{
  const int retval_median = B_TRANSFORM_PANEL_MEDIAN;
  const int retval_elem_dims = B_TRANSFORM_PANEL_ELEM_DIMS;
  const bool is_median = BLI_array_findindex(params->unique_retval_ids,
                                             params->unique_retval_ids_len,
                                             &retval_median) != -1;
  const bool is_elem_dims = BLI_array_findindex(params->unique_retval_ids,
                                                params->unique_retval_ids_len,
                                                &retval_elem_dims) != -1;
  if (!(is_median || is_elem_dims)) {
    return nullptr;
  }

  /* Paranoid, an unfinished drag would scale from stale coordinates. */
  coords_orig_free(C);

  Object *ob = static_cast<Object *>(arg1);
  if (ob->type != OB_MESH) {
    return nullptr;
  }
  BMesh *bm = BKE_editmesh_bmesh_get_for_write(ob);
  if (bm == nullptr) {
    return nullptr;
  }

  int verts_mask_count = 0;
  BMIter iter;
  BMVert *eve;
  int i;

  BitVector<> verts_mask(bm->totvert);
  BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
    if (!BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
      continue;
    }
    verts_mask[i].set();
    verts_mask_count += 1;
  }

  BMPartialUpdate_Params update_params{};
  update_params.do_tessellate = true;
  update_params.do_normals = true;
  /* Translation keeps the normals of faces inside the selection,
   * a non-uniform scale doesn't, so every face touching the selection is needed. */
  BMPartialUpdate *bmpinfo;
  if (is_elem_dims) {
    bmpinfo = BM_mesh_partial_create_from_verts(*bm, update_params, verts_mask, verts_mask_count);
  }
  else {
    /* The mask is looked up by vertex index. */
    BM_mesh_elem_index_ensure(bm, BM_VERT);
    bmpinfo = BM_mesh_partial_create_from_verts_group_single(
        *bm, update_params, verts_mask, verts_mask_count);
  }

  return bmpinfo;
}

static void transform_panel_interaction_end_fn(bContext *C,
                                               const ui::BlockInteraction_Params * /*params*/,
                                               void * /*arg1*/,
                                               void *user_data)
{
  coords_orig_free(C);

  BMPartialUpdate *bmpinfo = static_cast<BMPartialUpdate *>(user_data);
  if (bmpinfo == nullptr) {
    return;
  }
  BM_mesh_partial_destroy(bmpinfo);
}

static void transform_panel_interaction_update_fn(bContext *C,
                                                  const ui::BlockInteraction_Params * /*params*/,
                                                  void *arg1,
                                                  void *user_data)
{
  BMPartialUpdate *bmpinfo = static_cast<BMPartialUpdate *>(user_data);
  if (bmpinfo == nullptr) {
    return;
  }

  View3D *v3d = CTX_wm_view3d(C);
  TransformProperties *tfp = v3d_transform_props_ensure(v3d);
  if (tfp->tag_for_update == false) {
    return;
  }
  tfp->tag_for_update = false;

  Object *ob = static_cast<Object *>(arg1);
  EditMesh *em = BKE_editmesh_from_object(ob);

  BKE_editmesh_looptris_and_normals_calc_with_partial(em, BKE_editmesh_bmesh_ensure(ob), bmpinfo);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Median Utilities
 * \{ */

/** Bounds before any point has been added, see #median_bounds_add. */
static Bounds<float3> median_bounds_init()
{
  return {
      float3(FLT_MAX),
      float3(-FLT_MAX),
  };
}

/**
 * Return true once a point has been added, an inverted range never had one.
 *
 * Unlike #Bounds::is_empty which is also true for a flat selection (any axis with zero size).
 */
static bool median_bounds_is_set(const Bounds<float3> &bounds)
{
  return bounds.min.x <= bounds.max.x;
}

/**
 * Accumulate `co` into `location` and expand `bounds` to include it.
 *
 * A naive approach would be to accumulate the bounds in local space then transform them,
 * however transforming a bounding box grows it when the object is rotated,
 * so transform each point as it's added.
 *
 * \param matrix: Maps `co` into the space the bounds are measured in,
 * null to keep them in object space.
 * \param co: The point in object space (grease pencil layers must convert first).
 */
static void median_bounds_add(float location[3],
                              Bounds<float3> &bounds,
                              const float4x4 *matrix,
                              const float co[3])
{
  add_v3_v3(location, co);

  float3 co_test(co);
  if (matrix) {
    co_test = math::transform_point(*matrix, co_test);
  }
  math::min_max(co_test, bounds.min, bounds.max);
}

/** First update, `co` hasn't been scaled yet so store it. */
static void dims_apply_store(TransformCoordsOrig &coords_orig, const float4x4 &mat, float3 &co)
{
  const float3 co_orig = co;
  coords_orig.coords[coords_orig.coords_index++] = co_orig;
  co = math::transform_point(mat, co_orig);
}

/** Later updates, `co` is what the last update scaled it to, so replace it. */
static void dims_apply_restore(TransformCoordsOrig &coords_orig, const float4x4 &mat, float3 &co)
{
  co = math::transform_point(mat, coords_orig.coords[coords_orig.coords_index++]);
}

void TransformCoordsOrig::apply(const float4x4 &mat, float3 &co)
{
  this->is_stored ? dims_apply_restore(*this, mat, co) : dims_apply_store(*this, mat, co);
}

/**
 * Calculate the matrix which scales the selection from `dims_orig` to `dims_new`.
 *
 * The scale is about the transform pivot (see #calc_pivot_pos) to match an interactive scale.
 * A size only says how big the selection becomes, the pivot decides where it grows from.
 *
 * "Individual Origins" is the exception, #calc_pivot_pos resolves it to the median.
 * Scaling each island about its own center leaves the selection a size other than the one
 * entered, so a single pivot is required here.
 *
 * \param matrix: The matrix the dimensions were measured with, see #median_bounds_add.
 * The same matrix must be used, the scale is per-axis in that space.
 * \return the scale in local space, ready to apply to element coordinates.
 * None when the measured space can't be mapped back into object space.
 */
static std::optional<float4x4> median_dims_scale_matrix_calc(const Object &ob,
                                                             const float4x4 *matrix,
                                                             const float3 &pivot_world,
                                                             const float3 &dims_orig,
                                                             const float3 &dims_new)
{
  float3 scale(1.0f);
  for (const int axis : IndexRange(3)) {
    const float scale_axis = dims_new[axis] / dims_orig[axis];
    if (!std::isfinite(scale_axis)) {
      /* Flat on this axis, no scale can give it a size.
       * #BKE_object_dimensions_set_ex discards a non-finite scale for the same reason. */
      continue;
    }
    scale[axis] = scale_axis;
  }

  const float4x4 object_to_measure = matrix ? *matrix : float4x4::identity();
  bool is_invertible;
  const float4x4 measure_to_object = math::invert(object_to_measure, is_invertible);
  if (!is_invertible) {
    /* The object has a zero scale on some axis (global space only), the zero matrix
     * #math::invert returns would collapse the selection onto the object origin. */
    return std::nullopt;
  }

  /* The pivot is always in global space, the dimensions may not be.
   * A pseudo inverse maps the pivot even when the object has a zero scaled axis,
   * it only positions the scale so a true inverse isn't needed. */
  const float3 pivot = math::transform_point(
      object_to_measure * math::pseudo_invert(ob.object_to_world()), pivot_world);

  /* Scale about the pivot in the measured space, wrapped to apply to object coordinates. */
  const float4x4 mat = math::from_loc_scale<float4x4>(pivot - (scale * pivot), scale);
  return measure_to_object * mat * object_to_measure;
}

/* Helper function to compute a median changed value,
 * when the value should be clamped in [0.0, 1.0].
 * Returns either 0.0, 1.0 (both can be applied directly), a positive scale factor
 * for scale down, or a negative one for scale up.
 */
static float compute_scale_factor(const float ve_median, const float median)
{
  if (ve_median <= 0.0f) {
    return 0.0f;
  }
  if (ve_median >= 1.0f) {
    return 1.0f;
  }

  /* Scale value to target median. */
  float median_new = ve_median;
  float median_orig = ve_median - median; /* Previous median value. */

  /* In case of floating point error. */
  CLAMP(median_orig, 0.0f, 1.0f);
  CLAMP(median_new, 0.0f, 1.0f);

  if (median_new <= median_orig) {
    /* Scale down. */
    return median_new / median_orig;
  }

  /* Scale up, negative to indicate it... */
  return -(1.0f - median_new) / (1.0f - median_orig);
}

/**
 * Apply helpers.
 * \note In case we only have one element,
 * copy directly the value instead of applying the diff or scale factor.
 * Avoids some glitches when going e.g. from 3 to 0.0001 (see #37327).
 */
static void apply_raw_diff(float *val, const int tot, const float ve_median, const float median)
{
  *val = (tot == 1) ? ve_median : (*val + median);
}

static void apply_raw_diff_v3(float val[3],
                              const int tot,
                              const float ve_median[3],
                              const float median[3])
{
  if (tot == 1) {
    copy_v3_v3(val, ve_median);
  }
  else {
    add_v3_v3(val, median);
  }
}

static void apply_scale_factor(
    float *val, const int tot, const float ve_median, const float median, const float sca)
{
  if (tot == 1 || ve_median == median) {
    *val = ve_median;
  }
  else {
    *val *= sca;
  }
}

static void apply_scale_factor_clamp(float *val,
                                     const int tot,
                                     const float ve_median,
                                     const float sca)
{
  if (tot == 1) {
    *val = ve_median;
    CLAMP(*val, 0.0f, 1.0f);
  }
  else if (ELEM(sca, 0.0f, 1.0f)) {
    *val = sca;
  }
  else {
    *val = (sca > 0.0f) ? (*val * sca) : (1.0f + ((1.0f - *val) * sca));
    CLAMP(*val, 0.0f, 1.0f);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Properties
 * \{ */

static TransformProperties *v3d_transform_props_ensure(View3D *v3d)
{
  if (v3d->runtime.properties_storage == nullptr) {
    TransformProperties *tfp = MEM_new<TransformProperties>("TransformProperties");
    /* Construct C++ structures in otherwise zero initialized struct. */
    new (tfp) TransformProperties();

    v3d->runtime.properties_storage = tfp;
    v3d->runtime.properties_storage_free = [](void *properties_storage) {
      MEM_delete(static_cast<TransformProperties *>(properties_storage));
    };
  }
  return static_cast<TransformProperties *>(v3d->runtime.properties_storage);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Vertex Buttons
 * \{ */

static void calc_median_bmesh(BMesh &bm,
                              const float4x4 *bounds_matrix,
                              TransformMedian_Mesh &median,
                              Bounds<float3> &r_select_bounds,
                              int &tot,
                              int &totedgedata)
{
  BMVert *eve;
  BMEdge *eed;
  BMIter iter;

  const int cd_vert_bweight_offset = CustomData_get_offset_named(
      &bm.vdata, CD_PROP_FLOAT, "bevel_weight_vert");
  const int cd_vert_crease_offset = CustomData_get_offset_named(
      &bm.vdata, CD_PROP_FLOAT, "crease_vert");
  const int cd_vert_skin_offset = CustomData_get_offset_named(
      &bm.vdata, CD_PROP_FLOAT2, "skin_modifier_radius");
  const int cd_edge_bweight_offset = CustomData_get_offset_named(
      &bm.edata, CD_PROP_FLOAT, "bevel_weight_edge");
  const int cd_edge_crease_offset = CustomData_get_offset_named(
      &bm.edata, CD_PROP_FLOAT, "crease_edge");

  if (bm.totvertsel) {
    BM_ITER_MESH (eve, &iter, &bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
        tot++;
        median_bounds_add(median.location, r_select_bounds, bounds_matrix, eve->co);

        if (cd_vert_bweight_offset != -1) {
          median.bv_weight += BM_ELEM_CD_GET_FLOAT(eve, cd_vert_bweight_offset);
        }

        if (cd_vert_crease_offset != -1) {
          median.v_crease += BM_ELEM_CD_GET_FLOAT(eve, cd_vert_crease_offset);
        }

        if (cd_vert_skin_offset != -1) {
          const float2 *radius = static_cast<const float2 *>(
              BM_ELEM_CD_GET_VOID_P(eve, cd_vert_skin_offset));
          add_v2_v2(median.skin, *radius);
        }
      }
    }
  }

  if ((cd_edge_bweight_offset != -1) || (cd_edge_crease_offset != -1)) {
    if (bm.totedgesel) {
      BM_ITER_MESH (eed, &iter, &bm, BM_EDGES_OF_MESH) {
        if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
          if (cd_edge_bweight_offset != -1) {
            median.be_weight += BM_ELEM_CD_GET_FLOAT(eed, cd_edge_bweight_offset);
          }

          if (cd_edge_crease_offset != -1) {
            median.e_crease += BM_ELEM_CD_GET_FLOAT(eed, cd_edge_crease_offset);
          }

          totedgedata++;
        }
      }
    }
  }
  else {
    totedgedata = bm.totedgesel;
  }
}

static void apply_median_to_bmesh(BMesh &bm,
                                  const TransformMedian_Mesh &median,
                                  const TransformMedian_Mesh &ve_median,
                                  const int tot,
                                  const bool apply_vcos,
                                  const FunctionRef<void(float co[3])> apply_coord_fn)
{
  BMIter iter;
  BMVert *eve;
  BMEdge *eed;

  int cd_vert_bweight_offset = -1;
  int cd_vert_crease_offset = -1;
  int cd_vert_skin_offset = -1;
  int cd_edge_bweight_offset = -1;
  int cd_edge_crease_offset = -1;

  float scale_bv_weight = 1.0f;
  float scale_v_crease = 1.0f;
  float scale_skin[2] = {1.0f, 1.0f};
  float scale_be_weight = 1.0f;
  float scale_e_crease = 1.0f;

  /* Vertices */

  if (apply_vcos || median.bv_weight || median.v_crease || median.skin[0] || median.skin[1]) {
    if (median.bv_weight) {
      if (!CustomData_has_layer_named(&bm.vdata, CD_PROP_FLOAT, "bevel_weight_vert")) {
        BM_data_layer_add_named(&bm, &bm.vdata, CD_PROP_FLOAT, "bevel_weight_vert");
      }
      cd_vert_bweight_offset = CustomData_get_offset_named(
          &bm.vdata, CD_PROP_FLOAT, "bevel_weight_vert");
      BLI_assert(cd_vert_bweight_offset != -1);

      scale_bv_weight = compute_scale_factor(ve_median.bv_weight, median.bv_weight);
    }

    if (median.v_crease) {
      if (!CustomData_has_layer_named(&bm.vdata, CD_PROP_FLOAT, "crease_vert")) {
        BM_data_layer_add_named(&bm, &bm.vdata, CD_PROP_FLOAT, "crease_vert");
      }
      cd_vert_crease_offset = CustomData_get_offset_named(&bm.vdata, CD_PROP_FLOAT, "crease_vert");
      BLI_assert(cd_vert_crease_offset != -1);

      scale_v_crease = compute_scale_factor(ve_median.v_crease, median.v_crease);
    }

    for (int i = 0; i < 2; i++) {
      if (median.skin[i]) {
        cd_vert_skin_offset = CustomData_get_offset_named(
            &bm.vdata, CD_PROP_FLOAT2, "skin_modifier_radius");
        BLI_assert(cd_vert_skin_offset != -1);

        if (ve_median.skin[i] != median.skin[i]) {
          scale_skin[i] = ve_median.skin[i] / (ve_median.skin[i] - median.skin[i]);
        }
      }
    }

    BM_ITER_MESH (eve, &iter, &bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
        apply_coord_fn(eve->co);

        if (cd_vert_bweight_offset != -1) {
          float *b_weight = static_cast<float *>(
              BM_ELEM_CD_GET_VOID_P(eve, cd_vert_bweight_offset));
          apply_scale_factor_clamp(b_weight, tot, ve_median.bv_weight, scale_bv_weight);
        }

        if (cd_vert_crease_offset != -1) {
          float *crease = static_cast<float *>(BM_ELEM_CD_GET_VOID_P(eve, cd_vert_crease_offset));
          apply_scale_factor_clamp(crease, tot, ve_median.v_crease, scale_v_crease);
        }

        if (cd_vert_skin_offset != -1) {
          float2 *radius = static_cast<float2 *>(BM_ELEM_CD_GET_VOID_P(eve, cd_vert_skin_offset));

          /* That one is not clamped to [0.0, 1.0]. */
          for (int i = 0; i < 2; i++) {
            if (median.skin[i] != 0.0f) {
              apply_scale_factor(
                  &(*radius)[i], tot, ve_median.skin[i], median.skin[i], scale_skin[i]);
            }
          }
        }
      }
    }
  }

  /* Edges */

  if (median.be_weight || median.e_crease) {
    if (median.be_weight) {
      if (!CustomData_has_layer_named(&bm.edata, CD_PROP_FLOAT, "bevel_weight_edge")) {
        BM_data_layer_add_named(&bm, &bm.edata, CD_PROP_FLOAT, "bevel_weight_edge");
      }
      cd_edge_bweight_offset = CustomData_get_offset_named(
          &bm.edata, CD_PROP_FLOAT, "bevel_weight_edge");
      BLI_assert(cd_edge_bweight_offset != -1);

      scale_be_weight = compute_scale_factor(ve_median.be_weight, median.be_weight);
    }

    if (median.e_crease) {
      if (!CustomData_has_layer_named(&bm.edata, CD_PROP_FLOAT, "crease_edge")) {
        BM_data_layer_add_named(&bm, &bm.edata, CD_PROP_FLOAT, "crease_edge");
      }
      cd_edge_crease_offset = CustomData_get_offset_named(&bm.edata, CD_PROP_FLOAT, "crease_edge");
      BLI_assert(cd_edge_crease_offset != -1);

      scale_e_crease = compute_scale_factor(ve_median.e_crease, median.e_crease);
    }

    BM_ITER_MESH (eed, &iter, &bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
        if (median.be_weight != 0.0f) {
          float *b_weight = static_cast<float *>(
              BM_ELEM_CD_GET_VOID_P(eed, cd_edge_bweight_offset));
          apply_scale_factor_clamp(b_weight, tot, ve_median.be_weight, scale_be_weight);
        }

        if (median.e_crease != 0.0f) {
          float *crease = static_cast<float *>(BM_ELEM_CD_GET_VOID_P(eed, cd_edge_crease_offset));
          apply_scale_factor_clamp(crease, tot, ve_median.e_crease, scale_e_crease);
        }
      }
    }
  }
}

static void v3d_editvertex_buts(
    const bContext *C, ui::Layout *layout, View3D *v3d, Object *ob, float lim)
{
  ui::Block *block = (layout) ? layout->absolute().block() : nullptr;
  TransformProperties *tfp = v3d_transform_props_ensure(v3d);
  TransformMedian median_basis, ve_median_basis;
  int tot = 0, totedgedata = 0;
  bool has_meshdata = false;
  bool has_skinradius = false;

  std::fill_n(reinterpret_cast<float *>(&median_basis), TRANSFORM_MEDIAN_ARRAY_LEN, 0.0f);

  /* Measure the bounds in the space the dimensions are shown in, null for local. */
  const float4x4 *bounds_matrix = (v3d->flag & V3D_GLOBAL_STATS) ? &ob->object_to_world() :
                                                                   nullptr;
  Bounds<float3> select_bounds = median_bounds_init();

  if (ob->type == OB_MESH) {
    TransformMedian_Mesh &median = median_basis.mesh;
    Mesh *mesh = id_cast<Mesh *>(ob->data);
    BMesh *bm = BKE_editmesh_bmesh_ensure(mesh);
    calc_median_bmesh(*bm, bounds_matrix, median, select_bounds, tot, totedgedata);

    has_meshdata = (tot || totedgedata);
    has_skinradius = CustomData_has_layer_named(
        &bm->vdata, CD_PROP_FLOAT2, "skin_modifier_radius");
  }

  if (tot == 0) {
    uiDefBut(block,
             ui::ButtonType::Label,
             IFACE_("Nothing selected"),
             0,
             130,
             200,
             20,
             nullptr,
             0,
             0,
             "");
    return;
  }

  /* Location, X/Y/Z */
  mul_v3_fl(median_basis.generic.location, 1.0f / float(tot));
  if (v3d->flag & V3D_GLOBAL_STATS) {
    mul_m4_v3(ob->object_to_world().ptr(), median_basis.generic.location);
  }

  if (has_meshdata) {
    TransformMedian_Mesh *median = &median_basis.mesh;
    if (totedgedata) {
      median->e_crease /= float(totedgedata);
      median->be_weight /= float(totedgedata);
    }
    if (tot) {
      median->bv_weight /= float(tot);
      median->v_crease /= float(tot);
      if (has_skinradius) {
        median->skin[0] /= float(tot);
        median->skin[1] /= float(tot);
      }
    }
  }

  if (block) { /* buttons */
    ui::Button *but;
    int yi = 200;
    const int butw = 200;
    const int buth = 20 * UI_SCALE_FAC;
    const int but_margin = 2;
    const char *c;

    memcpy(&tfp->ve_median, &median_basis, sizeof(tfp->ve_median));
    tfp->ve_dims = select_bounds.size();
    tfp->ve_dims_prev = tfp->ve_dims;

    block_align_begin(block);
    c = (tot == 1) ? IFACE_("Vertex:") : IFACE_("Median:");
    uiDefBut(block, ui::ButtonType::Label, c, 0, yi -= buth, butw, buth, nullptr, 0, 0, "");

    block_align_begin(block);

    /* Should be no need to translate these. */
    but = uiDefButV(block,
                    ui::ButtonType::Num,
                    IFACE_("X:"),
                    0,
                    yi -= buth,
                    butw,
                    buth,
                    &tfp->ve_median.generic.location[0],
                    -lim,
                    lim,
                    "");
    button_retval_set(but, B_TRANSFORM_PANEL_MEDIAN);
    button_number_step_size_set(but, 10);
    button_number_precision_set(but, RNA_TRANSLATION_PREC_DEFAULT);
    button_unit_type_set(but, PROP_UNIT_LENGTH);
    but = uiDefButV(block,
                    ui::ButtonType::Num,
                    IFACE_("Y:"),
                    0,
                    yi -= buth,
                    butw,
                    buth,
                    &tfp->ve_median.generic.location[1],
                    -lim,
                    lim,
                    "");
    button_retval_set(but, B_TRANSFORM_PANEL_MEDIAN);
    button_number_step_size_set(but, 10);
    button_number_precision_set(but, RNA_TRANSLATION_PREC_DEFAULT);
    button_unit_type_set(but, PROP_UNIT_LENGTH);
    but = uiDefButV(block,
                    ui::ButtonType::Num,
                    IFACE_("Z:"),
                    0,
                    yi -= buth,
                    butw,
                    buth,
                    &tfp->ve_median.generic.location[2],
                    -lim,
                    lim,
                    "");
    button_retval_set(but, B_TRANSFORM_PANEL_MEDIAN);
    button_number_step_size_set(but, 10);
    button_number_precision_set(but, RNA_TRANSLATION_PREC_DEFAULT);
    button_unit_type_set(but, PROP_UNIT_LENGTH);

    block_align_end(block);

    /* A single point always has a zero size, the buttons would only show zeros. */
    if (tot > 1) {
      const StringRef dims_tip = TIP_(
          "Size of the selection, editing scales it about the transform pivot point");
      uiDefBut(block,
               ui::ButtonType::Label,
               IFACE_("Dimensions:"),
               0,
               yi -= buth + but_margin,
               butw,
               buth,
               nullptr,
               0,
               0,
               "");

      block_align_begin(block);
      for (int i = 0; i < 3; i++) {
        const char text[3] = {char('X' + i), ':', '\0'};
        but = uiDefButV(block,
                        ui::ButtonType::Num,
                        text,
                        0,
                        yi -= buth,
                        butw,
                        buth,
                        &tfp->ve_dims[i],
                        0.0f,
                        lim,
                        dims_tip);
        button_retval_set(but, B_TRANSFORM_PANEL_ELEM_DIMS);
        button_number_step_size_set(but, 10);
        button_number_precision_set(but, 3);
        button_unit_type_set(but, PROP_UNIT_LENGTH);
      }
      block_align_end(block);
    }

    block_align_begin(block);
    but = uiDefButBit(block,
                      ui::ButtonType::Toggle,
                      V3D_GLOBAL_STATS,
                      IFACE_("Global"),
                      0,
                      yi -= buth + but_margin,
                      100,
                      buth,
                      &v3d->flag,
                      0,
                      0,
                      TIP_("Displays global values"));
    ui::button_func_set(but, [](bContext &C) { ED_area_tag_redraw(CTX_wm_area(&C)); });
    but = uiDefButBit(block,
                      ui::ButtonType::ToggleN,
                      V3D_GLOBAL_STATS,
                      IFACE_("Local"),
                      100,
                      yi,
                      100,
                      buth,
                      &v3d->flag,
                      0,
                      0,
                      TIP_("Displays local values"));
    ui::button_func_set(but, [](bContext &C) { ED_area_tag_redraw(CTX_wm_area(&C)); });
    block_align_end(block);

    /* Meshes... */
    if (has_meshdata) {
      TransformMedian_Mesh *ve_median = &tfp->ve_median.mesh;
      if (tot) {
        uiDefBut(block,
                 ui::ButtonType::Label,
                 tot == 1 ? IFACE_("Vertex Data:") : IFACE_("Vertices Data:"),
                 0,
                 yi -= buth + but_margin,
                 butw,
                 buth,
                 nullptr,
                 0.0,
                 0.0,
                 "");
        /* customdata layer added on demand */
        but = uiDefButV(block,
                        ui::ButtonType::Num,
                        tot == 1 ? IFACE_("Bevel Weight:") : IFACE_("Mean Bevel Weight:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->bv_weight,
                        0.0,
                        1.0,
                        TIP_("Vertex weight used by Bevel modifier"));
        button_retval_set(but, B_TRANSFORM_PANEL_MEDIAN);
        button_number_step_size_set(but, 1);
        button_number_precision_set(but, 2);
        /* customdata layer added on demand */
        but = uiDefButV(block,
                        ui::ButtonType::Num,
                        tot == 1 ? IFACE_("Vertex Crease:") : IFACE_("Mean Vertex Crease:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->v_crease,
                        0.0,
                        1.0,
                        TIP_("Weight used by the Subdivision Surface modifier"));
        button_retval_set(but, B_TRANSFORM_PANEL_MEDIAN);
        button_number_step_size_set(but, 1);
        button_number_precision_set(but, 2);
      }
      if (has_skinradius) {
        block_align_begin(block);
        but = uiDefButV(block,
                        ui::ButtonType::Num,
                        tot == 1 ? IFACE_("Radius X:") : IFACE_("Mean Radius X:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->skin[0],
                        0.0,
                        100.0,
                        TIP_("X radius used by Skin modifier"));
        button_retval_set(but, B_TRANSFORM_PANEL_MEDIAN);
        button_number_step_size_set(but, 1);
        button_number_precision_set(but, 3);
        but = uiDefButV(block,
                        ui::ButtonType::Num,
                        tot == 1 ? IFACE_("Radius Y:") : IFACE_("Mean Radius Y:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->skin[1],
                        0.0,
                        100.0,
                        TIP_("Y radius used by Skin modifier"));
        button_retval_set(but, B_TRANSFORM_PANEL_MEDIAN);
        button_number_step_size_set(but, 1);
        button_number_precision_set(but, 3);
        block_align_end(block);
      }
      if (totedgedata) {
        uiDefBut(block,
                 ui::ButtonType::Label,
                 totedgedata == 1 ? IFACE_("Edge Data:") : IFACE_("Edges Data:"),
                 0,
                 yi -= buth + but_margin,
                 butw,
                 buth,
                 nullptr,
                 0.0,
                 0.0,
                 "");
        /* customdata layer added on demand */
        but = uiDefButV(block,
                        ui::ButtonType::Num,
                        totedgedata == 1 ? IFACE_("Bevel Weight:") : IFACE_("Mean Bevel Weight:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->be_weight,
                        0.0,
                        1.0,
                        TIP_("Edge weight used by Bevel modifier"));
        button_retval_set(but, B_TRANSFORM_PANEL_MEDIAN);
        button_number_step_size_set(but, 1);
        button_number_precision_set(but, 2);
        /* customdata layer added on demand */
        but = uiDefButV(block,
                        ui::ButtonType::Num,
                        totedgedata == 1 ? IFACE_("Crease:") : IFACE_("Mean Crease:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->e_crease,
                        0.0,
                        1.0,
                        TIP_("Weight used by the Subdivision Surface modifier"));
        button_retval_set(but, B_TRANSFORM_PANEL_MEDIAN);
        button_number_step_size_set(but, 1);
        button_number_precision_set(but, 2);
      }
    }

    block_align_end(block);

    ui::BlockInteraction_CallbackData callback_data{};
    callback_data.begin_fn = transform_panel_interaction_begin_fn;
    callback_data.end_fn = transform_panel_interaction_end_fn;
    callback_data.update_fn = transform_panel_interaction_update_fn;
    callback_data.arg1 = ob;
    block_interaction_set(block, &callback_data);
  }
  else {
    memcpy(&ve_median_basis, &tfp->ve_median, sizeof(tfp->ve_median));

    if (v3d->flag & V3D_GLOBAL_STATS) {
      invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());
      mul_m4_v3(ob->world_to_object().ptr(), median_basis.generic.location);
      mul_m4_v3(ob->world_to_object().ptr(), ve_median_basis.generic.location);
    }
    sub_vn_vnvn(reinterpret_cast<float *>(&median_basis),
                reinterpret_cast<float *>(&ve_median_basis),
                reinterpret_cast<float *>(&median_basis),
                TRANSFORM_MEDIAN_ARRAY_LEN);

    std::optional<float4x4> dims_mat;
    TransformCoordsOrig *coords_orig = tfp->coords_orig ? &*tfp->coords_orig : nullptr;
    if (median_bounds_is_set(select_bounds)) {
      const float3 dims_new = tfp->ve_dims;
      if ((coords_orig == nullptr) && (dims_new != tfp->ve_dims_prev)) {
        const Scene *scene = CTX_data_scene(C);
        float3 pivot;
        if (ed::transform::calc_pivot_pos(C, scene->toolsettings->transform_pivot_point, pivot)) {
          coords_orig = &tfp->coords_orig.emplace();
          coords_orig->dims = select_bounds.size();
          coords_orig->pivot = pivot;
          coords_orig->coords.reinitialize(tot);
        }
      }
      if (coords_orig) {
        dims_mat = median_dims_scale_matrix_calc(
            *ob, bounds_matrix, coords_orig->pivot, coords_orig->dims, dims_new);
        if (dims_mat) {
          coords_orig->coords_index = 0;
        }
      }
    }

    const bool apply_vcos = (tot == 1) || dims_mat.has_value() ||
                            (len_squared_v3(median_basis.generic.location) != 0.0f);

    auto apply_coord_fn = [&](float co[3]) {
      if (dims_mat) {
        coords_orig->apply(*dims_mat, *reinterpret_cast<float3 *>(co));
      }
      else if (apply_vcos) {
        apply_raw_diff_v3(
            co, tot, ve_median_basis.generic.location, median_basis.generic.location);
      }
    };

    if ((ob->type == OB_MESH) &&
        (apply_vcos || median_basis.mesh.bv_weight || median_basis.mesh.v_crease ||
         median_basis.mesh.skin[0] || median_basis.mesh.skin[1] || median_basis.mesh.be_weight ||
         median_basis.mesh.e_crease))
    {
      const TransformMedian_Mesh &median = median_basis.mesh;
      const TransformMedian_Mesh &ve_median = ve_median_basis.mesh;
      Mesh *mesh = id_cast<Mesh *>(ob->data);
      BMesh *bm = BKE_editmesh_bmesh_ensure(mesh);
      apply_median_to_bmesh(*bm, median, ve_median, tot, apply_vcos, apply_coord_fn);

      if (apply_vcos) {
        tfp->tag_for_update = true;
      }

      WM_main_add_notifier(NC_GEOM | ND_DATA, &mesh->id);
    }

    if (dims_mat) {
      BLI_assert(coords_orig->coords_index == coords_orig->coords.size());
      coords_orig->is_stored = true;
    }

    DEG_id_tag_update(ob->data, ID_RECALC_GEOMETRY);
  }
}

#undef TRANSFORM_MEDIAN_ARRAY_LEN

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Dimension Buttons
 * \{ */

static void v3d_object_dimension_buts(bContext *C, ui::Layout *layout, View3D *v3d, Object *ob)
{
  ui::Block *block = (layout) ? layout->block() : nullptr;
  ui::Layout *sub_layout = layout ? &layout->absolute(false) : nullptr;
  TransformProperties *tfp = v3d_transform_props_ensure(v3d);
  const bool is_editable = ID_IS_EDITABLE(&ob->id);

  if (block) {
    BLI_assert(C == nullptr);
    int yi = 200;
    const int butw = 200;
    const int buth = 20 * UI_SCALE_FAC;

    BKE_object_dimensions_eval_cached_get(ob, tfp->ob_dims);
    copy_v3_v3(tfp->ob_dims_orig, tfp->ob_dims);
    copy_v3_v3(tfp->ob_scale_orig, ob->scale);
    copy_m4_m4(tfp->ob_obmat_orig, ob->object_to_world().ptr());

    if (!is_editable && sub_layout) {
      sub_layout->enabled_set(false);
    }

    uiDefBut(block,
             ui::ButtonType::Label,
             IFACE_("Dimensions:"),
             0,
             yi -= buth,
             butw,
             buth,
             nullptr,
             0,
             0,
             "");
    block_align_begin(block);
    const float lim = FLT_MAX;
    for (int i = 0; i < 3; i++) {
      ui::Button *but;
      const char text[3] = {char('X' + i), ':', '\0'};
      but = uiDefButV(block,
                      ui::ButtonType::Num,
                      text,
                      0,
                      yi -= buth,
                      butw,
                      buth,
                      &(tfp->ob_dims[i]),
                      0.0f,
                      lim,
                      "");
      button_retval_set(but, B_TRANSFORM_PANEL_OBJECT_DIMS);
      button_number_step_size_set(but, 10);
      button_number_precision_set(but, 3);
      button_unit_type_set(but, PROP_UNIT_LENGTH);
      if (!is_editable) {
        button_disable(but, "Cannot edit this property from a linked data-block");
      }
    }
    block_align_end(block);
  }
  else { /* apply */
    int axis_mask = 0;
    for (int i = 0; i < 3; i++) {
      if (tfp->ob_dims[i] == tfp->ob_dims_orig[i]) {
        axis_mask |= (1 << i);
      }
    }
    BKE_object_dimensions_set_ex(
        ob, tfp->ob_dims, axis_mask, tfp->ob_scale_orig, tfp->ob_obmat_orig);

    PointerRNA obptr = RNA_id_pointer_create(&ob->id);
    PropertyRNA *prop = RNA_struct_find_property(&obptr, "scale");
    RNA_property_update(C, &obptr, prop);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Panel
 * \{ */

#define B_VGRP_PNL_EDIT_SINGLE 8 /* or greater */

static void do_view3d_vgroup_buttons(bContext *C, void * /*arg*/, int event)
{
  if (event < B_VGRP_PNL_EDIT_SINGLE) {
    /* not for me */
    return;
  }

  const Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  ed::object::vgroup_vert_active_mirror(ob, event - B_VGRP_PNL_EDIT_SINGLE);
  DEG_id_tag_update(ob->data, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
}

static bool view3d_panel_vgroup_poll(const bContext *C, PanelType * /*pt*/)
{
  const Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  if (ob && (BKE_object_is_in_editmode_vgroup(ob) || BKE_object_is_in_wpaint_select_vert(ob))) {
    MDeformVert *dvert_act = ED_mesh_active_dvert_get_only(ob);
    if (dvert_act) {
      return (dvert_act->totweight != 0);
    }
  }

  return false;
}

static void update_active_vertex_weight(bContext *C, void *arg1, void * /*arg2*/)
{
  View3D *v3d = CTX_wm_view3d(C);
  TransformProperties *tfp = v3d_transform_props_ensure(v3d);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  MDeformVert *dv = ED_mesh_active_dvert_get_only(ob);
  const int vertex_group_index = POINTER_AS_INT(arg1);
  MDeformWeight *dw = BKE_defvert_find_index(dv, vertex_group_index);
  dw->weight = tfp->vertex_weights[vertex_group_index];
}

static void view3d_panel_vgroup(const bContext *C, Panel *panel)
{
  ui::Block *block = panel->layout->absolute().block();
  const Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  View3D *v3d = CTX_wm_view3d(C);
  TransformProperties *tfp = v3d_transform_props_ensure(v3d);

  MDeformVert *dv;

  dv = ED_mesh_active_dvert_get_only(ob);

  if (dv && dv->totweight) {
    ToolSettings *ts = scene->toolsettings;

    wmOperatorType *ot;
    PointerRNA op_ptr;
    PointerRNA *but_ptr;

    ui::Layout *col, *bcol;
    ui::Layout *row;
    ui::Button *but;
    bDeformGroup *dg;
    uint i;
    int subset_count, vgroup_tot;
    const bool *vgroup_validmap;
    eVGroupSelect subset_type = eVGroupSelect(ts->vgroupsubset);
    int yco = 0;
    int lock_count = 0;

    block_func_handle_set(block, do_view3d_vgroup_buttons, nullptr);

    bcol = &panel->layout->column(true);
    row = &bcol->row(true); /* The filter button row */

    PointerRNA tools_ptr = RNA_pointer_create_discrete(nullptr, RNA_ToolSettings, ts);
    row->prop(&tools_ptr, "vertex_group_subset", ui::ITEM_R_EXPAND, std::nullopt, ICON_NONE);

    col = &bcol->column(true);

    vgroup_validmap = BKE_object_defgroup_subset_from_select_type(
        ob, subset_type, &vgroup_tot, &subset_count);
    const ListBaseT<bDeformGroup> *defbase = BKE_object_defgroup_list(ob);
    const int vgroup_num = defbase->count();
    tfp->vertex_weights.resize(vgroup_num);

    for (i = 0, dg = defbase->first(); dg; i++, dg = dg->next) {
      bool locked = (dg->flag & DG_LOCK_WEIGHT) != 0;
      if (vgroup_validmap[i]) {
        MDeformWeight *dw = BKE_defvert_find_index(dv, i);
        if (dw) {
          int x, xco = 0;
          int icon;
          ui::Layout &split = col->split(0.45, true);
          row = &split.row(true);

          /* The Weight Group Name */

          ot = WM_operatortype_find("OBJECT_OT_vertex_weight_set_active", true);
          but = uiDefButO_ptr(block,
                              ui::ButtonType::But,
                              ot,
                              wm::OpCallContext::ExecDefault,
                              dg->name,
                              xco,
                              yco,
                              (x = UI_UNIT_X * 5),
                              UI_UNIT_Y,
                              "");
          but_ptr = button_operator_ptr_ensure(but);
          RNA_int_set(but_ptr, "weight_group", i);
          button_drawflag_enable(but, ui::BUT_TEXT_RIGHT);
          if (BKE_object_defgroup_active_index_get(ob) != i + 1) {
            button_flag_enable(but, ui::BUT_INACTIVE);
          }
          xco += x;

          row = &split.row(true);
          row->enabled_set(!locked);

          /* The weight group value */
          /* To be reworked still */
          float &vertex_weight = tfp->vertex_weights[i];
          vertex_weight = dw->weight;
          but = uiDefButV(block,
                          ui::ButtonType::Num,
                          "",
                          xco,
                          yco,
                          (x = UI_UNIT_X * 4),
                          UI_UNIT_Y,
                          &vertex_weight,
                          0.0,
                          1.0,
                          "");
          button_retval_set(but, B_VGRP_PNL_EDIT_SINGLE + i);
          button_number_step_size_set(but, 1);
          button_number_precision_set(but, 3);
          button_drawflag_enable(but, ui::BUT_TEXT_LEFT);
          button_func_set(but, update_active_vertex_weight, POINTER_FROM_INT(i), nullptr);
          if (locked) {
            lock_count++;
          }
          xco += x;

          /* The weight group paste function */
          icon = (locked) ? ICON_BLANK1 : ICON_PASTEDOWN;
          op_ptr = row->op("OBJECT_OT_vertex_weight_paste",
                           "",
                           icon,
                           wm::OpCallContext::InvokeDefault,
                           UI_ITEM_NONE);
          RNA_int_set(&op_ptr, "weight_group", i);

          /* The weight entry delete function */
          icon = (locked) ? ICON_LOCKED : ICON_X;
          op_ptr = row->op("OBJECT_OT_vertex_weight_delete",
                           "",
                           icon,
                           wm::OpCallContext::InvokeDefault,
                           UI_ITEM_NONE);
          RNA_int_set(&op_ptr, "weight_group", i);

          yco -= UI_UNIT_Y;
        }
      }
    }
    MEM_delete(vgroup_validmap);

    yco -= 2;

    col = &panel->layout->column(true);
    row = &col->row(true);

    ot = WM_operatortype_find("OBJECT_OT_vertex_weight_normalize_active_vertex", true);
    but = uiDefButO_ptr(
        block,
        ui::ButtonType::But,
        ot,
        wm::OpCallContext::ExecDefault,
        IFACE_("Normalize"),
        0,
        yco,
        UI_UNIT_X * 5,
        UI_UNIT_Y,
        TIP_("Normalize weights of active vertex (if affected groups are unlocked)"));

    ot = WM_operatortype_find("OBJECT_OT_vertex_weight_copy", true);
    but = uiDefButO_ptr(
        block,
        ui::ButtonType::But,
        ot,
        wm::OpCallContext::ExecDefault,
        IFACE_("Copy"),
        UI_UNIT_X * 5,
        yco,
        UI_UNIT_X * 5,
        UI_UNIT_Y,
        TIP_("Copy active vertex to other selected vertices (if affected groups are unlocked)"));
    if (lock_count) {
      button_flag_enable(but, ui::BUT_DISABLED);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Buttons
 * \{ */

static void v3d_transform_butsR(ui::Layout &layout, PointerRNA *ptr)
{
  ui::Layout *split = &layout.split(0.8f, false);

  ui::Layout *colsub = &split->column(true);
  colsub->prop(ptr, "location", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  colsub = &split->column(true);
  colsub->emboss_set(ui::EmbossType::NoneOrStatus);
  colsub->label("", ICON_NONE);
  colsub->prop(
      ptr, "lock_location", ui::ITEM_R_TOGGLE | ui::ITEM_R_ICON_ONLY, "", ICON_DECORATE_UNLOCKED);

  split = &layout.split(0.8f, false);

  switch (RNA_enum_get(ptr, "rotation_mode")) {
    case ROT_MODE_QUAT: /* quaternion */
      colsub = &split->column(true);
      colsub->prop(ptr, "rotation_quaternion", UI_ITEM_NONE, IFACE_("Rotation"), ICON_NONE);
      colsub = &split->column(true);
      colsub->emboss_set(ui::EmbossType::NoneOrStatus);
      colsub->prop(ptr, "lock_rotations_4d", ui::ITEM_R_TOGGLE, IFACE_("4L"), ICON_NONE);
      if (RNA_boolean_get(ptr, "lock_rotations_4d")) {
        colsub->prop(ptr,
                     "lock_rotation_w",
                     ui::ITEM_R_TOGGLE | ui::ITEM_R_ICON_ONLY,
                     "",
                     ICON_DECORATE_UNLOCKED);
      }
      else {
        colsub->label("", ICON_NONE);
      }
      colsub->prop(ptr,
                   "lock_rotation",
                   ui::ITEM_R_TOGGLE | ui::ITEM_R_ICON_ONLY,
                   "",
                   ICON_DECORATE_UNLOCKED);
      break;
    case ROT_MODE_AXISANGLE: /* axis angle */
      colsub = &split->column(true);
      colsub->prop(ptr, "rotation_axis_angle", UI_ITEM_NONE, IFACE_("Rotation"), ICON_NONE);
      colsub = &split->column(true);
      colsub->emboss_set(ui::EmbossType::NoneOrStatus);
      colsub->prop(ptr, "lock_rotations_4d", ui::ITEM_R_TOGGLE, IFACE_("4L"), ICON_NONE);
      if (RNA_boolean_get(ptr, "lock_rotations_4d")) {
        colsub->prop(ptr,
                     "lock_rotation_w",
                     ui::ITEM_R_TOGGLE | ui::ITEM_R_ICON_ONLY,
                     "",
                     ICON_DECORATE_UNLOCKED);
      }
      else {
        colsub->label("", ICON_NONE);
      }
      colsub->prop(ptr,
                   "lock_rotation",
                   ui::ITEM_R_TOGGLE | ui::ITEM_R_ICON_ONLY,
                   "",
                   ICON_DECORATE_UNLOCKED);
      break;
    default: /* euler rotations */
      colsub = &split->column(true);
      colsub->prop(ptr, "rotation_euler", UI_ITEM_NONE, IFACE_("Rotation"), ICON_NONE);
      colsub = &split->column(true);
      colsub->emboss_set(ui::EmbossType::NoneOrStatus);
      colsub->label("", ICON_NONE);
      colsub->prop(ptr,
                   "lock_rotation",
                   ui::ITEM_R_TOGGLE | ui::ITEM_R_ICON_ONLY,
                   "",
                   ICON_DECORATE_UNLOCKED);
      break;
  }

  ui::Layout &row = layout.row(/*align=*/true);
  row.prop(ptr, "rotation_mode", UI_ITEM_NONE, "", ICON_NONE);

  split = &layout.split(0.8f, false);
  colsub = &split->column(true);
  colsub->prop(ptr, "scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  colsub = &split->column(true);
  colsub->emboss_set(ui::EmbossType::NoneOrStatus);
  colsub->label("", ICON_NONE);
  colsub->prop(
      ptr, "lock_scale", ui::ITEM_R_TOGGLE | ui::ITEM_R_ICON_ONLY, "", ICON_DECORATE_UNLOCKED);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Panel
 * \{ */

static void do_view3d_region_buttons(bContext *C, void * /*index*/, int event)
{
  const Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  switch (event) {
    case B_TRANSFORM_PANEL_MEDIAN:
    case B_TRANSFORM_PANEL_ELEM_DIMS:
      if (ob) {
        v3d_editvertex_buts(C, nullptr, v3d, ob, 1.0);
      }
      break;
    case B_TRANSFORM_PANEL_OBJECT_DIMS:
      if (ob) {
        v3d_object_dimension_buts(C, nullptr, v3d, ob);
      }
      break;
  }

  /* default for now */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);
}

static bool view3d_panel_transform_poll(const bContext *C, PanelType * /*pt*/)
{
  const Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
  return (BKE_view_layer_active_base_get(view_layer) != nullptr);
}

static void view3d_panel_transform(const bContext *C, Panel *panel)
{
  ui::Block *block;
  const Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  Object *obedit = OBEDIT_FROM_OBACT(ob);

  block = panel->layout->block();
  block_func_handle_set(block, do_view3d_region_buttons, nullptr);

  ui::Layout &col = panel->layout->column(false);

  if (ob == obedit) {
    View3D *v3d = CTX_wm_view3d(C);
    v3d_editvertex_buts(C, &col, v3d, ob, FLT_MAX);
  }
  else {
    PointerRNA obptr = RNA_id_pointer_create(&ob->id);
    v3d_transform_butsR(col, &obptr);

    /* Dimensions and editmode are mostly the same check. */
    if (OB_TYPE_SUPPORT_EDITMODE(ob->type) || ELEM(ob->type, OB_VOLUME, OB_LIGHTPROBE)) {
      View3D *v3d = CTX_wm_view3d(C);
      v3d_object_dimension_buts(nullptr, &col, v3d, ob);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void view3d_buttons_register(ARegionType *art)
{
  PanelType *pt;

  pt = MEM_new_zeroed<PanelType>("spacetype view3d panel object");
  STRNCPY_UTF8(pt->idname, "VIEW3D_PT_transform");
  STRNCPY_UTF8(pt->label, N_("Transform")); /* XXX C panels unavailable through RNA bpy.types! */
  STRNCPY_UTF8(pt->category, "Item");
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = view3d_panel_transform;
  pt->poll = view3d_panel_transform_poll;
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_new_zeroed<PanelType>("spacetype view3d panel vgroup");
  STRNCPY_UTF8(pt->idname, "VIEW3D_PT_vgroup");
  STRNCPY_UTF8(pt->label,
               N_("Vertex Weights")); /* XXX C panels unavailable through RNA bpy.types! */
  STRNCPY_UTF8(pt->category, "Item");
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = view3d_panel_vgroup;
  pt->poll = view3d_panel_vgroup_poll;
  BLI_addtail(&art->paneltypes, pt);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Mode Pie or Toggle Operator
 * \{ */

static wmOperatorStatus view3d_object_mode_menu_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    BKE_report(op->reports, RPT_WARNING, "No active object found");
    return OPERATOR_CANCELLED;
  }
  ui::pie_menu_invoke(C, "VIEW3D_MT_object_mode_pie", CTX_wm_window(C)->runtime->eventstate);
  return OPERATOR_CANCELLED;
}

void VIEW3D_OT_object_mode_pie_or_toggle(wmOperatorType *ot)
{
  ot->name = "Object Mode Menu";
  ot->idname = "VIEW3D_OT_object_mode_pie_or_toggle";

  ot->exec = view3d_object_mode_menu_exec;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

}  // namespace blender
