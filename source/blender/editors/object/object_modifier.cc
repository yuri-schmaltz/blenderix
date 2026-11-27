/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <cstdio>
#include <cstdlib>

#include "CLG_log.h"

#include "DNA_defs.h"
#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_array_utils.hh"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_force_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"

#include "BLI_array_utils.hh"
#include "BLI_bitmap.hh"
#include "BLI_implicit_sharing.hh"
#include "BLI_listbase.hh"
#include "BLI_string.hh"
#include "BLI_string_utf8.hh"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.hh"

#include "BKE_animsys.hh"
#include "BKE_anonymous_attribute_id.hh"
#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_geometry_set.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_modifier.hh"
#include "BKE_multires.hh"
#include "BKE_object.hh"
#include "BKE_object_deform.h"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "ED_object.hh"
#include "ED_object_vgroup.hh"
#include "ED_screen.hh"

#include "UI_interface.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "object_intern.hh"

namespace blender::ed::object {

static CLG_LogRef LOG = {"object"};

static void modifier_skin_customdata_delete(Object *ob);

/* ------------------------------------------------------------------- */
/** \name Public API
 * \{ */

static void object_force_modifier_update_for_bind(Depsgraph *depsgraph, Object *ob)
{
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
  BKE_object_eval_reset(ob_eval);
  if (ob->type == OB_MESH) {
    Mesh *mesh_eval = bke::mesh_create_eval_final(
        depsgraph, scene_eval, ob_eval, &CD_MASK_DERIVEDMESH);
    BKE_id_free(nullptr, mesh_eval);
  }
}

static void object_force_modifier_bind_simple_options(Depsgraph *depsgraph,
                                                      Object *object,
                                                      ModifierData *md)
{
  ModifierData *md_eval = BKE_modifier_get_evaluated(depsgraph, object, md);
  const ModifierMode mode = md_eval->mode;
  md_eval->mode |= eModifierMode_Realtime;
  object_force_modifier_update_for_bind(depsgraph, object);
  md_eval->mode = mode;
}

ModifierData *modifier_add(
    ReportList *reports, Main *bmain, Scene * /*scene*/, Object *ob, const char *name, int type)
{
  ModifierData *new_md = nullptr;
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(type));

  /* Check compatibility of modifier [#25291, #50373]. */
  if (!BKE_object_support_modifier_type_check(ob, type)) {
    BKE_reportf(reports, RPT_WARNING, "Modifiers cannot be added to object '%s'", ob->id.name + 2);
    return nullptr;
  }

  if (mti->flags & eModifierTypeFlag_Single) {
    if (BKE_modifiers_findby_type(ob, ModifierType(type))) {
      BKE_report(reports, RPT_WARNING, "Only one modifier of this type is allowed");
      return nullptr;
    }
  }

  if (type == eModifierType_ParticleSystem) {
    return nullptr;
  }

  /* get new modifier data to add */
  new_md = BKE_modifier_new(type);

  BKE_modifiers_add_at_end_if_possible(ob, new_md);
  BKE_modifiers_persistent_uid_init(*ob, *new_md);

  if (name) {
    STRNCPY_UTF8(new_md->name, name);
  }

  /* make sure modifier data has unique name */
  BKE_modifier_unique_name(&ob->modifiers, new_md);

  if (type == eModifierType_Multires) {
    /* set totlvl from existing MDISPS layer if object already had it */
    multiresModifier_set_levels_from_disps(reinterpret_cast<MultiresModifierData *>(new_md), ob);

    if (ob->mode & OB_MODE_SCULPT) {
      /* ensure that grid paint mask layer is created */
      BKE_sculpt_mask_layers_ensure(
          nullptr, nullptr, ob, reinterpret_cast<MultiresModifierData *>(new_md));
    }
  }
  else if (type == eModifierType_Skin) {
    /* ensure skin-node customdata exists */
    BKE_mesh_ensure_skin_customdata(id_cast<Mesh *>(ob->data));
  }

  BKE_object_modifier_set_active(ob, new_md);

  if (bmain) {
    DEG_id_tag_update_ex(bmain, &ob->id, ID_RECALC_GEOMETRY);
    DEG_relations_tag_update(bmain);
  }

  return new_md;
}

/* Return true if the object has a modifier of type 'type' other than
 * the modifier pointed to be 'exclude', otherwise returns false. */
static bool object_has_modifier(const Object *ob, const ModifierData *exclude, ModifierType type)
{
  for (ModifierData &md : ob->modifiers) {
    if ((&md != exclude) && (md.type == type)) {
      return true;
    }
  }

  return false;
}

bool iter_other(Main *bmain,
                Object *orig_ob,
                const bool include_orig,
                bool (*callback)(Object *ob, void *callback_data),
                void *callback_data)
{
  ID *ob_data_id = orig_ob->data;
  int users = ob_data_id->us;

  if (ob_data_id->flag & ID_FLAG_FAKEUSER) {
    users--;
  }

  /* First check that the object's data has multiple users */
  if (users > 1) {
    Object *ob;
    int totfound = include_orig ? 0 : 1;

    for (ob = bmain->objects.first(); ob && totfound < users;
         ob = reinterpret_cast<Object *>(ob->id.next))
    {
      if (((ob != orig_ob) || include_orig) && (ob->data == orig_ob->data)) {
        if (callback(ob, callback_data)) {
          return true;
        }

        totfound++;
      }
    }
  }
  else if (include_orig) {
    return callback(orig_ob, callback_data);
  }

  return false;
}

static bool object_has_modifier_cb(Object *ob, void *data)
{
  ModifierType type = *(static_cast<ModifierType *>(data));

  return object_has_modifier(ob, nullptr, type);
}

bool multires_update_totlevels(Object *ob, void *totlevel_v)
{
  int totlevel = *(static_cast<char *>(totlevel_v));

  for (ModifierData &md : ob->modifiers) {
    if (md.type == eModifierType_Multires) {
      multires_set_tot_level(ob, reinterpret_cast<MultiresModifierData *>(&md), totlevel);
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
  }
  return false;
}

/* Return true if no modifier of type 'type' other than 'exclude' */
static bool object_modifier_safe_to_delete(Main *bmain,
                                           Object *ob,
                                           ModifierData *exclude,
                                           ModifierType type)
{
  return (!object_has_modifier(ob, exclude, type) &&
          !iter_other(bmain, ob, false, object_has_modifier_cb, &type));
}

static bool object_modifier_remove(
    Main *bmain, Scene * /*scene*/, Object *ob, ModifierData *md, bool *r_sort_depsgraph)
{
  /* It seems on rapid delete it is possible to
   * get called twice on same modifier, so make
   * sure it is in list. */
  if (BLI_findindex(&ob->modifiers, md) == -1) {
    return false;
  }

  if (md->type == eModifierType_Collision) {
    if (ob->pd) {
      ob->pd->deflect = 0;
    }

    *r_sort_depsgraph = true;
  }
  else if (md->type == eModifierType_Surface) {
    *r_sort_depsgraph = true;
  }
  else if (md->type == eModifierType_Multires) {
    /* Delete MDisps layer if not used by another multires modifier */
    if (object_modifier_safe_to_delete(bmain, ob, md, eModifierType_Multires)) {
      multires_customdata_delete(id_cast<Mesh *>(ob->data));
    }
  }
  else if (md->type == eModifierType_Skin) {
    /* Delete skin vertex attributes if not used by another skin modifier */
    if (object_modifier_safe_to_delete(bmain, ob, md, eModifierType_Skin)) {
      modifier_skin_customdata_delete(ob);
    }
  }

  BKE_animdata_drivers_remove_for_rna_struct(ob->id, *RNA_Modifier, md);

  BKE_modifier_remove_from_list(ob, md);
  BKE_modifier_free(md);
  BKE_object_free_derived_caches(ob);

  return true;
}

bool modifier_remove(ReportList *reports, Main *bmain, Scene *scene, Object *ob, ModifierData *md)
{
  bool sort_depsgraph = false;

  bool ok = object_modifier_remove(bmain, scene, ob, md, &sort_depsgraph);

  if (!ok) {
    BKE_reportf(reports, RPT_ERROR, "Modifier '%s' not in object '%s'", md->name, ob->id.name);
    return false;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);

  return true;
}

void modifiers_clear(Main *bmain, Scene *scene, Object *ob)
{
  ModifierData *md = ob->modifiers.first();
  bool sort_depsgraph = false;

  if (!md) {
    return;
  }

  while (md) {
    ModifierData *next_md = md->next;

    object_modifier_remove(bmain, scene, ob, md, &sort_depsgraph);

    md = next_md;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);
}

static bool object_modifier_check_move_before(ReportList *reports,
                                              eReportType error_type,
                                              ModifierData *md,
                                              ModifierData *md_prev)
{
  if (md_prev) {
    if (md->flag & eModifierFlag_PinLast && !(md_prev->flag & eModifierFlag_PinLast)) {
      return false;
    }
    const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);

    if (mti->type != ModifierTypeType::OnlyDeform) {
      const ModifierTypeInfo *nmti = BKE_modifier_get_info(md_prev->type);

      if (nmti->flags & eModifierTypeFlag_RequiresOriginalData) {
        BKE_report(reports, error_type, "Cannot move above a modifier requiring original data");
        return false;
      }
    }
  }
  else {
    BKE_report(reports, error_type, "Cannot move modifier beyond the start of the list");
    return false;
  }

  return true;
}

