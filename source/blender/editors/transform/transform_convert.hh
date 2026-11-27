/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 * \brief conversion and adaptation of different datablocks to a common struct.
 */

#pragma once

#include <climits>

#include "DNA_listBase.h"
#include "DNA_scene_types.h"

#include "BLI_index_mask.hh"
#include "BLI_vector_set.hh"

#include "UI_view2d.hh"

#include "transform.hh"

struct TransData;
struct TransInfo;
namespace blender {

struct EditMesh;
struct BMesh;
struct bConstraint;
struct Object;
struct bContext;

namespace ed::transform {

struct TransConvertTypeInfo {
  int flags; /* #eTFlag. */

  /**
   * Allocate and initialize `t->data`.
   */
  void (*create_trans_data)(bContext *C, TransInfo *t);

  /**
   * Force recalculation of data during transformation.
   */
  void (*recalc_data)(TransInfo *t);

  /**
   * Called when the operation is finished.
   */
  void (*special_aftertrans_update)(bContext *C, TransInfo *t);
};

/**
 * Structure used for Edge Slide operation.
 * The data is filled based on the 'transform_convert_' type.
 */
struct TransDataEdgeSlideVert {
  TransData *td;
  float3 dir_side[2]; /* Directional vectors on the sides. */
  float edge_len;     /* Distance between vectors. */
  int loop_nr;        /* Number that identifies the group of connected edges. */

  const float *v_co_orig() const
  {
    return this->td->iloc;
  }
};

/**
 * Structure used for Vert Slide operation.
 * The data is filled based on the 'transform_convert_' type.
 */
struct TransDataVertSlideVert {
  TransData *td;
  Span<float3> co_link_orig_3d; /* Target locations. */
  int co_link_curr;

  const float *co_orig_3d() const
  {
    return this->td->iloc;
  }

  const float3 &co_dest_3d() const
  {
    return this->co_link_orig_3d[this->co_link_curr];
  }
};

/* `transform_convert.cc` */

int special_transform_moving(TransInfo *t);
/**
 * Inserting keys, point-cache, redraw events.
 */
void special_aftertrans_update(bContext *C, TransInfo *t);
void sort_trans_data_dist(TransInfo *t);
void create_trans_data(bContext *C, TransInfo *t);
/**
 * Called for updating while transform acts, once per redraw.
 */
void recalc_data(TransInfo *t);

/* `transform_convert_mesh.cc` */

void transform_convert_mesh_customdatacorrect_init(TransInfo *t);

/********************* intern **********************/

/* `transform_convert.cc` */

bool transform_mode_use_local_origins(const TransInfo *t);
/**
 * Transforming around ourselves is no use, fall back to individual origins,
 * useful for curve/armatures.
 */
void transform_around_single_fallback_ex(TransInfo *t, int data_len_all);
void transform_around_single_fallback(TransInfo *t);
bool constraints_list_needinv(TransInfo *t, ListBaseT<bConstraint> *list);
void transform_convert_clip_mirror_modifier_apply(TransDataContainer *tc);

/* `transform_convert_cursor.cc` */

extern TransConvertTypeInfo TransConvertType_Cursor3D;

/* `transform_convert_mesh.cc` */

extern TransConvertTypeInfo TransConvertType_Mesh;

struct TransIslandData {
  float (*center)[3];
  float (*axismtx)[3][3];
  int island_tot;
  int *island_vert_map;
};

struct MirrorDataVert {
  int index;
  int flag;
};

struct TransMirrorData {
  MirrorDataVert *vert_map;
  int mirror_elem_len;
};

struct TransMeshDataCrazySpace {
  float (*quats)[4];
  Array<float3x3, 0> defmats;
};

void transform_convert_mesh_islands_calc(EditMesh *em,
                                         BMesh *bm,
                                         bool calc_single_islands,
                                         bool calc_island_center,
                                         bool calc_island_axismtx,
                                         TransIslandData *r_island_data);
void transform_convert_mesh_islanddata_free(TransIslandData *island_data);
/**
 * \param mtx: Measure distance in this space.
 * \param dists: Store the closest connected distance to selected vertices.
 * \param index: Optionally store the original index we're measuring the distance to (can be NULL).
 */
void transform_convert_mesh_connectivity_distance(BMesh *bm,
                                                  const float mtx[3][3],
                                                  float *dists,
                                                  int *index);
void transform_convert_mesh_mirrordata_calc(EditMesh *em,
                                            BMesh *bm,
                                            bool use_select,
                                            bool use_topology,
                                            const bool mirror_axis[3],
                                            TransMirrorData *r_mirror_data);
void transform_convert_mesh_mirrordata_free(TransMirrorData *mirror_data);
/**
 * Detect CrazySpace (Blender term).
 * Vertices with space affected by quaternions are marked with #BM_ELEM_TAG.
 */
void transform_convert_mesh_crazyspace_detect(TransInfo *t,
                                              TransDataContainer *tc,
                                              TransMeshDataCrazySpace *r_crazyspace_data);
void transform_convert_mesh_crazyspace_transdata_set(const float mtx[3][3],
                                                     const float smtx[3][3],
                                                     const float defmat[3][3],
                                                     const float quat[4],
                                                     TransData *r_td);
void transform_convert_mesh_crazyspace_free(TransMeshDataCrazySpace *r_crazyspace_data);

float *transform_convert_mesh_shell_factor_calc(const TransDataContainer *tc);

Array<TransDataVertSlideVert> transform_mesh_vert_slide_data_create(
    const TransDataContainer *tc, Vector<float3> &r_loc_dst_buffer);

Array<TransDataEdgeSlideVert> transform_mesh_edge_slide_data_create(const TransDataContainer *tc,
                                                                    int *r_group_len);

/* `transform_convert_mesh_edge.cc` */

extern TransConvertTypeInfo TransConvertType_MeshEdge;

/* `transform_convert_mesh_skin.cc` */

extern TransConvertTypeInfo TransConvertType_MeshSkin;

/* `transform_convert_mesh_vert_cdata.cc` */

extern TransConvertTypeInfo TransConvertType_MeshVertCData;

/* `transform_convert_object.cc` */

extern TransConvertTypeInfo TransConvertType_Object;

/* `transform_convert_object_texspace.cc` */

extern TransConvertTypeInfo TransConvertType_ObjectTexSpace;

}  // namespace ed::transform
}  // namespace blender
