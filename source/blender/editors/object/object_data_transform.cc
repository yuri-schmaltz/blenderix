/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 *
 * Use to transform object origins only.
 *
 * This is a small API to store & apply transformations to object data,
 * where a transformation matrix can be continually applied on top of the original values
 * so we don't lose precision over time.
 */

#include <cstdlib>
#include <cstring>

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BLI_math_matrix.hh"

#include "BKE_editmesh.hh"
#include "BKE_key.hh"
#include "BKE_mesh_types.hh"

#include "bmesh.hh"

#include "DEG_depsgraph.hh"

#include "ED_mesh.hh"
#include "ED_object.hh"

namespace blender::ed::object {

struct XFormObjectData_Mesh : public XFormObjectData {
  /* Optional data for shape keys. */
  Array<float3> key_data;
  Array<float3> positions;
  bool is_edit_mode = false;
  ~XFormObjectData_Mesh() override = default;
};

static std::unique_ptr<XFormObjectData> data_xform_create_ex(ID *id, bool is_edit_mode)
{
  if (id == nullptr) {
    return {};
  }

  if (GS(id->name) != ID_ME) {
    return {};
  }

  Mesh *mesh = id_cast<Mesh *>(id);
  Key *key = mesh->key;
  const int key_index = -1;

  if (is_edit_mode) {
    const BMesh *bm = BKE_editmesh_bmesh_ensure(mesh);
    auto xod = std::make_unique<XFormObjectData_Mesh>();
    xod->id = id;
    xod->is_edit_mode = is_edit_mode;
    xod->positions.reinitialize(bm->totvert);

    BM_mesh_vert_coords_get(bm, xod->positions.as_mutable_span());

    if (key != nullptr) {
      const size_t key_size = BKE_keyblock_element_calc_size_from_shape(key, key_index);
      if (key_size) {
        xod->key_data.reinitialize(key_size);
        BKE_keyblock_data_get_from_shape(key, xod->key_data, key_index);
      }
    }
    return xod;
  }

  auto xod = std::make_unique<XFormObjectData_Mesh>();
  xod->id = id;
  xod->is_edit_mode = is_edit_mode;
  xod->positions = mesh->vert_positions();

  if (key != nullptr) {
    const size_t key_size = BKE_keyblock_element_calc_size_from_shape(key, key_index);
    if (key_size) {
      xod->key_data.reinitialize(key_size);
      BKE_keyblock_data_get_from_shape(key, xod->key_data, key_index);
    }
  }
  return xod;
}

std::unique_ptr<XFormObjectData> data_xform_create(ID *id)
{
  return data_xform_create_ex(id, false);
}

std::unique_ptr<XFormObjectData> data_xform_create_from_edit_mode(ID *id)
{
  return data_xform_create_ex(id, true);
}

void data_xform_by_mat4(XFormObjectData &xod_base, const float4x4 &transform)
{
  if (GS(xod_base.id->name) != ID_ME) {
    return;
  }

  Mesh *mesh = id_cast<Mesh *>(xod_base.id);
  Key *key = mesh->key;
  const int key_index = -1;

  const auto &xod = reinterpret_cast<XFormObjectData_Mesh &>(xod_base);
  if (xod.is_edit_mode) {
    BMesh *bm = BKE_editmesh_bmesh_ensure(mesh);
    BM_mesh_vert_coords_apply_with_mat4(bm, xod.positions, transform);
  }
  else {
    math::transform_points(xod.positions, transform, mesh->vert_positions_for_write());
    mesh->tag_positions_changed();
  }

  if (key != nullptr) {
    BKE_keyblock_data_set_with_mat4(key, key_index, xod.key_data, transform);
  }
}

void data_xform_restore(XFormObjectData &xod_base)
{
  if (GS(xod_base.id->name) != ID_ME) {
    return;
  }

  Mesh *mesh = id_cast<Mesh *>(xod_base.id);
  Key *key = mesh->key;
  const int key_index = -1;

  const auto &xod = reinterpret_cast<XFormObjectData_Mesh &>(xod_base);
  if (xod.is_edit_mode) {
    BMesh *bm = BKE_editmesh_bmesh_ensure(mesh);
    BM_mesh_vert_coords_apply(bm, xod.positions);
  }
  else {
    mesh->vert_positions_for_write().copy_from(xod.positions);
    mesh->tag_positions_changed();
  }

  if ((key != nullptr) && !xod.key_data.is_empty()) {
    BKE_keyblock_data_set(key, key_index, xod.key_data.data());
  }
}

void data_xform_tag_update(XFormObjectData &xod_base)
{
  if (GS(xod_base.id->name) != ID_ME) {
    return;
  }

  Mesh *mesh = id_cast<Mesh *>(xod_base.id);
  const auto &xod = reinterpret_cast<XFormObjectData_Mesh &>(xod_base);
  if (xod.is_edit_mode) {
    EDBMUpdate_Params params{};
    params.calc_looptris = true;
    params.calc_normals = true;
    params.is_destructive = false;
    EDBM_update(mesh, &params);
  }
  DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY);
}

}  // namespace blender::ed::object