bool modifier_move_up(ReportList *reports, eReportType error_type, Object *ob, ModifierData *md)
{
  if (object_modifier_check_move_before(reports, error_type, md, md->prev)) {
    BLI_listbase_swaplinks(&ob->modifiers, md, md->prev);
    return true;
  }

  return false;
}

static bool object_modifier_check_move_after(ReportList *reports,
                                             eReportType error_type,
                                             ModifierData *md,
                                             ModifierData *md_next)
{
  if (md_next) {
    if (md_next->flag & eModifierFlag_PinLast && !(md->flag & eModifierFlag_PinLast)) {
      return false;
    }
    const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);

    if (mti->flags & eModifierTypeFlag_RequiresOriginalData) {
      const ModifierTypeInfo *nmti = BKE_modifier_get_info(md_next->type);

      if (nmti->type != ModifierTypeType::OnlyDeform) {
        BKE_report(reports, error_type, "Cannot move beyond a non-deforming modifier");
        return false;
      }
    }
  }
  else {
    BKE_report(reports, error_type, "Cannot move modifier beyond the end of the list");
    return false;
  }

  return true;
}

bool modifier_move_down(ReportList *reports, eReportType error_type, Object *ob, ModifierData *md)
{
  if (object_modifier_check_move_after(reports, error_type, md, md->next)) {
    BLI_listbase_swaplinks(&ob->modifiers, md, md->next);
    return true;
  }

  return false;
}

bool modifier_move_to_index(ReportList *reports,
                            eReportType error_type,
                            Object *ob,
                            ModifierData *md,
                            const int index,
                            bool allow_partial)
{
  BLI_assert(md != nullptr);

  if (index < 0 || index >= ob->modifiers.count()) {
    BKE_report(reports, error_type, "Cannot move modifier beyond the end of the stack");
    return false;
  }

  int md_index = BLI_findindex(&ob->modifiers, md);
  BLI_assert(md_index != -1);

  if (md_index < index) {
    /* Move modifier down in list. */
    ModifierData *md_target = md;

    for (; md_index < index; md_index++, md_target = md_target->next) {
      if (!object_modifier_check_move_after(reports, error_type, md, md_target->next)) {
        if (!allow_partial || md == md_target) {
          return false;
        }

        break;
      }
    }

    BLI_assert(md != md_target && md_target);

    BLI_remlink(&ob->modifiers, md);
    BLI_insertlinkafter(&ob->modifiers, md_target, md);
  }
  else if (md_index > index) {
    /* Move modifier up in list. */
    ModifierData *md_target = md;

    for (; md_index > index; md_index--, md_target = md_target->prev) {
      if (!object_modifier_check_move_before(reports, error_type, md, md_target->prev)) {
        if (!allow_partial || md == md_target) {
          return false;
        }

        break;
      }
    }

    BLI_assert(md != md_target && md_target);

    BLI_remlink(&ob->modifiers, md);
    BLI_insertlinkbefore(&ob->modifiers, md_target, md);
  }
  else {
    return true;
  }

  /* NOTE: Dependency graph only uses modifier nodes for visibility updates, and exact order of
   * modifier nodes in the graph does not matter. */

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);

  return true;
}

void modifier_link(bContext *C, Object *ob_dst, Object *ob_src)
{
  BKE_object_link_modifiers(ob_dst, ob_src);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob_dst);
  DEG_id_tag_update(&ob_dst->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

  Main *bmain = CTX_data_main(C);
  DEG_relations_tag_update(bmain);
}

ModifierData *modifier_copy_to_object(Main *bmain,
                                      const Scene *scene,
                                      const Object *ob_src,
                                      const ModifierData *md,
                                      Object *ob_dst,
                                      ReportList *reports)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);

  BLI_assert(ob_src != ob_dst);

  /* Checked in #BKE_object_copy_modifier, but check here too so we can give a better message. */
  if (!BKE_object_support_modifier_type_check(ob_dst, md->type)) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Object '%s' does not support %s modifiers",
                ob_dst->id.name + 2,
                RPT_(mti->name));
    return nullptr;
  }

  if (mti->flags & eModifierTypeFlag_Single) {
    if (BKE_modifiers_findby_type(ob_dst, md->type)) {
      BKE_reportf(reports,
                  RPT_WARNING,
                  "Modifier can only be added once to object '%s'",
                  ob_dst->id.name + 2);
      return nullptr;
    }
  }

  ModifierData *md_dst = BKE_object_copy_modifier(bmain, scene, ob_dst, ob_src, md);
  if (!md_dst) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Copying modifier '%s' to object '%s' failed",
                md->name,
                ob_dst->id.name + 2);
    return nullptr;
  }

  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_ADDED, ob_dst);
  DEG_id_tag_update(&ob_dst->id, ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
  DEG_relations_tag_update(bmain);
  return md_dst;
}

bool convert_psys_to_mesh(ReportList * /*reports*/,
                          Main * /*bmain*/,
                          Depsgraph * /*depsgraph*/,
                          Scene * /*scene*/,
                          ViewLayer * /*view_layer*/,
                          Object * /*ob*/,
                          ModifierData * /*md*/)
{
  return false;
}

static void add_shapekey_layers(Mesh &mesh_dest, const Mesh &mesh_src)
{
  if (!mesh_src.key) {
    return;
  }

  for (const auto [i, kb] : mesh_src.key->block.enumerate()) {
    void *array;
    if (mesh_src.verts_num != kb.totelem) {
      CLOG_ERROR(&LOG,
                 "vertex size mismatch (Mesh '%s':%d != KeyBlock '%s':%d)",
                 mesh_src.id.name + 2,
                 mesh_src.verts_num,
                 kb.name,
                 kb.totelem);
      array = MEM_new_array_zeroed<float[3]>(mesh_src.verts_num, __func__);
    }
    else {
      array = MEM_new_array_uninitialized<float[3]>(size_t(mesh_src.verts_num), __func__);
      memcpy(array, kb.data, sizeof(float[3]) * size_t(mesh_src.verts_num));
    }

    CustomData_add_layer_with_data(
        &mesh_dest.vert_data, CD_SHAPEKEY, array, mesh_dest.verts_num, nullptr);
    const int ci = CustomData_get_layer_index_n(&mesh_dest.vert_data, CD_SHAPEKEY, i);

    mesh_dest.vert_data.layers[ci].uid = kb.uid;
  }
}

/**
 * \param use_virtual_modifiers: When enabled, calculate virtual-modifiers before applying
 * `md_eval`. This is supported because virtual-modifiers are not modifiers from a user
 * perspective, allowing shape keys to be included with the modifier being applied, see: #91923.
 */
static Mesh *create_applied_mesh_for_modifier(Depsgraph *depsgraph,
                                              Scene *scene,
                                              Object *ob_eval,
                                              ModifierData *md_eval,
                                              const bool use_virtual_modifiers,
                                              const bool build_shapekey_layers,
                                              ReportList *reports)
{
  Mesh *mesh = ob_eval->runtime->data_orig ?
                   reinterpret_cast<Mesh *>(ob_eval->runtime->data_orig) :
                   reinterpret_cast<Mesh *>(ob_eval->data);
  const ModifierTypeInfo *mti = BKE_modifier_get_info(md_eval->type);
  const ModifierEvalContext mectx = {depsgraph, ob_eval, MOD_APPLY_TO_ORIGINAL};

  if (!(md_eval->mode & eModifierMode_Realtime)) {
    return nullptr;
  }

  if (mti->is_disabled && mti->is_disabled(scene, md_eval, false)) {
    return nullptr;
  }

  if (build_shapekey_layers && mesh->key) {
    if (KeyBlock *kb = static_cast<KeyBlock *>(
            BLI_findlink(&mesh->key->block, ob_eval->shapenr - 1)))
    {
      BKE_keyblock_convert_to_mesh(kb, mesh->vert_positions_for_write());
    }
  }

  Mesh *mesh_temp = reinterpret_cast<Mesh *>(
      BKE_id_copy_ex(nullptr, &mesh->id, nullptr, LIB_ID_COPY_LOCALIZE));
  MutableSpan<float3> deformedVerts = mesh_temp->vert_positions_for_write();

  if (use_virtual_modifiers) {
    VirtualModifierData virtual_modifier_data;
    for (ModifierData *md_eval_virt =
             BKE_modifiers_get_virtual_modifierlist(ob_eval, &virtual_modifier_data);
         md_eval_virt && (md_eval_virt != ob_eval->modifiers.first_);
         md_eval_virt = md_eval_virt->next)
    {
      if (!BKE_modifier_is_enabled(scene, md_eval_virt, eModifierMode_Realtime)) {
        continue;
      }
      /* All virtual modifiers are deform modifiers. */
      const ModifierTypeInfo *mti_virt = BKE_modifier_get_info(md_eval_virt->type);
      BLI_assert(mti_virt->type == ModifierTypeType::OnlyDeform);
      if (mti_virt->type != ModifierTypeType::OnlyDeform) {
        continue;
      }

      mti_virt->deform_verts(md_eval_virt, &mectx, mesh_temp, deformedVerts);
    }
  }

  Mesh *result = nullptr;
  if (mti->type == ModifierTypeType::OnlyDeform) {
    result = mesh_temp;
    mti->deform_verts(md_eval, &mectx, result, deformedVerts);
    result->tag_positions_changed();

    if (build_shapekey_layers) {
      add_shapekey_layers(*result, *mesh);
    }
  }
  else {
    if (build_shapekey_layers) {
      add_shapekey_layers(*mesh_temp, *mesh);
    }

    if (mti->modify_geometry_set) {
      bke::GeometrySet geometry_set = bke::GeometrySet::from_mesh(
          mesh_temp, bke::GeometryOwnershipType::Owned);
      mti->modify_geometry_set(md_eval, &mectx, &geometry_set);
      if (!geometry_set.has_mesh()) {
        BKE_report(reports, RPT_ERROR, "Evaluated geometry from modifier does not contain a mesh");
        return nullptr;
      }
      result = geometry_set.get_component_for_write<bke::MeshComponent>().release();
    }
    else {
      result = mti->modify_mesh(md_eval, &mectx, mesh_temp);
      if (mesh_temp != result) {
        BKE_id_free(nullptr, mesh_temp);
      }
    }
  }

  return result;
}

