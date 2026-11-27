/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edutil
 */

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "MEM_guardedalloc.h"

#include "BLI_math_vector_c.hh"
#include "BLI_utildefines.hh"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_mesh_iterators.hh"
#include "BKE_mesh_types.hh"
#include "BKE_object.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "ED_transverts.hh" /* own include */

namespace blender {

void ED_transverts_update_obedit(TransVertStore * /*tvs*/, Object *obedit)
{
  BLI_assert(ED_transverts_check_obedit(obedit) == true);

  DEG_id_tag_update(obedit->data, ID_RECALC_GEOMETRY);

  if (obedit->type == OB_MESH) {
    BMesh *bm = BKE_editmesh_bmesh_ensure(obedit);
    BM_mesh_normals_update(bm);
  }
}

static void set_mapped_co(void *vuserdata, int index, const float co[3], const float /*no*/[3])
{
  void **userdata = static_cast<void **>(vuserdata);
  BMesh *bm = static_cast<BMesh *>(userdata[0]);
  TransVert *tv = static_cast<TransVert *>(userdata[1]);
  BMVert *eve = BM_vert_at_index(bm, index);

  if (BM_elem_index_get(eve) != TM_INDEX_SKIP) {
    tv = &tv[BM_elem_index_get(eve)];

    /* Be clever, get the closest vertex to the original,
     * behaves most logically when the mirror modifier is used for eg #33051. */
    if ((tv->flag & TX_VERT_USE_MAPLOC) == 0) {
      /* first time */
      copy_v3_v3(tv->maploc, co);
      tv->flag |= TX_VERT_USE_MAPLOC;
    }
    else {
      /* find best location to use */
      if (len_squared_v3v3(eve->co, co) < len_squared_v3v3(eve->co, tv->maploc)) {
        copy_v3_v3(tv->maploc, co);
      }
    }
  }
}

static void transverts_create_from_bmesh(
    TransVertStore &tvs, const Object &obedit, const EditMesh &em, BMesh &bm, const int mode)
{
  BMVert *eve;
  BMIter iter;
  void *userdata[2] = {&bm, nullptr};

  /* transform now requires awareness for select mode, so we tag the f1 flags in verts */
  tvs.transverts_tot = 0;
  if (em.selectmode & SCE_SELECT_VERTEX) {
    BM_ITER_MESH (eve, &iter, &bm, BM_VERTS_OF_MESH) {
      if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN) && BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
        BM_elem_index_set(eve, TM_INDEX_ON); /* set_dirty! */
        tvs.transverts_tot++;
      }
      else {
        BM_elem_index_set(eve, TM_INDEX_OFF); /* set_dirty! */
      }
    }
  }
  else if (em.selectmode & SCE_SELECT_EDGE) {
    BMEdge *eed;

    BM_ITER_MESH (eve, &iter, &bm, BM_VERTS_OF_MESH) {
      BM_elem_index_set(eve, TM_INDEX_OFF); /* set_dirty! */
    }

    BM_ITER_MESH (eed, &iter, &bm, BM_EDGES_OF_MESH) {
      if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) && BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
        BM_elem_index_set(eed->v1, TM_INDEX_ON); /* set_dirty! */
        BM_elem_index_set(eed->v2, TM_INDEX_ON); /* set_dirty! */
      }
    }

    BM_ITER_MESH (eve, &iter, &bm, BM_VERTS_OF_MESH) {
      if (BM_elem_index_get(eve) == TM_INDEX_ON) {
        tvs.transverts_tot++;
      }
    }
  }
  else {
    BMFace *efa;

    BM_ITER_MESH (eve, &iter, &bm, BM_VERTS_OF_MESH) {
      BM_elem_index_set(eve, TM_INDEX_OFF); /* set_dirty! */
    }

    BM_ITER_MESH (efa, &iter, &bm, BM_FACES_OF_MESH) {
      if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
        BMIter liter;
        BMLoop *l;

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          BM_elem_index_set(l->v, TM_INDEX_ON); /* set_dirty! */
        }
      }
    }

    BM_ITER_MESH (eve, &iter, &bm, BM_VERTS_OF_MESH) {
      if (BM_elem_index_get(eve) == TM_INDEX_ON) {
        tvs.transverts_tot++;
      }
    }
  }
  /* for any of the 3 loops above which all dirty the indices */
  bm.elem_index_dirty |= BM_VERT;

  /* and now make transverts */
  if (tvs.transverts_tot) {
    TransVert *tv = tvs.transverts = MEM_new_array_zeroed<TransVert>(tvs.transverts_tot, __func__);

    int a = 0;
    BM_ITER_MESH (eve, &iter, &bm, BM_VERTS_OF_MESH) {
      if (BM_elem_index_get(eve)) {
        BM_elem_index_set(eve, a); /* set_dirty! */
        copy_v3_v3(tv->oldloc, eve->co);
        tv->loc = eve->co;
        tv->flag = (BM_elem_index_get(eve) == TM_INDEX_ON) ? SELECT : 0;

        if (mode & TM_CALC_NORMALS) {
          tv->flag |= TX_VERT_USE_NORMAL;
          copy_v3_v3(tv->normal, eve->no);
        }

        tv++;
        a++;
      }
      else {
        BM_elem_index_set(eve, TM_INDEX_SKIP); /* set_dirty! */
      }
    }
    /* set dirty already, above */

    userdata[1] = tvs.transverts;
  }

  if (mode & TM_CALC_MAPLOC) {
    const Mesh *editmesh_eval_cage = BKE_object_get_editmesh_eval_cage(&obedit);
    if (tvs.transverts && editmesh_eval_cage) {
      BM_mesh_elem_table_ensure(&bm, BM_VERT);
      BKE_mesh_foreach_mapped_vert(editmesh_eval_cage, set_mapped_co, userdata, MESH_FOREACH_NOP);
    }
  }
}

bool ED_transverts_check_obedit(const Object *obedit)
{
  return obedit->type == OB_MESH;
}

void ED_transverts_create_from_obedit(TransVertStore *tvs, const Object *obedit, const int mode)
{
  BLI_assert(DEG_is_evaluated(obedit));

  tvs->transverts_tot = 0;

  if (obedit->type == OB_MESH) {
    /* The evaluated mesh may not have an edit-mesh, use the original. */
    const Object *object_orig = DEG_get_original(obedit);
    Mesh &mesh = *id_cast<Mesh *>(object_orig->data);
    BMesh *bm = BKE_editmesh_bmesh_ensure(&mesh);
    transverts_create_from_bmesh(*tvs, *obedit, *mesh.runtime->edit_mesh, *bm, mode);
  }

  if (!tvs->transverts_tot && tvs->transverts) {
    MEM_delete(tvs->transverts);
    tvs->transverts = nullptr;
  }

  tvs->mode = mode;
}

void ED_transverts_free(TransVertStore *tvs)
{
  MEM_SAFE_DELETE(tvs->transverts);
  tvs->transverts_tot = 0;
}

bool ED_transverts_poll(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit) {
    if (ED_transverts_check_obedit(obedit)) {
      return true;
    }
  }
  return false;
}

}  // namespace blender
