/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BLI_math_geom_c.hh"
#include "BLI_rect.hh"

#include "BKE_attribute.hh"
#include "BKE_editmesh.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_iterators.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_view3d_runtime.hh"

#include "DEG_depsgraph_query.hh"

#include "bmesh.hh"

#include "ED_view3d.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Clipping Utilities
 * \{ */

int ED_view3d_content_planes_from_clip_flag(const ARegion *region,
                                            const Object *ob,
                                            const eV3DProjTest clip_flag,
                                            float planes[6][4])
{
  BLI_assert(clip_flag & V3D_PROJ_TEST_CLIP_CONTENT);

  float *clip_xmin = nullptr, *clip_xmax = nullptr;
  float *clip_ymin = nullptr, *clip_ymax = nullptr;
  float *clip_zmin = nullptr, *clip_zmax = nullptr;

  int planes_len = 0;

  /* The order of `planes` has been selected based on the likelihood of points being fully
   * outside the plane to increase the chance of an early exit in #clip_segment_v3_plane_n.
   * With "near" being most likely and "far" being unlikely.
   *
   * Otherwise the order of axes in `planes` isn't significant. */

  if (clip_flag & V3D_PROJ_TEST_CLIP_NEAR) {
    clip_zmin = planes[planes_len++];
  }
  if (clip_flag & V3D_PROJ_TEST_CLIP_WIN) {
    clip_xmin = planes[planes_len++];
    clip_xmax = planes[planes_len++];
    clip_ymin = planes[planes_len++];
    clip_ymax = planes[planes_len++];
  }
  if (clip_flag & V3D_PROJ_TEST_CLIP_FAR) {
    clip_zmax = planes[planes_len++];
  }

  BLI_assert(planes_len <= 6);
  if (planes_len != 0) {
    RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
    const float4x4 projection = ED_view3d_ob_project_mat_get(rv3d, ob);
    planes_from_projmat(
        projection.ptr(), clip_xmin, clip_xmax, clip_ymin, clip_ymax, clip_zmin, clip_zmax);
  }
  return planes_len;
}

bool ED_view3d_project_segment_to_screen_with_content_clip_planes(const ARegion *region,
                                                                  const float v_a[3],
                                                                  const float v_b[3],
                                                                  const eV3DProjTest clip_flag,
                                                                  const rctf *win_rect,
                                                                  const float content_planes[][4],
                                                                  const int content_planes_len,
                                                                  /* Output. */
                                                                  float r_screen_co_a[2],
                                                                  float r_screen_co_b[2])
{
  /* Clipping already handled, no need to check in projection. */
  eV3DProjTest clip_flag_nowin = clip_flag & ~V3D_PROJ_TEST_CLIP_WIN;

  const eV3DProjStatus status_a = ED_view3d_project_float_object(
      region, v_a, r_screen_co_a, clip_flag_nowin);
  const eV3DProjStatus status_b = ED_view3d_project_float_object(
      region, v_b, r_screen_co_b, clip_flag_nowin);

  if ((status_a == V3D_PROJ_RET_OK) && (status_b == V3D_PROJ_RET_OK)) {
    if (clip_flag & V3D_PROJ_TEST_CLIP_WIN) {
      if (!BLI_rctf_isect_segment(win_rect, r_screen_co_a, r_screen_co_b)) {
        return false;
      }
    }
  }
  else {
    if (content_planes_len == 0) {
      return false;
    }

    /* Both too near, ignore. */
    if (status_a == V3D_PROJ_RET_CLIP_NEAR && status_b == V3D_PROJ_RET_CLIP_NEAR) {
      return false;
    }

    /* Both too far, ignore. */
    if (status_a == V3D_PROJ_RET_CLIP_FAR && status_b == V3D_PROJ_RET_CLIP_FAR) {
      return false;
    }

    /* Simple cases have been ruled out, clip by viewport planes, then re-project. */
    float v_a_clip[3], v_b_clip[3];
    if (!clip_segment_v3_plane_n(v_a, v_b, content_planes, content_planes_len, v_a_clip, v_b_clip))
    {
      return false;
    }

    if ((ED_view3d_project_float_object(region, v_a_clip, r_screen_co_a, clip_flag_nowin) !=
         V3D_PROJ_RET_OK) ||
        (ED_view3d_project_float_object(region, v_b_clip, r_screen_co_b, clip_flag_nowin) !=
         V3D_PROJ_RET_OK))
    {
      return false;
    }

    /* No need for #V3D_PROJ_TEST_CLIP_WIN check here,
     * clipping the segment by planes handle this. */
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Private User Data Structures
 * \{ */

struct foreachScreenObjectVert_userData {
  void (*func)(void *user_data, const float screen_co[2], int index);
  void *user_data;
  ViewContext vc;
  VArraySpan<bool> hide_vert;
  eV3DProjTest clip_flag;
};

struct foreachScreenVert_userData {
  void (*func)(void *user_data, BMVert *eve, const float screen_co[2], int index);
  void *user_data;
  ViewContext vc;
  BMesh *bm;
  eV3DProjTest clip_flag;
};

/** User data structures for evaluated mesh callbacks. */
struct foreachScreenEdge_userData {
  void (*func)(void *user_data,
               BMEdge *eed,
               const float screen_co_a[2],
               const float screen_co_b[2],
               int index);
  void *user_data;
  ViewContext vc;
  BMesh *bm;
  eV3DProjTest clip_flag;

  rctf win_rect; /* copy of: vc.region->winx/winy, use for faster tests, minx/y will always be 0 */

  /**
   * Clip plans defined by the view bounds,
   * use when #V3D_PROJ_TEST_CLIP_CONTENT is enabled.
   */
  float content_planes[6][4];
  int content_planes_len;
};

struct foreachScreenFace_userData {
  void (*func)(void *user_data, BMFace *efa, const float screen_co_b[2], int index);
  void *user_data;
  ViewContext vc;
  BMesh *bm;
  eV3DProjTest clip_flag;
};

/**
 * \note foreach functions should be called while drawing or directly after
 * if not, #ED_view3d_init_mats_rv3d() can be used for selection tools
 * but would not give correct results with dupli's for eg. which don't
 * use the object matrix in the usual way.
 */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-Mesh: For Each Screen Vertex
 * \{ */

static void meshobject_foreachScreenVert__mapFunc(void *user_data,
                                                  int index,
                                                  const float co[3],
                                                  const float /*no*/[3])
{
  foreachScreenObjectVert_userData *data = static_cast<foreachScreenObjectVert_userData *>(
      user_data);
  if (!data->hide_vert.is_empty() && data->hide_vert[index]) {
    return;
  }

  float screen_co[2];

  if (ED_view3d_project_float_object(data->vc.region, co, screen_co, data->clip_flag) !=
      V3D_PROJ_RET_OK)
  {
    return;
  }

  data->func(data->user_data, screen_co, index);
}

void meshobject_foreachScreenVert(const ViewContext *vc,
                                  void (*func)(void *user_data,
                                               const float screen_co[2],
                                               int index),
                                  void *user_data,
                                  eV3DProjTest clip_flag)
{
  BLI_assert((clip_flag & V3D_PROJ_TEST_CLIP_CONTENT) == 0);
  foreachScreenObjectVert_userData data;

  const Object *ob_eval = DEG_get_evaluated(vc->depsgraph, vc->obact);
  const Mesh *mesh = BKE_object_get_evaluated_mesh(ob_eval);
  const bke::AttributeAccessor attributes = mesh->attributes();

  ED_view3d_check_mats_rv3d(vc->rv3d);

  data.vc = *vc;
  data.func = func;
  data.user_data = user_data;
  data.clip_flag = clip_flag;
  data.hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);

  if (clip_flag & V3D_PROJ_TEST_CLIP_BB) {
    ED_view3d_clipping_local(vc->rv3d, vc->obact->object_to_world().ptr());
  }

  BKE_mesh_foreach_mapped_vert(
      mesh, meshobject_foreachScreenVert__mapFunc, &data, MESH_FOREACH_NOP);
}

static void mesh_foreachScreenVert__mapFunc(void *user_data,
                                            int index,
                                            const float co[3],
                                            const float /*no*/[3])
{
  foreachScreenVert_userData *data = static_cast<foreachScreenVert_userData *>(user_data);
  BMVert *eve = BM_vert_at_index(data->bm, index);
  if (BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) [[unlikely]] {
    return;
  }

  float screen_co[2];
  if (ED_view3d_project_float_object(data->vc.region, co, screen_co, data->clip_flag) !=
      V3D_PROJ_RET_OK)
  {
    return;
  }

  data->func(data->user_data, eve, screen_co, index);
}

void mesh_foreachScreenVert(
    const ViewContext *vc,
    void (*func)(void *user_data, BMVert *eve, const float screen_co[2], int index),
    void *user_data,
    eV3DProjTest clip_flag)
{
  foreachScreenVert_userData data;

  const Mesh *mesh = bke::editbmesh_get_eval_cage_from_orig(
      vc->depsgraph, vc->scene, vc->obedit, &CD_MASK_BAREMESH);
  mesh = BKE_mesh_wrapper_ensure_subdivision(mesh);

  ED_view3d_check_mats_rv3d(vc->rv3d);

  data.vc = *vc;
  data.bm = BKE_editmesh_bmesh_ensure(vc->obedit);
  data.func = func;
  data.user_data = user_data;
  data.clip_flag = clip_flag;

  if (clip_flag & V3D_PROJ_TEST_CLIP_BB) {
    ED_view3d_clipping_local(vc->rv3d,
                             vc->obedit->object_to_world().ptr()); /* for local clipping lookups */
  }

  BM_mesh_elem_table_ensure(data.bm, BM_VERT);
  BKE_mesh_foreach_mapped_vert(mesh, mesh_foreachScreenVert__mapFunc, &data, MESH_FOREACH_NOP);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-Mesh: For Each Screen Mesh Edge
 * \{ */

static void mesh_foreachScreenEdge__mapFunc(void *user_data,
                                            int index,
                                            const float v_a[3],
                                            const float v_b[3])
{
  foreachScreenEdge_userData *data = static_cast<foreachScreenEdge_userData *>(user_data);
  BMEdge *eed = BM_edge_at_index(data->bm, index);
  if (BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) [[unlikely]] {
    return;
  }

  float screen_co_a[2], screen_co_b[2];
  if (!ED_view3d_project_segment_to_screen_with_content_clip_planes(data->vc.region,
                                                                    v_a,
                                                                    v_b,
                                                                    data->clip_flag,
                                                                    &data->win_rect,
                                                                    data->content_planes,
                                                                    data->content_planes_len,
                                                                    screen_co_a,
                                                                    screen_co_b))
  {
    return;
  }

  data->func(data->user_data, eed, screen_co_a, screen_co_b, index);
}

void mesh_foreachScreenEdge(const ViewContext *vc,
                            void (*func)(void *user_data,
                                         BMEdge *eed,
                                         const float screen_co_a[2],
                                         const float screen_co_b[2],
                                         int index),
                            void *user_data,
                            eV3DProjTest clip_flag)
{
  foreachScreenEdge_userData data;

  const Mesh *mesh = bke::editbmesh_get_eval_cage_from_orig(
      vc->depsgraph, vc->scene, vc->obedit, &CD_MASK_BAREMESH);
  mesh = BKE_mesh_wrapper_ensure_subdivision(mesh);

  ED_view3d_check_mats_rv3d(vc->rv3d);

  data.vc = *vc;
  data.bm = BKE_editmesh_bmesh_ensure(vc->obedit);

  data.win_rect.xmin = 0;
  data.win_rect.ymin = 0;
  data.win_rect.xmax = vc->region->winx;
  data.win_rect.ymax = vc->region->winy;

  data.func = func;
  data.user_data = user_data;
  data.clip_flag = clip_flag;

  if (clip_flag & V3D_PROJ_TEST_CLIP_BB) {
    ED_view3d_clipping_local(vc->rv3d,
                             vc->obedit->object_to_world().ptr()); /* for local clipping lookups */
  }

  if (clip_flag & V3D_PROJ_TEST_CLIP_CONTENT) {
    data.content_planes_len = ED_view3d_content_planes_from_clip_flag(
        vc->region, vc->obedit, clip_flag, data.content_planes);
  }
  else {
    data.content_planes_len = 0;
  }

  BM_mesh_elem_table_ensure(data.bm, BM_EDGE);
  BKE_mesh_foreach_mapped_edge(mesh, data.bm->totedge, mesh_foreachScreenEdge__mapFunc, &data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-Mesh: For Each Screen Edge (Bounding Box Clipped)
 * \{ */

/**
 * Only call for bound-box clipping.
 * Otherwise call #mesh_foreachScreenEdge__mapFunc
 */
static void mesh_foreachScreenEdge_clip_bb_segment__mapFunc(void *user_data,
                                                            int index,
                                                            const float v_a[3],
                                                            const float v_b[3])
{
  foreachScreenEdge_userData *data = static_cast<foreachScreenEdge_userData *>(user_data);
  BMEdge *eed = BM_edge_at_index(data->bm, index);
  if (BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) [[unlikely]] {
    return;
  }

  BLI_assert(data->clip_flag & V3D_PROJ_TEST_CLIP_BB);

  float v_a_clip[3], v_b_clip[3];
  if (!clip_segment_v3_plane_n(
          v_a, v_b, data->vc.rv3d->runtime->clip_local, 4, v_a_clip, v_b_clip))
  {
    return;
  }

  float screen_co_a[2], screen_co_b[2];
  if (!ED_view3d_project_segment_to_screen_with_content_clip_planes(data->vc.region,
                                                                    v_a_clip,
                                                                    v_b_clip,
                                                                    data->clip_flag,
                                                                    &data->win_rect,
                                                                    data->content_planes,
                                                                    data->content_planes_len,
                                                                    screen_co_a,
                                                                    screen_co_b))
  {
    return;
  }

  data->func(data->user_data, eed, screen_co_a, screen_co_b, index);
}

void mesh_foreachScreenEdge_clip_bb_segment(const ViewContext *vc,
                                            void (*func)(void *user_data,
                                                         BMEdge *eed,
                                                         const float screen_co_a[2],
                                                         const float screen_co_b[2],
                                                         int index),
                                            void *user_data,
                                            eV3DProjTest clip_flag)
{
  foreachScreenEdge_userData data;

  const Mesh *mesh = bke::editbmesh_get_eval_cage_from_orig(
      vc->depsgraph, vc->scene, vc->obedit, &CD_MASK_BAREMESH);
  mesh = BKE_mesh_wrapper_ensure_subdivision(mesh);

  ED_view3d_check_mats_rv3d(vc->rv3d);

  data.vc = *vc;
  data.bm = BKE_editmesh_bmesh_ensure(vc->obedit);

  data.win_rect.xmin = 0;
  data.win_rect.ymin = 0;
  data.win_rect.xmax = vc->region->winx;
  data.win_rect.ymax = vc->region->winy;

  data.func = func;
  data.user_data = user_data;
  data.clip_flag = clip_flag;

  if (clip_flag & V3D_PROJ_TEST_CLIP_CONTENT) {
    data.content_planes_len = ED_view3d_content_planes_from_clip_flag(
        vc->region, vc->obedit, clip_flag, data.content_planes);
  }
  else {
    data.content_planes_len = 0;
  }

  BM_mesh_elem_table_ensure(data.bm, BM_EDGE);

  if ((clip_flag & V3D_PROJ_TEST_CLIP_BB) && (vc->rv3d->clipbb != nullptr)) {
    ED_view3d_clipping_local(
        vc->rv3d, vc->obedit->object_to_world().ptr()); /* for local clipping lookups. */
    BKE_mesh_foreach_mapped_edge(
        mesh, data.bm->totedge, mesh_foreachScreenEdge_clip_bb_segment__mapFunc, &data);
  }
  else {
    BKE_mesh_foreach_mapped_edge(mesh, data.bm->totedge, mesh_foreachScreenEdge__mapFunc, &data);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-Mesh: For Each Screen Face Center
 * \{ */

static void mesh_foreachScreenFace__mapFunc(void *user_data,
                                            int index,
                                            const float cent[3],
                                            const float /*no*/[3])
{
  foreachScreenFace_userData *data = static_cast<foreachScreenFace_userData *>(user_data);
  BMFace *efa = BM_face_at_index(data->bm, index);
  if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) [[unlikely]] {
    return;
  }

  float screen_co[2];
  if (ED_view3d_project_float_object(data->vc.region, cent, screen_co, data->clip_flag) !=
      V3D_PROJ_RET_OK)
  {
    return;
  }

  data->func(data->user_data, efa, screen_co, index);
}

void mesh_foreachScreenFace(
    const ViewContext *vc,
    void (*func)(void *user_data, BMFace *efa, const float screen_co_b[2], int index),
    void *user_data,
    const eV3DProjTest clip_flag)
{
  BLI_assert((clip_flag & V3D_PROJ_TEST_CLIP_CONTENT) == 0);
  foreachScreenFace_userData data;

  const Mesh *mesh = bke::editbmesh_get_eval_cage_from_orig(
      vc->depsgraph, vc->scene, vc->obedit, &CD_MASK_BAREMESH);
  mesh = BKE_mesh_wrapper_ensure_subdivision(mesh);
  ED_view3d_check_mats_rv3d(vc->rv3d);

  data.vc = *vc;
  data.bm = BKE_editmesh_bmesh_ensure(vc->obedit);
  data.func = func;
  data.user_data = user_data;
  data.clip_flag = clip_flag;

  BM_mesh_elem_table_ensure(data.bm, BM_FACE);

  const int face_dot_tags_num = mesh->runtime->subsurf_face_dot_tags.size();
  if (face_dot_tags_num && (face_dot_tags_num != mesh->verts_num)) {
    BKE_mesh_foreach_mapped_subdiv_face_center(
        mesh, mesh_foreachScreenFace__mapFunc, &data, MESH_FOREACH_NOP);
  }
  else {
    BKE_mesh_foreach_mapped_face_center(
        mesh, mesh_foreachScreenFace__mapFunc, &data, MESH_FOREACH_NOP);
  }
}

/** \} */

}  // namespace blender