static bool modifier_apply_shape(Main *bmain,
                                 ReportList *reports,
                                 Depsgraph *depsgraph,
                                 Scene *scene,
                                 Object *ob,
                                 ModifierData *md_eval)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(md_eval->type);

  if (mti->is_disabled && mti->is_disabled(scene, md_eval, false)) {
    BKE_report(reports, RPT_ERROR, "Modifier is disabled, skipping apply");
    return false;
  }

  /* We could investigate using the #CD_ORIGINDEX layer
   * to support other kinds of modifiers besides deforming modifiers.
   * as this is done in many other places, see: #BKE_mesh_foreach_mapped_vert_coords_get.
   *
   * This isn't high priority in practice since most modifiers users
   * want to apply as a shape are deforming modifiers.
   *
   * If a compelling use-case comes up where we want to support other kinds of modifiers
   * we can look into supporting them. */

  if (ob->type == OB_MESH) {
    Mesh *mesh = id_cast<Mesh *>(ob->data);
    Key *key = mesh->key;

    if (!BKE_modifier_is_same_topology(md_eval) || mti->type == ModifierTypeType::NonGeometrical) {
      BKE_report(reports, RPT_ERROR, "Only deforming modifiers can be applied to shapes");
      return false;
    }

    Mesh *mesh_applied = create_applied_mesh_for_modifier(depsgraph,
                                                          DEG_get_evaluated_scene(depsgraph),
                                                          DEG_get_evaluated(depsgraph, ob),
                                                          md_eval,
                                                          true,
                                                          false,
                                                          reports);
    if (!mesh_applied) {
      BKE_report(reports, RPT_ERROR, "Modifier is disabled or returned error, skipping apply");
      return false;
    }

    if (key == nullptr) {
      key = mesh->key = BKE_key_add(bmain, id_cast<ID *>(mesh));
      key->type = KEY_RELATIVE;
      /* if that was the first key block added, then it was the basis.
       * Initialize it with the mesh, and add another for the modifier */
      KeyBlock *kb = BKE_keyblock_add(key, nullptr);
      BKE_keyblock_convert_from_mesh(mesh, key, kb);
    }

    KeyBlock *kb = BKE_keyblock_add(key, md_eval->name);
    BKE_mesh_nomain_to_meshkey(mesh_applied, mesh, kb);

    BKE_id_free(nullptr, mesh_applied);
  }
  else {
    BKE_report(reports, RPT_ERROR, "Cannot apply modifier for this object type");
    return false;
  }
  return true;
}

static bool modifier_apply_obdata(ReportList *reports,
                                  Depsgraph *depsgraph,
                                  Scene *scene,
                                  Object *ob,
                                  ModifierData *md_eval,
                                  const bool /*do_all_keyframes*/)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(md_eval->type);

  if (mti->is_disabled && mti->is_disabled(scene, md_eval, false)) {
    BKE_report(reports, RPT_ERROR, "Modifier is disabled, skipping apply");
    return false;
  }

  if (ob->type == OB_MESH) {
    Mesh *mesh = id_cast<Mesh *>(ob->data);
    MultiresModifierData *mmd = find_multires_modifier_before(scene, md_eval);

    if (mesh->key && mti->type != ModifierTypeType::NonGeometrical) {
      BKE_report(reports, RPT_ERROR, "Modifier cannot be applied to a mesh with shape keys");
      return false;
    }

    /* Multires: ensure that recent sculpting is applied */
    if (md_eval->type == eModifierType_Multires) {
      multires_force_sculpt_rebuild(ob);
    }

    if (mmd && mmd->totlvl &&
        (mti->type == ModifierTypeType::OnlyDeform || md_eval->type == eModifierType_Nodes))
    {
      if (!multiresModifier_reshapeFromDeformModifier(depsgraph, ob, mmd, md_eval)) {
        BKE_report(reports, RPT_ERROR, "Multires modifier returned error, skipping apply");
        return false;
      }
    }
    else {
      Mesh *mesh_applied = create_applied_mesh_for_modifier(
          depsgraph,
          DEG_get_evaluated_scene(depsgraph),
          DEG_get_evaluated(depsgraph, ob),
          md_eval,
          false,
          true,
          reports);
      if (!mesh_applied) {
        return false;
      }

      Main *bmain = DEG_get_bmain(depsgraph);
      BKE_object_material_from_eval_data(bmain, ob, &mesh_applied->id);
      BKE_mesh_nomain_to_mesh(mesh_applied, mesh, ob);

      /* Anonymous attributes shouldn't be available on the applied geometry. */
      mesh->attributes_for_write().remove_anonymous();

      /* Remove strings referring to attributes if they no longer exist. */
      bke::mesh_remove_invalid_attribute_strings(*mesh);

      /* Make sure that if there are uv maps, one is marked as active. */
      bke::mesh_ensure_active_uv_map(*mesh);

      if (md_eval->type == eModifierType_Multires) {
        multires_customdata_delete(mesh);
      }
    }
  }
  else {
    BKE_report(reports, RPT_ERROR, "Cannot apply modifier for this object type");
    return false;
  }

  return true;
}

bool modifier_apply(Main *bmain,
                    ReportList *reports,
                    Depsgraph *depsgraph,
                    Scene *scene,
                    Object *ob,
                    ModifierData *md,
                    int mode,
                    bool keep_modifier,
                    const bool do_all_keyframes)
{
  if (BKE_object_is_in_editmode(ob)) {
    BKE_report(reports, RPT_ERROR, "Modifiers cannot be applied in edit mode");
    return false;
  }
  if (mode != MODIFIER_APPLY_SHAPE && ID_REAL_USERS(ob->data) > 1) {
    BKE_report(reports, RPT_ERROR, "Modifiers cannot be applied to multi-user data");
    return false;
  }
  if ((ob->mode & OB_MODE_SCULPT) && find_multires_modifier_before(scene, md) &&
      (BKE_modifier_is_same_topology(md) == false))
  {
    BKE_report(reports,
               RPT_ERROR,
               "Constructive modifier cannot be applied to multi-res data in sculpt mode");
    return false;
  }

  if (md != ob->modifiers.first_) {
    BKE_report(reports, RPT_INFO, "Applied modifier was not first, result may not be as expected");
  }

  /* Get evaluated modifier, so object links pointer to evaluated data,
   * but still use original object it is applied to the original mesh. */
  Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
  ModifierData *md_eval = (ob_eval) ? BKE_modifiers_findby_name(ob_eval, md->name) : md;

  Depsgraph *apply_depsgraph = depsgraph;
  Depsgraph *local_depsgraph = nullptr;

  /* If the object is hidden or the modifier is not enabled for the viewport is disabled a special
   * handling is required. This is because the viewport dependency graph optimizes out evaluation
   * of objects which are used by hidden objects and disabled modifiers.
   *
   * The idea is to create a dependency graph which does not perform those optimizations. */
  if ((ob_eval->base_flag & BASE_ENABLED_VIEWPORT) == 0 ||
      (md_eval->mode & eModifierMode_Realtime) == 0)
  {
    ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);

    local_depsgraph = DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_VIEWPORT);
    DEG_disable_visibility_optimization(local_depsgraph);

    DEG_graph_build_from_ids(local_depsgraph, {&ob->id});
    DEG_evaluate_on_refresh(local_depsgraph);

    apply_depsgraph = local_depsgraph;

    /* The evaluated object and modifier are now from the different dependency graph. */
    ob_eval = DEG_get_evaluated(local_depsgraph, ob);
    md_eval = BKE_modifiers_findby_name(ob_eval, md->name);

    /* Force mode on the evaluated modifier, enforcing the modifier evaluation in the apply()
     * functions. */
    md_eval->mode |= eModifierMode_Realtime;
  }

  bool did_apply = false;
  if (mode == MODIFIER_APPLY_SHAPE) {
    did_apply = modifier_apply_shape(bmain, reports, apply_depsgraph, scene, ob, md_eval);
  }
  else {
    did_apply = modifier_apply_obdata(
        reports, apply_depsgraph, scene, ob, md_eval, do_all_keyframes);
  }

  if (did_apply) {
    if (!keep_modifier) {
      BKE_modifier_remove_from_list(ob, md);
      BKE_modifier_free(md);
    }
    BKE_object_free_derived_caches(ob);
  }

  if (local_depsgraph != nullptr) {
    DEG_graph_free(local_depsgraph);
  }

  return true;
}

