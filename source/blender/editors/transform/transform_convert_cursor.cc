/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 *
 * Instead of transforming the selection, move the 2D/3D cursor.
 */

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix_c.hh"
#include "BLI_math_rotation_c.hh"
#include "BLI_math_vector_c.hh"

#include "BKE_library.hh"
#include "BKE_report.hh"

#include "transform.hh"
#include "transform_convert.hh"

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name View 3D Cursor
 * \{ */

static void createTransCursor_view3d(bContext * /*C*/, TransInfo *t)
{
  TransData *td;
  TransDataExtension *td_ext;

  Scene *scene = t->scene;
  if (!ID_IS_EDITABLE(scene)) {
    BKE_report(t->reports, RPT_ERROR, "Cannot create transform on linked data");
    return;
  }

  View3DCursor *cursor = &scene->cursor;
  {
    BLI_assert(t->data_container_len == 1);
    TransDataContainer *tc = t->data_container;
    tc->data_len = 1;
    td = tc->data = MEM_new_zeroed<TransData>("TransTexspace");
    td_ext = tc->data_ext = MEM_new_zeroed<TransDataExtension>("TransTexspace");
  }

  td->flag = TD_SELECTED;
  copy_v3_v3(td->center, cursor->location);

  unit_m3(td->mtx);
  copy_m3_m3(td->axismtx, cursor->matrix<float3x3>().ptr());
  normalize_m3(td->axismtx);
  pseudoinverse_m3_m3(td->smtx, td->mtx, PSEUDOINVERSE_EPSILON);

  td->loc = cursor->location;
  copy_v3_v3(td->iloc, cursor->location);

  if (cursor->rotation_mode > 0) {
    td_ext->rot = cursor->rotation_euler;
    td_ext->rotAxis = nullptr;
    td_ext->rotAngle = nullptr;
    td_ext->quat = nullptr;

    copy_v3_v3(td_ext->irot, cursor->rotation_euler);
  }
  else if (cursor->rotation_mode == ROT_MODE_AXISANGLE) {
    td_ext->rot = nullptr;
    td_ext->rotAxis = cursor->rotation_axis;
    td_ext->rotAngle = &cursor->rotation_angle;
    td_ext->quat = nullptr;

    td_ext->irotAngle = cursor->rotation_angle;
    copy_v3_v3(td_ext->irotAxis, cursor->rotation_axis);
  }
  else {
    td_ext->rot = nullptr;
    td_ext->rotAxis = nullptr;
    td_ext->rotAngle = nullptr;
    td_ext->quat = cursor->rotation_quaternion;

    copy_qt_qt(td_ext->iquat, cursor->rotation_quaternion);
  }
  td_ext->rotOrder = cursor->rotation_mode;
}

static void recalcData_cursor_view3d(TransInfo *t)
{
  DEG_id_tag_update(&t->scene->id, ID_RECALC_SYNC_TO_EVAL);
}

/** \} */

TransConvertTypeInfo TransConvertType_Cursor3D = {
    /*flags*/ 0,
    /*create_trans_data*/ createTransCursor_view3d,
    /*recalc_data*/ recalcData_cursor_view3d,
    /*special_aftertrans_update*/ nullptr,
};

}  // namespace blender::ed::transform