bool modifier_copy(
    ReportList * /*reports*/, Main * /*bmain*/, Scene * /*scene*/, Object *ob, ModifierData *md)
{
  if (md->type == eModifierType_ParticleSystem) {
    return false;
  }

  ModifierData *nmd = BKE_modifier_new(md->type);
  BKE_modifier_copydata(md, nmd);
  BLI_insertlinkafter(&ob->modifiers, md, nmd);
  STRNCPY_UTF8(nmd->name, md->name);
  BKE_modifier_unique_name(&ob->modifiers, nmd);
  BKE_modifiers_persistent_uid_init(*ob, *nmd);
  BKE_object_modifier_set_active(ob, nmd);

  nmd->flag |= eModifierFlag_OverrideLibrary_Local;

  return true;
}

/** \} */

Vector<PointerRNA> modifier_get_edit_objects(const bContext &C, const wmOperator &op)
{
  Vector<PointerRNA> objects;
  if (RNA_boolean_get(op.ptr, "use_selected_objects")) {
    CTX_data_selected_editable_objects(&C, &objects);
  }
  else {
    if (Object *object = context_active_object(&C)) {
      objects.append(RNA_id_pointer_create(&object->id));
    }
  }
  return objects;
}

void modifier_register_use_selected_objects_prop(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna,
      "use_selected_objects",
      false,
      "Selected Objects",
      "Affect all selected objects instead of just the active object");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/* ------------------------------------------------------------------- */
/** \name Add Modifier Operator
 * \{ */

static wmOperatorStatus modifier_add_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  int type = RNA_enum_get(op->ptr, "type");

  bool changed = false;
  for (const PointerRNA &ptr : modifier_get_edit_objects(*C, *op)) {
    Object *ob = static_cast<Object *>(ptr.data);
    if (!modifier_add(op->reports, bmain, scene, ob, nullptr, type)) {
      continue;
    }
    changed = true;
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER | NA_ADDED, ob);
  }
  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_add_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->modifier & KM_ALT || CTX_wm_view3d(C)) {
    RNA_boolean_set(op->ptr, "use_selected_objects", true);
  }
  if (!RNA_struct_property_is_set(op->ptr, "type")) {
    return WM_menu_invoke(C, op, event);
  }
  return modifier_add_exec(C, op);
}

static const EnumPropertyItem *modifier_add_itemf(bContext *C,
                                                  PointerRNA * /*ptr*/,
                                                  PropertyRNA * /*prop*/,
                                                  bool *r_free)
{
  Object *ob = (C) ? context_active_object(C) : nullptr;

  if (!ob) {
    return rna_enum_object_modifier_type_items;
  }

  EnumPropertyItem *items = nullptr;
  int totitem = 0;

  const EnumPropertyItem *group_item = nullptr;
  for (int a = 0; rna_enum_object_modifier_type_items[a].identifier; a++) {
    const EnumPropertyItem *md_item = &rna_enum_object_modifier_type_items[a];

    if (md_item->identifier[0]) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md_item->value));

      if (mti->flags & eModifierTypeFlag_NoUserAdd) {
        continue;
      }

      if (!BKE_object_support_modifier_type_check(ob, md_item->value)) {
        continue;
      }
    }
    else {
      group_item = md_item;
      continue;
    }

    if (group_item) {
      RNA_enum_item_add(&items, &totitem, group_item);
      group_item = nullptr;
    }

    RNA_enum_item_add(&items, &totitem, md_item);
  }

  RNA_enum_item_end(&items, &totitem);
  *r_free = true;

  return items;
}

void OBJECT_OT_modifier_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Modifier";
  ot->description = "Add a procedural operation/effect to the active object";
  ot->idname = "OBJECT_OT_modifier_add";

  /* API callbacks. */
  ot->invoke = modifier_add_invoke;
  ot->exec = modifier_add_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(
      ot->srna, "type", rna_enum_object_modifier_type_items, eModifierType_Subsurf, "Type", "");
  RNA_def_enum_funcs(prop, modifier_add_itemf);
  ot->prop = prop;
  modifier_register_use_selected_objects_prop(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Generic Poll Function and Properties
 *
 * Using modifier names and data context.
 * \{ */

static PointerRNA edit_modifier_ptr_get(bContext *C, StructRNA *rna_type)
{
  return CTX_data_pointer_get_type(C, "modifier", rna_type);
}

static Object *edit_modifier_object_get(bContext *C, const PointerRNA &ptr)
{
  return (ptr.owner_id != nullptr) ? id_cast<Object *>(ptr.owner_id) : context_active_object(C);
}

bool edit_modifier_poll_generic(bContext *C,
                                StructRNA *rna_type,
                                int obtype_flag,
                                const bool is_editmode_allowed,
                                const bool is_liboverride_allowed)
{
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr = edit_modifier_ptr_get(C, rna_type);
  Object *ob = edit_modifier_object_get(C, ptr);
  ModifierData *mod = static_cast<ModifierData *>(ptr.data); /* May be nullptr. */

  if (mod == nullptr && ob != nullptr) {
    mod = BKE_object_active_modifier(ob);
  }

  if (!ob || !BKE_id_is_editable(bmain, &ob->id)) {
    return false;
  }
  if (obtype_flag && ((1 << ob->type) & obtype_flag) == 0) {
    return false;
  }
  if (ptr.owner_id && !BKE_id_is_editable(bmain, ptr.owner_id)) {
    return false;
  }

  if (!is_liboverride_allowed && BKE_modifier_is_nonlocal_in_liboverride(ob, mod)) {
    CTX_wm_operator_poll_msg_set(
        C, "Cannot edit modifiers coming from linked data in a library override");
    return false;
  }

  if (!is_editmode_allowed && CTX_data_edit_object(C) != nullptr) {
    CTX_wm_operator_poll_msg_set(C, "This modifier operation is not allowed from Edit mode");
    return false;
  }

  return true;
}

static bool edit_modifier_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, RNA_Modifier, 0, true, false);
}

/* Used by operators performing actions allowed also on modifiers from the overridden linked object
 * (not only from added 'local' ones). */
static bool edit_modifier_liboverride_allowed_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, RNA_Modifier, 0, true, true);
}

void edit_modifier_properties(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_string(
      ot->srna, "modifier", nullptr, MAX_NAME, "Modifier", "Name of the modifier to edit");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

static void edit_modifier_report_property(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "report", false, "Report", "Create a notification after the operation");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Generic Invoke Functions
 *
 * Using modifier names and data context.
 * \{ */

bool edit_modifier_invoke_properties(bContext *C, wmOperator *op)
{
  if (RNA_struct_property_is_set(op->ptr, "modifier")) {
    return true;
  }

  PointerRNA ctx_ptr = CTX_data_pointer_get_type(C, "modifier", RNA_Modifier);
  if (ctx_ptr) {
    ModifierData *md = static_cast<ModifierData *>(ctx_ptr.data);
    RNA_string_set(op->ptr, "modifier", md->name);
    return true;
  }

  return false;
}

/**
 * If the "modifier" property is not set, fill the modifier property with the name of the modifier
 * with a UI panel below the mouse cursor, unless a specific modifier is set with a context
 * pointer. Used in order to apply modifier operators on hover over their panels.
 */
static bool edit_modifier_invoke_properties_with_hover(bContext *C,
                                                       wmOperator *op,
                                                       const wmEvent *event,
                                                       wmOperatorStatus *r_retval)
{
  if (RNA_struct_find_property(op->ptr, "use_selected_objects")) {
    if (event->modifier & KM_ALT) {
      RNA_boolean_set(op->ptr, "use_selected_objects", true);
    }
  }

  if (RNA_struct_property_is_set(op->ptr, "modifier")) {
    return true;
  }

  /* Note that the context pointer is *not* the active modifier, it is set in UI layouts. */
  PointerRNA ctx_ptr = CTX_data_pointer_get_type(C, "modifier", RNA_Modifier);
  if (ctx_ptr) {
    ModifierData *md = static_cast<ModifierData *>(ctx_ptr.data);
    RNA_string_set(op->ptr, "modifier", md->name);
    return true;
  }

  PointerRNA *panel_ptr = ui::region_panel_custom_data_under_cursor(C, event);
  if (panel_ptr == nullptr || !*panel_ptr) {
    /* The operators using this function can typically be called from UIs that aren't related to
     * the modifiers UI at all. So include #OPERATOR_PASS_THROUGH to not block events from reaching
     * other operators/handlers. */
    *r_retval = (OPERATOR_PASS_THROUGH | OPERATOR_CANCELLED);
    return false;
  }

  if (!RNA_struct_is_a(panel_ptr->type, RNA_Modifier)) {
    /* Work around multiple operators using the same shortcut. The operators for the other
     * stacks in the property editor use the same key, and will not run after these return
     * OPERATOR_CANCELLED. */
    *r_retval = (OPERATOR_PASS_THROUGH | OPERATOR_CANCELLED);
    return false;
  }

  const ModifierData *md = static_cast<const ModifierData *>(panel_ptr->data);
  RNA_string_set(op->ptr, "modifier", md->name);
  return true;
}

ModifierData *edit_modifier_property_get(wmOperator *op, Object *ob, int type)
{
  char modifier_name[MAX_NAME];
  RNA_string_get(op->ptr, "modifier", modifier_name);

  ModifierData *md = BKE_modifiers_findby_name(ob, modifier_name);

  if (md && type != 0 && md->type != type) {
    md = nullptr;
  }

  return md;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Remove Modifier Operator
 * \{ */

static wmOperatorStatus modifier_remove_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  char name[MAX_NAME];
  RNA_string_get(op->ptr, "modifier", name);

  bool changed = false;
  for (const PointerRNA &ptr : modifier_get_edit_objects(*C, *op)) {
    Object *ob = static_cast<Object *>(ptr.data);
    ModifierData *md = BKE_modifiers_findby_name(ob, name);
    if (md == nullptr) {
      continue;
    }

    int mode_orig = ob->mode;
    if (!modifier_remove(op->reports, bmain, scene, ob, md)) {
      continue;
    }

    changed = true;

    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER | NA_REMOVED, ob);

    /* if cloth/softbody was removed, particle mode could be cleared */
    if (mode_orig & OB_MODE_PARTICLE_EDIT) {
      if ((ob->mode & OB_MODE_PARTICLE_EDIT) == 0) {
        BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
        if (ob == BKE_view_layer_active_object_get(view_layer)) {
          WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_OBJECT, nullptr);
        }
      }
    }
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "report")) {
    BKE_reportf(op->reports, RPT_INFO, "Removed modifier: %s", name);
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_remove_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    return modifier_remove_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_modifier_remove(wmOperatorType *ot)
{
  ot->name = "Remove Modifier";
  ot->description = "Remove a modifier from the active object";
  ot->idname = "OBJECT_OT_modifier_remove";

  ot->invoke = modifier_remove_invoke;
  ot->exec = modifier_remove_exec;
  ot->poll = edit_modifier_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
  edit_modifier_report_property(ot);
  modifier_register_use_selected_objects_prop(ot);
}

static wmOperatorStatus modifiers_clear_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  CTX_DATA_BEGIN (C, Object *, object, selected_editable_objects) {
    modifiers_clear(bmain, scene, object);
    WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_REMOVED, object);
  }
  CTX_DATA_END;

  return OPERATOR_FINISHED;
}

static bool modifiers_clear_poll(bContext *C)
{
  if (!ED_operator_object_active_local_editable(C)) {
    return false;
  }
  const Object *object = context_active_object(C);
  if (!BKE_object_supports_modifiers(object)) {
    return false;
  }
  return true;
}

void OBJECT_OT_modifiers_clear(wmOperatorType *ot)
{
  ot->name = "Clear Object Modifiers";
  ot->description = "Clear all modifiers from the selected objects";
  ot->idname = "OBJECT_OT_modifiers_clear";

  ot->exec = modifiers_clear_exec;
  ot->poll = modifiers_clear_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Move Up Modifier Operator
 * \{ */

static wmOperatorStatus modifier_move_up_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  ModifierData *md = edit_modifier_property_get(op, ob, 0);

  if (!md || !modifier_move_up(op->reports, RPT_WARNING, ob, md)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_move_up_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    return modifier_move_up_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_modifier_move_up(wmOperatorType *ot)
{
  ot->name = "Move Up Modifier";
  ot->description = "Move modifier up in the stack";
  ot->idname = "OBJECT_OT_modifier_move_up";

  ot->invoke = modifier_move_up_invoke;
  ot->exec = modifier_move_up_exec;
  ot->poll = edit_modifier_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Move Down Modifier Operator
 * \{ */

static wmOperatorStatus modifier_move_down_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  ModifierData *md = edit_modifier_property_get(op, ob, 0);

  if (!md || !modifier_move_down(op->reports, RPT_WARNING, ob, md)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_move_down_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    return modifier_move_down_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_modifier_move_down(wmOperatorType *ot)
{
  ot->name = "Move Down Modifier";
  ot->description = "Move modifier down in the stack";
  ot->idname = "OBJECT_OT_modifier_move_down";

  ot->invoke = modifier_move_down_invoke;
  ot->exec = modifier_move_down_exec;
  ot->poll = edit_modifier_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Move to Index Modifier Operator
 * \{ */

static wmOperatorStatus modifier_move_to_index_exec(bContext *C, wmOperator *op)
{
  char name[MAX_NAME];
  RNA_string_get(op->ptr, "modifier", name);

  const int index = RNA_int_get(op->ptr, "index");

  bool changed = false;
  for (const PointerRNA &ptr : modifier_get_edit_objects(*C, *op)) {
    Object *ob = static_cast<Object *>(ptr.data);
    ModifierData *md = BKE_modifiers_findby_name(ob, name);
    if (!md) {
      continue;
    }

    if (!modifier_move_to_index(op->reports, RPT_WARNING, ob, md, index, true)) {
      continue;
    }
    changed = true;
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_move_to_index_invoke(bContext *C,
                                                      wmOperator *op,
                                                      const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    return modifier_move_to_index_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_modifier_move_to_index(wmOperatorType *ot)
{
  ot->name = "Move Active Modifier to Index";
  ot->description =
      "Change the modifier's index in the stack so it evaluates after the set number of others";
  ot->idname = "OBJECT_OT_modifier_move_to_index";

  ot->invoke = modifier_move_to_index_invoke;
  ot->exec = modifier_move_to_index_exec;
  ot->poll = edit_modifier_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
  RNA_def_int(
      ot->srna, "index", 0, 0, INT_MAX, "Index", "The index to move the modifier to", 0, INT_MAX);
  modifier_register_use_selected_objects_prop(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Apply Modifier Operator
 * \{ */

static bool modifier_apply_poll(bContext *C)
{
  if (!edit_modifier_poll_generic(C, RNA_Modifier, 0, false, false)) {
    return false;
  }

  Scene *scene = CTX_data_scene(C);
  PointerRNA ptr = edit_modifier_ptr_get(C, RNA_Modifier);
  Object *ob = edit_modifier_object_get(C, ptr);
  ModifierData *md = static_cast<ModifierData *>(ptr.data); /* May be nullptr. */
  if (ob->type == OB_EMPTY) {
    CTX_wm_operator_poll_msg_set(C, "Modifiers cannot be applied on empty object type");
    return false;
  }

  if (ID_IS_OVERRIDE_LIBRARY(ob) || ((ob->data != nullptr) && ID_IS_OVERRIDE_LIBRARY(ob->data))) {
    CTX_wm_operator_poll_msg_set(C, "Modifiers cannot be applied on override data");
    return false;
  }
  if (md != nullptr) {
    if ((ob->mode & OB_MODE_SCULPT) && find_multires_modifier_before(scene, md) &&
        (BKE_modifier_is_same_topology(md) == false))
    {
      CTX_wm_operator_poll_msg_set(
          C, "Constructive modifier cannot be applied to multi-res data in sculpt mode");
      return false;
    }
  }
  return true;
}

static wmOperatorStatus modifier_apply_exec_ex(bContext *C,
                                               wmOperator *op,
                                               int apply_as,
                                               bool keep_modifier)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Vector<PointerRNA> objects = modifier_get_edit_objects(*C, *op);

  char name[MAX_NAME];
  RNA_string_get(op->ptr, "modifier", name);

  const bool do_report = RNA_boolean_get(op->ptr, "report");
  const int reports_len = do_report ? op->reports->list.count() : 0;

  const bool do_single_user = (apply_as == MODIFIER_APPLY_DATA) ?
                                  RNA_boolean_get(op->ptr, "single_user") :
                                  false;
  const bool do_merge_customdata = (apply_as == MODIFIER_APPLY_DATA) ?
                                       RNA_boolean_get(op->ptr, "merge_customdata") :
                                       false;
  const bool do_all_keyframes = (apply_as == MODIFIER_APPLY_DATA) ?
                                    RNA_boolean_get(op->ptr, "all_keyframes") :
                                    false;

  bool changed = false;
  for (const PointerRNA &ptr : objects) {
    Object *ob = static_cast<Object *>(ptr.data);
    ModifierData *md = BKE_modifiers_findby_name(ob, name);
    if (md == nullptr) {
      continue;
    }

    const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);

    if (do_single_user && ID_REAL_USERS(ob->data) > 1) {
      single_obdata_user_make(bmain, scene, ob);
      BKE_main_id_newptr_and_tag_clear(bmain);
      WM_event_add_notifier(C, NC_WINDOW, nullptr);
      DEG_relations_tag_update(bmain);
    }

    if (!modifier_apply(bmain,
                        op->reports,
                        depsgraph,
                        scene,
                        ob,
                        md,
                        apply_as,
                        keep_modifier,
                        do_all_keyframes))
    {
      continue;
    }
    changed = true;

    if (ob->type == OB_MESH && do_merge_customdata &&
        ELEM(mti->type, ModifierTypeType::Constructive, ModifierTypeType::Nonconstructive))
    {
      BKE_mesh_merge_customdata_for_apply_modifier(id_cast<Mesh *>(ob->data));
    }

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    DEG_relations_tag_update(bmain);
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  if (do_report) {
    /* Only add this report if the operator didn't cause another one. The purpose here is
     * to alert that something happened, and the previous report will do that anyway. */
    if (op->reports->list.count() == reports_len) {
      BKE_reportf(op->reports, RPT_INFO, "Applied modifier: %s", name);
    }
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_apply_exec(bContext *C, wmOperator *op)
{
  return modifier_apply_exec_ex(C, op, MODIFIER_APPLY_DATA, false);
}

static wmOperatorStatus modifier_apply_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    PointerRNA ptr = edit_modifier_ptr_get(C, RNA_Modifier);
    Object *ob = edit_modifier_object_get(C, ptr);

    if ((ob->data != nullptr) && ID_REAL_USERS(ob->data) > 1) {
      PropertyRNA *prop = RNA_struct_find_property(op->ptr, "single_user");
      if (!RNA_property_is_set(op->ptr, prop)) {
        RNA_property_boolean_set(op->ptr, prop, true);
      }
      if (RNA_property_boolean_get(op->ptr, prop)) {
        return WM_operator_confirm_ex(
            C,
            op,
            IFACE_("Apply Modifier"),
            IFACE_("Make data single-user, apply modifier, and remove it from the list."),
            IFACE_("Apply"),
            ui::AlertIcon::Warning,
            false);
      }
    }
    return modifier_apply_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_modifier_apply(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Apply Modifier";
  ot->description = "Apply modifier and remove from the stack";
  ot->idname = "OBJECT_OT_modifier_apply";

  ot->invoke = modifier_apply_invoke;
  ot->exec = modifier_apply_exec;
  ot->poll = modifier_apply_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  edit_modifier_properties(ot);
  edit_modifier_report_property(ot);

  RNA_def_boolean(ot->srna,
                  "merge_customdata",
                  true,
                  "Merge UVs",
                  "For mesh objects, merge UV coordinates that share a vertex to account for "
                  "imprecision in some modifiers");
  prop = RNA_def_boolean(ot->srna,
                         "single_user",
                         false,
                         "Make Data Single User",
                         "Make the object's data single user if needed");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "all_keyframes",
                         false,
                         "Apply to all keyframes",
                         "For Grease Pencil objects, apply the modifier to all the keyframes");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  modifier_register_use_selected_objects_prop(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Apply Modifier As Shape-Key Operator
 * \{ */

static bool modifier_apply_as_shapekey_poll(bContext *C)
{
  return modifier_apply_poll(C);
}

static wmOperatorStatus modifier_apply_as_shapekey_exec(bContext *C, wmOperator *op)
{
  bool keep = RNA_boolean_get(op->ptr, "keep_modifier");

  return modifier_apply_exec_ex(C, op, MODIFIER_APPLY_SHAPE, keep);
}

static wmOperatorStatus modifier_apply_as_shapekey_invoke(bContext *C,
                                                          wmOperator *op,
                                                          const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    return modifier_apply_as_shapekey_exec(C, op);
  }
  return retval;
}

static std::string modifier_apply_as_shapekey_get_description(bContext * /*C*/,
                                                              wmOperatorType * /*ot*/,
                                                              PointerRNA *ptr)
{
  bool keep = RNA_boolean_get(ptr, "keep_modifier");
  if (keep) {
    return TIP_("Apply modifier as a new shapekey and keep it in the stack");
  }

  return "";
}

void OBJECT_OT_modifier_apply_as_shapekey(wmOperatorType *ot)
{
  ot->name = "Apply Modifier as Shape Key";
  ot->description = "Apply modifier as a new shape key and remove from the stack";
  ot->idname = "OBJECT_OT_modifier_apply_as_shapekey";

  ot->invoke = modifier_apply_as_shapekey_invoke;
  ot->exec = modifier_apply_as_shapekey_exec;
  ot->poll = modifier_apply_as_shapekey_poll;
  ot->get_description = modifier_apply_as_shapekey_get_description;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  RNA_def_boolean(
      ot->srna, "keep_modifier", false, "Keep Modifier", "Do not remove the modifier from stack");
  edit_modifier_properties(ot);
  edit_modifier_report_property(ot);
  modifier_register_use_selected_objects_prop(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Convert Particle System Modifier to Mesh Operator
 * \{ */

static wmOperatorStatus modifier_convert_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = context_active_object(C);
  ModifierData *md = edit_modifier_property_get(op, ob, 0);

  if (!md || !convert_psys_to_mesh(op->reports, bmain, depsgraph, scene, view_layer, ob, md)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_convert_invoke(bContext *C,
                                                wmOperator *op,
                                                const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return modifier_convert_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_modifier_convert(wmOperatorType *ot)
{
  ot->name = "Convert Particles to Mesh";
  ot->description = "Convert particles to a mesh object";
  ot->idname = "OBJECT_OT_modifier_convert";

  ot->invoke = modifier_convert_invoke;
  ot->exec = modifier_convert_exec;
  ot->poll = edit_modifier_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Copy Modifier Operator
 * \{ */

static wmOperatorStatus modifier_copy_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  char name[MAX_NAME];
  RNA_string_get(op->ptr, "modifier", name);

  bool changed = false;
  for (const PointerRNA &ptr : modifier_get_edit_objects(*C, *op)) {
    Object *ob = static_cast<Object *>(ptr.data);
    ModifierData *md = BKE_modifiers_findby_name(ob, name);
    if (!md) {
      continue;
    }

    if (!modifier_copy(op->reports, bmain, scene, ob, md)) {
      continue;
    }
    changed = true;
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    DEG_relations_tag_update(bmain);
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER | NA_ADDED, ob);
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_copy_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    return modifier_copy_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_modifier_copy(wmOperatorType *ot)
{
  ot->name = "Copy Modifier";
  ot->description = "Duplicate modifier at the same position in the stack";
  ot->idname = "OBJECT_OT_modifier_copy";

  ot->invoke = modifier_copy_invoke;
  ot->exec = modifier_copy_exec;
  ot->poll = edit_modifier_liboverride_allowed_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
  modifier_register_use_selected_objects_prop(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Set Active Modifier Operator
 * \{ */

static wmOperatorStatus modifier_set_active_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  ModifierData *md = edit_modifier_property_get(op, ob, 0);

  /* If there is no modifier set for this operator, clear the active modifier field. */
  BKE_object_modifier_set_active(ob, md);

  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_set_active_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    return modifier_set_active_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_modifier_set_active(wmOperatorType *ot)
{
  ot->name = "Set Active Modifier";
  ot->description = "Activate the modifier to use as the context";
  ot->idname = "OBJECT_OT_modifier_set_active";

  ot->invoke = modifier_set_active_invoke;
  ot->exec = modifier_set_active_exec;
  ot->poll = ED_operator_object_active_only;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Copy Modifier To Selected Operator
 * \{ */

static wmOperatorStatus modifier_copy_to_selected_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  Object *obact = context_active_object(C);
  ModifierData *md = edit_modifier_property_get(op, obact, 0);
  if (!md) {
    return OPERATOR_CANCELLED;
  }

  int num_copied = 0;

  Vector<PointerRNA> selected_objects;
  CTX_data_selected_objects(C, &selected_objects);
  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    if (ob == obact) {
      continue;
    }
    if (!ID_IS_EDITABLE(ob)) {
      continue;
    }
    ModifierData *md_dst = modifier_copy_to_object(bmain, scene, obact, md, ob, op->reports);
    if (md_dst) {
      BKE_object_modifier_set_active(ob, md_dst);
      WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER | NA_ADDED, ob);
      num_copied++;
    }
  }
  CTX_DATA_END;

  if (num_copied > 0) {
    DEG_relations_tag_update(bmain);
  }
  else {
    BKE_reportf(op->reports, RPT_ERROR, "Modifier '%s' was not copied to any objects", md->name);
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_copy_to_selected_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    return modifier_copy_to_selected_exec(C, op);
  }
  return retval;
}

static bool modifier_copy_to_selected_poll(bContext *C)
{
  PointerRNA ptr = edit_modifier_ptr_get(C, RNA_Modifier);
  Object *obact = edit_modifier_object_get(C, ptr);
  ModifierData *md = static_cast<ModifierData *>(ptr.data);

  /* This just mirrors the check in #BKE_object_copy_modifier,
   * but there is no reasoning for it there. */
  if (md && ELEM(md->type, eModifierType_Hook, eModifierType_Collision)) {
    CTX_wm_operator_poll_msg_set(C, R"(Not supported for "Collision" or "Hook" modifiers)");
    return false;
  }

  if (!obact) {
    CTX_wm_operator_poll_msg_set(C, "No selected object is active");
    return false;
  }

  if (!BKE_object_supports_modifiers(obact)) {
    CTX_wm_operator_poll_msg_set(C, "Object type of source object is not supported");
    return false;
  }

  /* This could have a performance impact in the worst case, where there are many objects selected
   * and none of them pass either of the checks. But that should be uncommon, and this operator is
   * only exposed in a drop-down menu anyway. */
  bool found_supported_objects = false;
  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    if (ob == obact) {
      continue;
    }

    if (!md) {
      /* Skip type check if modifier could not be found ("modifier" context variable not set). */
      if (BKE_object_supports_modifiers(ob)) {
        found_supported_objects = true;
        break;
      }
    }
    else if (BKE_object_support_modifier_type_check(ob, md->type)) {
      found_supported_objects = true;
      break;
    }
  }
  CTX_DATA_END;

  if (!found_supported_objects) {
    CTX_wm_operator_poll_msg_set(C, "No supported objects were selected");
    return false;
  }
  return true;
}

void OBJECT_OT_modifier_copy_to_selected(wmOperatorType *ot)
{
  ot->name = "Copy Modifier to Selected";
  ot->description = "Copy the modifier from the active object to all selected objects";
  ot->idname = "OBJECT_OT_modifier_copy_to_selected";

  ot->invoke = modifier_copy_to_selected_invoke;
  ot->exec = modifier_copy_to_selected_exec;
  ot->poll = modifier_copy_to_selected_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

static wmOperatorStatus object_modifiers_copy_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  Object *active_object = context_active_object(C);

  Vector<PointerRNA> selected_objects;
  CTX_data_selected_objects(C, &selected_objects);
  CTX_DATA_BEGIN (C, Object *, object, selected_objects) {
    if (object == active_object) {
      continue;
    }
    for (const ModifierData &md : active_object->modifiers) {
      ModifierData *md_dst = modifier_copy_to_object(
          bmain, scene, active_object, &md, object, op->reports);
      if (md_dst) {
        WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER | NA_ADDED, object);
        if (md.flag & eModifierFlag_Active) {
          BKE_object_modifier_set_active(object, md_dst);
        }
      }
    }
  }
  CTX_DATA_END;

  return OPERATOR_FINISHED;

  DEG_relations_tag_update(bmain);

  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER | NA_ADDED, nullptr);

  return OPERATOR_FINISHED;
}

static bool modifiers_copy_to_selected_poll(bContext *C)
{
  if (!ED_operator_object_active_editable(C)) {
    return false;
  }
  const Object *active_object = context_active_object(C);
  if (!BKE_object_supports_modifiers(active_object)) {
    return false;
  }
  if (active_object->modifiers.is_empty()) {
    CTX_wm_operator_poll_msg_set(C, "Active object has no modifiers");
    return false;
  }
  return true;
}

void OBJECT_OT_modifiers_copy_to_selected(wmOperatorType *ot)
{
  ot->name = "Copy Modifiers to Selected Objects";
  ot->idname = "OBJECT_OT_modifiers_copy_to_selected";
  ot->description = "Copy modifiers to other selected objects";

  ot->exec = object_modifiers_copy_exec;
  ot->poll = modifiers_copy_to_selected_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Skin Modifier
 * \{ */

static void modifier_skin_customdata_delete(Object *ob)
{
  Mesh *mesh = id_cast<Mesh *>(ob->data);
  if (BMesh *bm = BKE_editmesh_bmesh_get_for_write(mesh)) {
    BM_data_layer_free_named(bm, &bm->vdata, "skin_modifier_radius");
    BM_data_layer_free_named(bm, &bm->vdata, "skin_modifier_root");
    BM_data_layer_free_named(bm, &bm->vdata, "skin_modifier_loose");
  }
  else {
    mesh->attributes_for_write().remove("skin_modifier_radius");
    mesh->attributes_for_write().remove("skin_modifier_root");
    mesh->attributes_for_write().remove("skin_modifier_loose");
  }
}

static bool skin_edit_poll(bContext *C)
{
  /* Resolve the object the same way the exec functions do (#edit_modifier_object_get),
   * so the poll validates the object that will actually be edited. */
  PointerRNA ptr = edit_modifier_ptr_get(C, RNA_SkinModifier);
  Object *ob = edit_modifier_object_get(C, ptr);
  return (ob != nullptr && BKE_object_is_in_editmode(ob) &&
          edit_modifier_poll_generic(C, RNA_SkinModifier, (1 << OB_MESH), true, false) &&
          !ID_IS_OVERRIDE_LIBRARY(ob) && !ID_IS_OVERRIDE_LIBRARY(ob->data));
}

static void skin_root_clear(BMVert *bm_vert, Set<BMVert *> &visited, const int cd_skin_root_offset)
{
  BMEdge *bm_edge;
  BMIter bm_iter;

  BM_ITER_ELEM (bm_edge, &bm_iter, bm_vert, BM_EDGES_OF_VERT) {
    BMVert *v2 = BM_edge_other_vert(bm_edge, bm_vert);

    if (visited.add(v2)) {
      /* clear vertex root flag and add to visited set */
      BM_ELEM_CD_SET_BOOL(v2, cd_skin_root_offset, false);

      skin_root_clear(v2, visited, cd_skin_root_offset);
    }
  }
}

static wmOperatorStatus skin_root_mark_exec(bContext *C, wmOperator * /*op*/)
{
  PointerRNA ptr = edit_modifier_ptr_get(C, RNA_SkinModifier);
  Object *ob = edit_modifier_object_get(C, ptr);
  BMesh *bm = BKE_editmesh_bmesh_ensure(ob);

  Set<BMVert *> visited;

  BKE_mesh_ensure_skin_customdata(id_cast<Mesh *>(ob->data));

  const int cd_skin_root_offset = CustomData_get_offset_named(
      &bm->vdata, CD_PROP_BOOL, "skin_modifier_root");

  BMVert *bm_vert;
  BMIter bm_iter;
  BM_ITER_MESH (bm_vert, &bm_iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(bm_vert, BM_ELEM_SELECT) && visited.add(bm_vert)) {
      /* mark vertex as root and add to visited set */
      BM_ELEM_CD_SET_BOOL(bm_vert, cd_skin_root_offset, true);

      /* clear root flag from all connected vertices (recursively) */
      skin_root_clear(bm_vert, visited, cd_skin_root_offset);
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_skin_root_mark(wmOperatorType *ot)
{
  ot->name = "Skin Root Mark";
  ot->description = "Mark selected vertices as roots";
  ot->idname = "OBJECT_OT_skin_root_mark";

  ot->poll = skin_edit_poll;
  ot->exec = skin_root_mark_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

enum SkinLooseAction {
  SKIN_LOOSE_MARK,
  SKIN_LOOSE_CLEAR,
};

static wmOperatorStatus skin_loose_mark_clear_exec(bContext *C, wmOperator *op)
{
  PointerRNA ptr = edit_modifier_ptr_get(C, RNA_SkinModifier);
  Object *ob = edit_modifier_object_get(C, ptr);
  BMesh *bm = BKE_editmesh_bmesh_ensure(ob);
  SkinLooseAction action = static_cast<SkinLooseAction>(RNA_enum_get(op->ptr, "action"));

  if (!CustomData_has_layer_named(&bm->vdata, CD_PROP_FLOAT2, "skin_modifier_radius")) {
    return OPERATOR_CANCELLED;
  }

  BM_data_layer_ensure_named(bm, &bm->vdata, CD_PROP_BOOL, "skin_modifier_loose");
  const int cd_skin_loose_offset = CustomData_get_offset_named(
      &bm->vdata, CD_PROP_BOOL, "skin_modifier_loose");

  BMVert *bm_vert;
  BMIter bm_iter;
  BM_ITER_MESH (bm_vert, &bm_iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(bm_vert, BM_ELEM_SELECT)) {
      switch (action) {
        case SKIN_LOOSE_MARK:
          BM_ELEM_CD_SET_BOOL(bm_vert, cd_skin_loose_offset, true);
          break;
        case SKIN_LOOSE_CLEAR:
          BM_ELEM_CD_SET_BOOL(bm_vert, cd_skin_loose_offset, false);
          break;
      }
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_skin_loose_mark_clear(wmOperatorType *ot)
{
  static const EnumPropertyItem action_items[] = {
      {SKIN_LOOSE_MARK, "MARK", 0, "Mark", "Mark selected vertices as loose"},
      {SKIN_LOOSE_CLEAR, "CLEAR", 0, "Clear", "Set selected vertices as not loose"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  ot->name = "Skin Mark/Clear Loose";
  ot->description = "Mark/clear selected vertices as loose";
  ot->idname = "OBJECT_OT_skin_loose_mark_clear";

  ot->poll = skin_edit_poll;
  ot->exec = skin_loose_mark_clear_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna, "action", action_items, SKIN_LOOSE_MARK, "Action", nullptr);
}

static wmOperatorStatus skin_radii_equalize_exec(bContext *C, wmOperator * /*op*/)
{
  PointerRNA ptr = edit_modifier_ptr_get(C, RNA_SkinModifier);
  Object *ob = edit_modifier_object_get(C, ptr);
  BMesh *bm = BKE_editmesh_bmesh_ensure(ob);

  const int cd_skin_radius_offset = CustomData_get_offset_named(
      &bm->vdata, CD_PROP_FLOAT2, "skin_modifier_radius");
  if (cd_skin_radius_offset == -1) {
    return OPERATOR_CANCELLED;
  }

  BMVert *bm_vert;
  BMIter bm_iter;
  BM_ITER_MESH (bm_vert, &bm_iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(bm_vert, BM_ELEM_SELECT)) {
      float2 *radius = static_cast<float2 *>(
          BM_ELEM_CD_GET_VOID_P(bm_vert, cd_skin_radius_offset));
      const float avg = ((*radius)[0] + (*radius)[1]) * 0.5f;
      *radius = float2(avg);
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_skin_radii_equalize(wmOperatorType *ot)
{
  ot->name = "Skin Radii Equalize";
  ot->description = "Make skin radii of selected vertices equal on each axis";
  ot->idname = "OBJECT_OT_skin_radii_equalize";

  ot->poll = skin_edit_poll;
  ot->exec = skin_radii_equalize_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Delta Mesh Bind Operator
 * \{ */

static bool correctivesmooth_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, RNA_CorrectiveSmoothModifier, 0, true, false);
}

static wmOperatorStatus correctivesmooth_bind_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = context_active_object(C);
  CorrectiveSmoothModifierData *csmd = reinterpret_cast<CorrectiveSmoothModifierData *>(
      edit_modifier_property_get(op, ob, eModifierType_CorrectiveSmooth));

  if (!csmd) {
    return OPERATOR_CANCELLED;
  }

  if (!BKE_modifier_is_enabled(scene, &csmd->modifier, eModifierMode_Realtime)) {
    BKE_report(op->reports, RPT_ERROR, "Modifier is disabled");
    return OPERATOR_CANCELLED;
  }

  const bool is_bind = (csmd->bind_coords != nullptr);

  implicit_sharing::free_shared_data(&csmd->bind_coords, &csmd->bind_coords_sharing_info);
  MEM_SAFE_DELETE(csmd->delta_cache.deltas);

  if (is_bind) {
    /* toggle off */
    csmd->bind_coords_num = 0;
  }
  else {
    /* Signal to modifier to recalculate. */
    CorrectiveSmoothModifierData *csmd_eval = reinterpret_cast<CorrectiveSmoothModifierData *>(
        BKE_modifier_get_evaluated(depsgraph, ob, &csmd->modifier));
    csmd_eval->bind_coords_num = uint(-1);

    /* Force modifier to run, it will call binding routine
     * (this has to happen outside of depsgraph evaluation). */
    object_force_modifier_bind_simple_options(depsgraph, ob, &csmd->modifier);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus correctivesmooth_bind_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return correctivesmooth_bind_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_correctivesmooth_bind(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Corrective Smooth Bind";
  ot->description = "Bind base pose in Corrective Smooth modifier";
  ot->idname = "OBJECT_OT_correctivesmooth_bind";

  /* API callbacks. */
  ot->poll = correctivesmooth_poll;
  ot->invoke = correctivesmooth_bind_invoke;
  ot->exec = correctivesmooth_bind_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Mesh Deform Bind Operator
 * \{ */

static bool meshdeform_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, RNA_MeshDeformModifier, 0, true, false);
}

static wmOperatorStatus meshdeform_bind_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = context_active_object(C);
  MeshDeformModifierData *mmd = reinterpret_cast<MeshDeformModifierData *>(
      edit_modifier_property_get(op, ob, eModifierType_MeshDeform));

  if (mmd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (mmd->bindcagecos != nullptr) {
    implicit_sharing::free_shared_data(&mmd->bindcagecos, &mmd->bindcagecos_sharing_info);
    implicit_sharing::free_shared_data(&mmd->dyngrid, &mmd->dyngrid_sharing_info);
    implicit_sharing::free_shared_data(&mmd->dyninfluences, &mmd->dyninfluences_sharing_info);
    implicit_sharing::free_shared_data(&mmd->bindinfluences, &mmd->bindinfluences_sharing_info);
    implicit_sharing::free_shared_data(&mmd->bindoffsets, &mmd->bindoffsets_sharing_info);
    implicit_sharing::free_shared_data(&mmd->dynverts, &mmd->dynverts_sharing_info);
    MEM_SAFE_DELETE(mmd->bindweights); /* Deprecated */
    MEM_SAFE_DELETE(mmd->bindcos);     /* Deprecated */
    mmd->verts_num = 0;
    mmd->cage_verts_num = 0;
    mmd->influences_num = 0;
  }
  else {
    /* Force modifier to run, it will call binding routine
     * (this has to happen outside of depsgraph evaluation). */
    MeshDeformModifierData *mmd_eval = reinterpret_cast<MeshDeformModifierData *>(
        BKE_modifier_get_evaluated(depsgraph, ob, &mmd->modifier));
    mmd_eval->bindfunc = ED_mesh_deform_bind_callback;
    object_force_modifier_bind_simple_options(depsgraph, ob, &mmd->modifier);
    mmd_eval->bindfunc = nullptr;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus meshdeform_bind_invoke(bContext *C,
                                               wmOperator *op,
                                               const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return meshdeform_bind_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_meshdeform_bind(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mesh Deform Bind";
  ot->description = "Bind mesh to cage in mesh deform modifier";
  ot->idname = "OBJECT_OT_meshdeform_bind";

  /* API callbacks. */
  ot->poll = meshdeform_poll;
  ot->invoke = meshdeform_bind_invoke;
  ot->exec = meshdeform_bind_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Explode Refresh Operator
 * \{ */

static bool explode_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, RNA_ExplodeModifier, 0, true, false);
}

static wmOperatorStatus explode_refresh_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  ExplodeModifierData *emd = reinterpret_cast<ExplodeModifierData *>(
      edit_modifier_property_get(op, ob, eModifierType_Explode));

  if (!emd) {
    return OPERATOR_CANCELLED;
  }

  emd->flag |= eExplodeFlag_CalcFaces;

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus explode_refresh_invoke(bContext *C,
                                               wmOperator *op,
                                               const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return explode_refresh_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_explode_refresh(wmOperatorType *ot)
{
  ot->name = "Explode Refresh";
  ot->description = "Refresh data in the Explode modifier";
  ot->idname = "OBJECT_OT_explode_refresh";

  ot->poll = explode_poll;
  ot->invoke = explode_refresh_invoke;
  ot->exec = explode_refresh_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Laplacian-Deform Bind Operator
 * \{ */

static bool laplaciandeform_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, RNA_LaplacianDeformModifier, 0, false, false);
}

static wmOperatorStatus laplaciandeform_bind_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  LaplacianDeformModifierData *lmd = reinterpret_cast<LaplacianDeformModifierData *>(
      edit_modifier_property_get(op, ob, eModifierType_LaplacianDeform));

  if (lmd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (lmd->flag & MOD_LAPLACIANDEFORM_BIND) {
    lmd->flag &= ~MOD_LAPLACIANDEFORM_BIND;
  }
  else {
    lmd->flag |= MOD_LAPLACIANDEFORM_BIND;
  }

  LaplacianDeformModifierData *lmd_eval = reinterpret_cast<LaplacianDeformModifierData *>(
      BKE_modifier_get_evaluated(depsgraph, ob, &lmd->modifier));
  lmd_eval->flag = lmd->flag;

  /* Force modifier to run, it will call binding routine
   * (this has to happen outside of depsgraph evaluation). */
  object_force_modifier_bind_simple_options(depsgraph, ob, &lmd->modifier);

  /* This is hard to know from the modifier itself whether the evaluation is
   * happening for binding or not. So we copy all the required data here. */
  lmd->verts_num = lmd_eval->verts_num;
  if (lmd_eval->vertexco == nullptr) {
    implicit_sharing::free_shared_data(&lmd->vertexco, &lmd->vertexco_sharing_info);
  }
  else {
    implicit_sharing::copy_shared_pointer(lmd_eval->vertexco,
                                          lmd_eval->vertexco_sharing_info,
                                          &lmd->vertexco,
                                          &lmd->vertexco_sharing_info);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus laplaciandeform_bind_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return laplaciandeform_bind_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_laplaciandeform_bind(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Laplacian Deform Bind";
  ot->description = "Bind mesh to system in laplacian deform modifier";
  ot->idname = "OBJECT_OT_laplaciandeform_bind";

  /* API callbacks. */
  ot->poll = laplaciandeform_poll;
  ot->invoke = laplaciandeform_bind_invoke;
  ot->exec = laplaciandeform_bind_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Surface Deform Bind Operator
 * \{ */

static bool surfacedeform_bind_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, RNA_SurfaceDeformModifier, 0, true, false);
}

static wmOperatorStatus surfacedeform_bind_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  SurfaceDeformModifierData *smd = reinterpret_cast<SurfaceDeformModifierData *>(
      edit_modifier_property_get(op, ob, eModifierType_SurfaceDeform));

  if (smd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (smd->flags & MOD_SDEF_BIND) {
    smd->flags &= ~MOD_SDEF_BIND;
  }
  else if (smd->target) {
    smd->flags |= MOD_SDEF_BIND;
  }

  SurfaceDeformModifierData *smd_eval = reinterpret_cast<SurfaceDeformModifierData *>(
      BKE_modifier_get_evaluated(depsgraph, ob, &smd->modifier));
  smd_eval->flags = smd->flags;

  /* Force modifier to run, it will call binding routine
   * (this has to happen outside of depsgraph evaluation). */
  object_force_modifier_bind_simple_options(depsgraph, ob, &smd->modifier);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus surfacedeform_bind_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return surfacedeform_bind_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_surfacedeform_bind(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Surface Deform Bind";
  ot->description = "Bind mesh to target in surface deform modifier";
  ot->idname = "OBJECT_OT_surfacedeform_bind";

  /* API callbacks. */
  ot->poll = surfacedeform_bind_poll;
  ot->invoke = surfacedeform_bind_invoke;
  ot->exec = surfacedeform_bind_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

}  // namespace blender::ed::object
