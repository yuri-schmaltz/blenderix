/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fmt/format.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_vfont_types.h"

#include "BLI_kdtree_new.hh"
#include "BLI_linklist.hh"
#include "BLI_listbase.hh"
#include "BLI_math_matrix_c.hh"
#include "BLI_math_vector_c.hh"
#include "BLI_string_utf8.hh"
#include "BLI_utildefines.hh"
#include "BLI_vector_set.hh"

#include "BLT_translation.hh"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_collection.hh"
#include "BKE_constraint.h"
#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_idtype.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_light_linking.h"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_mesh_types.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "object_intern.hh"

namespace blender::ed::object {

/* ------------------------------------------------------------------- */
/** \name Make Vertex Parent Operator
 * \{ */

static bool vertex_parent_set_poll(bContext *C)
{
  return ED_operator_editmesh(C);
}

static wmOperatorStatus vertex_parent_set_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obedit = CTX_data_edit_object(C);
  Object *par;

#define INDEX_UNSET -1
  int par1, par2, par3, par4;
  par1 = par2 = par3 = par4 = INDEX_UNSET;

  /* we need 1 to 3 selected vertices */

  if (obedit->type == OB_MESH) {
    Mesh *mesh = id_cast<Mesh *>(obedit->data);

    EDBM_mesh_load(bmain, obedit);
    EDBM_mesh_make(obedit, scene->toolsettings->selectmode, true);

    DEG_id_tag_update(obedit->data, 0);

    EditMesh *em = mesh->runtime->edit_mesh.get();

    BKE_editmesh_looptris_and_normals_calc(em, BKE_editmesh_bmesh_ensure(mesh));

    BKE_scene_graph_update_tagged(depsgraph, bmain);

    BMVert *eve;
    BMIter iter;
    int curr_index;
    BMesh *bm = BKE_editmesh_bmesh_ensure(obedit);
    BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, curr_index) {
      if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
        if (par1 == INDEX_UNSET) {
          par1 = curr_index;
        }
        else if (par2 == INDEX_UNSET) {
          par2 = curr_index;
        }
        else if (par3 == INDEX_UNSET) {
          par3 = curr_index;
        }
        else if (par4 == INDEX_UNSET) {
          par4 = curr_index;
        }
        else {
          break;
        }
      }
    }
  }

  if (par4 != INDEX_UNSET || par1 == INDEX_UNSET || (par2 != INDEX_UNSET && par3 == INDEX_UNSET)) {
    BKE_report(op->reports, RPT_ERROR, "Select either 1 or 3 vertices to parent to");
    return OPERATOR_CANCELLED;
  }

  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    if (ob != obedit) {
      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
      par = obedit->parent;

      if (BKE_object_parent_loop_check(par, ob)) {
        BKE_report(op->reports, RPT_ERROR, "Loop in parents");
      }
      else {
        BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
        ob->parent = BKE_view_layer_active_object_get(view_layer);
        BKE_collection_object_parented_sort_index_reset(*bmain, *ob);
        if (par3 != INDEX_UNSET) {
          ob->partype = PARVERT3;
          ob->par1 = par1;
          ob->par2 = par2;
          ob->par3 = par3;

          /* inverse parent matrix */
          invert_m4_m4(ob->parentinv, BKE_object_calc_parent(depsgraph, scene, ob).ptr());
        }
        else {
          ob->partype = PARVERT1;
          ob->par1 = par1;

          /* inverse parent matrix */
          invert_m4_m4(ob->parentinv, BKE_object_calc_parent(depsgraph, scene, ob).ptr());
        }
      }
    }
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);

  WM_event_add_notifier(C, NC_OBJECT, nullptr);

  return OPERATOR_FINISHED;

#undef INDEX_UNSET
}

void OBJECT_OT_vertex_parent_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Vertex Parent";
  ot->description = "Parent selected objects to the selected vertices";
  ot->idname = "OBJECT_OT_vertex_parent_set";

  /* API callbacks. */
  ot->poll = vertex_parent_set_poll;
  ot->exec = vertex_parent_set_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Clear Parent Operator
 * \{ */

const EnumPropertyItem prop_clear_parent_types[] = {
    {CLEAR_PARENT_ALL,
     "CLEAR",
     0,
     "Clear Parent",
     "Completely clear the parenting relationship, including involved modifiers if any"},
    {CLEAR_PARENT_KEEP_TRANSFORM,
     "CLEAR_KEEP_TRANSFORM",
     0,
     "Clear and Keep Transformation",
     "As 'Clear Parent', but keep the current visual transformations of the object"},
    {CLEAR_PARENT_INVERSE,
     "CLEAR_INVERSE",
     0,
     "Clear Parent Inverse",
     "Reset the transform corrections applied to the parenting relationship, does not remove "
     "parenting itself"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* Helper for parent_clear() - Remove deform-modifiers associated with parent */
static void object_remove_parent_deform_modifiers(Object *ob, const Object *par)
{
  if (ELEM(par->type, OB_ARMATURE, OB_LATTICE, OB_CURVES_LEGACY)) {
    ModifierData *md, *mdn;

    /* assume that we only need to remove the first instance of matching deform modifier here */
    for (md = ob->modifiers.first(); md; md = mdn) {
      bool free = false;

      mdn = md->next;

      /* need to match types (modifier + parent) and references */
      if ((md->type == eModifierType_Armature) && (par->type == OB_ARMATURE)) {
        ArmatureModifierData *amd = reinterpret_cast<ArmatureModifierData *>(md);
        if (amd->object == par) {
          free = true;
        }
      }
      else if ((md->type == eModifierType_Lattice) && (par->type == OB_LATTICE)) {
        LatticeModifierData *lmd = reinterpret_cast<LatticeModifierData *>(md);
        if (lmd->object == par) {
          free = true;
        }
      }
      else if ((md->type == eModifierType_Curve) && (par->type == OB_CURVES_LEGACY)) {
        CurveModifierData *cmd = reinterpret_cast<CurveModifierData *>(md);
        if (cmd->object == par) {
          free = true;
        }
      }

      /* free modifier if match */
      if (free) {
        BKE_modifier_remove_from_list(ob, md);
        BKE_modifier_free(md);
      }
    }
  }
}

static void parent_clear_data(Object *ob)
{
  ob->parent = nullptr;
  /* Set parent type to default PAROBJECT and reset enum explicitly, to prevent rna enum errors
   * later. */
  ob->partype = PAROBJECT;
  ob->parsubstr[0] = '\0';
}

void parent_clear(Main *bmain, Object *ob, const int type)
{
  if (ob->parent == nullptr) {
    return;
  }
  uint flags = ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION;
  if (type != CLEAR_PARENT_INVERSE) {
    BKE_collection_object_parent_clear_sort_index_reset(*bmain, *ob);
  }
  switch (type) {
    case CLEAR_PARENT_ALL: {
      /* for deformers, remove corresponding modifiers to prevent
       * a large number of modifiers building up */
      object_remove_parent_deform_modifiers(ob, ob->parent);

      /* clear parenting relationship completely */
      parent_clear_data(ob);
      break;
    }
    case CLEAR_PARENT_KEEP_TRANSFORM: {
      /* remove parent, and apply the parented transform
       * result as object's local transforms */
      parent_clear_data(ob);
      BKE_object_apply_mat4(ob, ob->object_to_world().ptr(), true, false);
      /* Don't recalculate the animation because it would change the transform
       * instead of keeping it. */
      flags &= ~ID_RECALC_ANIMATION;
      break;
    }
    case CLEAR_PARENT_INVERSE: {
      /* object stays parented, but the parent inverse
       * (i.e. offset from parent to retain binding state)
       * is cleared. In other words: nothing to do here! */
      break;
    }
  }

  /* Always clear parentinv matrix for sake of consistency, see #41950. */
  unit_m4(ob->parentinv);

  DEG_id_tag_update(&ob->id, flags);
}

/* NOTE: poll should check for editable scene. */
static wmOperatorStatus parent_clear_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  /* Dependency graph must be evaluated for access to object's evaluated transform matrices. */
  CTX_data_ensure_evaluated_depsgraph(C);
  const int type = RNA_enum_get(op->ptr, "type");

  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    parent_clear(bmain, ob, type);
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, nullptr);
  return OPERATOR_FINISHED;
}

void OBJECT_OT_parent_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Parent";
  ot->description = "Clear the object's parenting";
  ot->idname = "OBJECT_OT_parent_clear";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = parent_clear_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_clear_parent_types, CLEAR_PARENT_ALL, "Type", "");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Make Parent Operator
 * \{ */

void parent_set(Object *ob, Object *par, const eObject_Partype type, const char *substr)
{
  /* Always clear parentinv matrix for sake of consistency, see #41950. */
  unit_m4(ob->parentinv);

  if (!par || BKE_object_parent_loop_check(par, ob)) {
    parent_clear_data(ob);
    return;
  }

  /* Other partypes are deprecated, do not use here! */
  BLI_assert(ELEM(type & PARTYPE, PAROBJECT, PARSKEL, PARVERT1, PARVERT3, PARBONE));

  /* this could use some more checks */

  ob->parent = par;
  ob->partype &= ~PARTYPE;
  ob->partype |= type;
  STRNCPY_UTF8(ob->parsubstr, substr);
}

const EnumPropertyItem prop_make_parent_types[] = {
    {PAR_OBJECT, "OBJECT", 0, "Object", ""},
    {PAR_ARMATURE, "ARMATURE", 0, "Armature Deform", ""},
    {PAR_ARMATURE_NAME, "ARMATURE_NAME", 0, "   With Empty Groups", ""},
    {PAR_ARMATURE_AUTO, "ARMATURE_AUTO", 0, "   With Automatic Weights", ""},
    {PAR_ARMATURE_ENVELOPE, "ARMATURE_ENVELOPE", 0, "   With Envelope Weights", ""},
    {PAR_BONE, "BONE", 0, "Bone", ""},
    {PAR_BONE_RELATIVE, "BONE_RELATIVE", 0, "Bone Relative", ""},
    {PAR_CURVE, "CURVE", 0, "Curve Deform", ""},
    {PAR_FOLLOW, "FOLLOW", 0, "Follow Path", ""},
    {PAR_PATH_CONST, "PATH_CONST", 0, "Path Constraint", ""},
    {PAR_LATTICE, "LATTICE", 0, "Lattice Deform", ""},
    {PAR_VERTEX, "VERTEX", 0, "Vertex", ""},
    {PAR_VERTEX_TRI, "VERTEX_TRI", 0, "Vertex (Triangle)", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static bool parent_set_with_depsgraph(ReportList *reports,
                                      const bContext *C,
                                      Scene *scene,
                                      Depsgraph *depsgraph,
                                      Object *const ob,
                                      Object *const par,
                                      Object *const /*parent_eval*/,
                                      int partype,
                                      const bool /*xmirror*/,
                                      const bool keep_transform,
                                      const int vert_par[3])
{
  Main *bmain = CTX_data_main(C);

  /* Preconditions. */
  if (ob == par) {
    /* Parenting an object to itself is impossible. */
    return false;
  }

  if (BKE_object_parent_loop_check(par, ob)) {
    BKE_report(reports, RPT_ERROR, "Loop in parents");
    return false;
  }

  if (!ELEM(partype, PAR_OBJECT, PAR_VERTEX, PAR_VERTEX_TRI)) {
    BKE_report(reports, RPT_ERROR, "Parenting mode not supported in mesh-only build");
    return false;
  }

  /* Apply transformation of previous parenting. */
  if (keep_transform) {
    BKE_object_apply_mat4(ob, ob->object_to_world().ptr(), false, false);
  }

  ob->parent = par;
  BKE_collection_object_parented_sort_index_reset(*bmain, *ob);
  unit_m4(ob->parentinv);
  ob->parsubstr[0] = 0;

  switch (partype) {
    case PAR_VERTEX:
      ob->partype = PARVERT1;
      ob->par1 = vert_par[0];
      break;
    case PAR_VERTEX_TRI:
      ob->partype = PARVERT3;
      copy_v3_v3_int(&ob->par1, vert_par);
      break;
    default:
      ob->partype = PAROBJECT;
      break;
  }

  /* calculate inverse parent matrix */
  invert_m4_m4(ob->parentinv, BKE_object_calc_parent(depsgraph, scene, ob).ptr());

  DEG_id_tag_update(&par->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  return true;
}

bool parent_set(ReportList *reports,
                const bContext *C,
                Scene *scene,
                Object *const ob,
                Object *const par,
                int partype,
                const bool xmirror,
                const bool keep_transform,
                const int vert_par[3])
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *parent_eval = DEG_get_evaluated(depsgraph, par);

  return parent_set_with_depsgraph(reports,
                                   C,
                                   scene,
                                   depsgraph,
                                   ob,
                                   par,
                                   parent_eval,
                                   partype,
                                   xmirror,
                                   keep_transform,
                                   vert_par);
}

static void parent_set_vert_find(const KDTree<float3> &tree,
                                 const Span<int> orig_indices,
                                 Object *child,
                                 int vert_par[3],
                                 bool is_tri)
{
  const auto to_orig_index = [&](const int index) {
    return orig_indices.is_empty() ? index : orig_indices[index];
  };
  const float3 co_find = child->object_to_world().location();
  if (is_tri) {
    std::array<KDTree<float3>::Nearest, 3> nearest;
    const int tot = tree.find_nearest_n(co_find, std::numeric_limits<float>::max(), nearest);
    BLI_assert(tot == 3);
    UNUSED_VARS_NDEBUG(tot);

    vert_par[0] = to_orig_index(nearest[0].index);
    vert_par[1] = to_orig_index(nearest[1].index);
    vert_par[2] = to_orig_index(nearest[2].index);

    BLI_assert(std::min({UNPACK3(vert_par)}) >= 0);
  }
  else {
    const int nearest = tree.find_nearest(co_find);
    BLI_assert(nearest >= 0);
    vert_par[0] = to_orig_index(nearest);
    vert_par[1] = 0;
    vert_par[2] = 0;
  }
}

struct ParentingContext {
  ReportList *reports;
  Scene *scene;
  Object *par;
  int partype;
  bool is_vertex_tri;
  bool xmirror;
  bool keep_transform;
};

static bool parent_set_nonvertex_parent(bContext *C, ParentingContext *parenting_context)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *parent_eval = DEG_get_evaluated(depsgraph, parenting_context->par);

  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    if (ob == parenting_context->par) {
      /* parent_set() will fail (and thus return false), but this case
       * shouldn't break this loop. It's expected that the active object is also selected. */
      continue;
    }

    if (!parent_set_with_depsgraph(parenting_context->reports,
                                   C,
                                   parenting_context->scene,
                                   depsgraph,
                                   ob,
                                   parenting_context->par,
                                   parent_eval,
                                   parenting_context->partype,
                                   parenting_context->xmirror,
                                   parenting_context->keep_transform,
                                   nullptr))
    {
      return false;
    }
  }
  CTX_DATA_END;

  return true;
}

static bool parent_set_vertex_parent_with_kdtree(bContext *C,
                                                 ParentingContext *parenting_context,
                                                 const KDTree<float3> &tree,
                                                 const Span<int> orig_indices)
{
  int vert_par[3] = {0, 0, 0};

  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    if (ob == parenting_context->par) {
      /* parent_set() will fail (and thus return false), but this case
       * shouldn't break this loop. It's expected that the active object is also selected. */
      continue;
    }

    parent_set_vert_find(tree, orig_indices, ob, vert_par, parenting_context->is_vertex_tri);
    if (!parent_set(parenting_context->reports,
                    C,
                    parenting_context->scene,
                    ob,
                    parenting_context->par,
                    parenting_context->partype,
                    parenting_context->xmirror,
                    parenting_context->keep_transform,
                    vert_par))
    {
      return false;
    }
  }
  CTX_DATA_END;
  return true;
}

static bool parent_set_vertex_parent(bContext *C, ParentingContext *parenting_context)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *par_eval = DEG_get_evaluated(depsgraph, parenting_context->par);

  Vector<int> orig_indices;
  /* Empty for unsupported object types. */
  const Vector<float3> positions = BKE_object_positions_for_kdtree(par_eval, orig_indices);

  if (positions.size() < (parenting_context->is_vertex_tri ? 3 : 1)) {
    BKE_report(parenting_context->reports, RPT_ERROR, "Not enough vertices for vertex-parent");
    return false;
  }

  const KDTree<float3> tree(positions);
  return parent_set_vertex_parent_with_kdtree(C, parenting_context, tree, orig_indices);
}

static wmOperatorStatus parent_set_exec(bContext *C, wmOperator *op)
{
  const int partype = RNA_enum_get(op->ptr, "type");
  ParentingContext parenting_context{};
  parenting_context.reports = op->reports;
  parenting_context.scene = CTX_data_scene(C);
  parenting_context.par = context_active_object(C);
  parenting_context.partype = partype;
  parenting_context.is_vertex_tri = partype == PAR_VERTEX_TRI;
  parenting_context.xmirror = RNA_boolean_get(op->ptr, "xmirror");
  parenting_context.keep_transform = RNA_boolean_get(op->ptr, "keep_transform");

  bool ok;
  if (ELEM(parenting_context.partype, PAR_VERTEX, PAR_VERTEX_TRI)) {
    ok = parent_set_vertex_parent(C, &parenting_context);
  }
  else {
    ok = parent_set_nonvertex_parent(C, &parenting_context);
  }
  if (!ok) {
    return OPERATOR_CANCELLED;
  }

  Main *bmain = CTX_data_main(C);
  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus parent_set_invoke_menu(bContext *C, wmOperatorType *ot)
{
  Object *parent = context_active_object(C);
  ui::PopupMenu *pup = ui::popup_menu_begin(C, IFACE_("Set Parent To"), ICON_NONE);
  ui::Layout &layout = *popup_menu_layout(pup);

  PointerRNA opptr = layout.op(
      ot, IFACE_("Object"), ICON_OBJECT_DATA, wm::OpCallContext::ExecDefault, UI_ITEM_NONE);
  RNA_enum_set(&opptr, "type", PAR_OBJECT);
  RNA_boolean_set(&opptr, "keep_transform", false);

  opptr = layout.op(ot,
                    IFACE_("Object (Keep Transform)"),
                    ICON_NONE,
                    wm::OpCallContext::ExecDefault,
                    UI_ITEM_NONE);
  RNA_enum_set(&opptr, "type", PAR_OBJECT);
  RNA_boolean_set(&opptr, "keep_transform", true);

  PointerRNA op_ptr = layout.op(
      "OBJECT_OT_parent_no_inverse_set", IFACE_("Object (Without Inverse)"), ICON_NONE);
  RNA_boolean_set(&op_ptr, "keep_transform", false);

  op_ptr = layout.op("OBJECT_OT_parent_no_inverse_set",
                     IFACE_("Object (Keep Transform Without Inverse)"),
                     ICON_NONE);
  RNA_boolean_set(&op_ptr, "keep_transform", true);

  /* vertex parenting */
  if (parent->type == OB_MESH) {
    op_ptr = layout.op(ot, IFACE_("Vertex"), ICON_VERTEXSEL);
    RNA_enum_set(&op_ptr, "type", PAR_VERTEX);
    op_ptr = layout.op(ot, IFACE_("Vertex (Triangle)"), ICON_NONE);
    RNA_enum_set(&op_ptr, "type", PAR_VERTEX_TRI);
  }

  popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static wmOperatorStatus parent_set_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  if (RNA_property_is_set(op->ptr, op->type->prop)) {
    return parent_set_exec(C, op);
  }
  return parent_set_invoke_menu(C, op->type);
}

static bool parent_set_poll_property(const bContext * /*C*/,
                                     wmOperator *op,
                                     const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);

  /* Only show XMirror for PAR_ARMATURE_ENVELOPE and PAR_ARMATURE_AUTO! */
  if (STREQ(prop_id, "xmirror")) {
    const int type = RNA_enum_get(op->ptr, "type");
    if (ELEM(type, PAR_ARMATURE_ENVELOPE, PAR_ARMATURE_AUTO)) {
      return true;
    }
    return false;
  }

  return true;
}

void OBJECT_OT_parent_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Parent";
  ot->description = "Set the object's parenting";
  ot->idname = "OBJECT_OT_parent_set";

  /* API callbacks. */
  ot->invoke = parent_set_invoke;
  ot->exec = parent_set_exec;
  ot->poll = ED_operator_object_active;
  ot->poll_property = parent_set_poll_property;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_make_parent_types, 0, "Type", "");
  RNA_def_boolean(
      ot->srna,
      "xmirror",
      false,
      "X Mirror",
      "Apply weights symmetrically along X axis, for Envelope/Automatic vertex groups creation");
  RNA_def_boolean(ot->srna,
                  "keep_transform",
                  false,
                  "Keep Transform",
                  "Apply transformation before parenting");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Make Parent Without Inverse Operator
 * \{ */

static wmOperatorStatus parent_noinv_set_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *par = context_active_object(C);

  const bool keep_transform = RNA_boolean_get(op->ptr, "keep_transform");

  DEG_id_tag_update(&par->id, ID_RECALC_TRANSFORM);

  /* context iterator */
  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    if (ob != par) {
      if (BKE_object_parent_loop_check(par, ob)) {
        BKE_report(op->reports, RPT_ERROR, "Loop in parents");
      }
      else {
        /* set recalc flags */
        DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

        /* set parenting type for object - object only... */
        ob->parent = par;
        ob->partype = PAROBJECT; /* NOTE: DNA define, not operator property. */

        if (keep_transform) {
          BKE_object_apply_parent_inverse(ob);
          continue;
        }

        /* clear inverse matrix and also the object location */
        unit_m4(ob->parentinv);
        memset(ob->loc, 0, sizeof(float[3]));
      }
    }
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, nullptr);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_parent_no_inverse_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Parent without Inverse";
  ot->description = "Set the object's parenting without setting the inverse parent correction";
  ot->idname = "OBJECT_OT_parent_no_inverse_set";

  /* API callbacks. */
  ot->exec = parent_noinv_set_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "keep_transform",
                  false,
                  "Keep Transform",
                  "Preserve the world transform throughout parenting");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Clear Track Operator
 * \{ */

enum {
  CLEAR_TRACK = 1,
  CLEAR_TRACK_KEEP_TRANSFORM = 2,
};

static const EnumPropertyItem prop_clear_track_types[] = {
    {CLEAR_TRACK, "CLEAR", 0, "Clear Track", ""},
    {CLEAR_TRACK_KEEP_TRANSFORM,
     "CLEAR_KEEP_TRANSFORM",
     0,
     "Clear and Keep Transformation (Clear Track)",
     ""},
    {0, nullptr, 0, nullptr, nullptr},
};

/* NOTE: poll should check for editable scene. */
static wmOperatorStatus object_track_clear_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const int type = RNA_enum_get(op->ptr, "type");

  if (CTX_data_edit_object(C)) {
    BKE_report(op->reports, RPT_ERROR, "Operation cannot be performed in edit mode");
    return OPERATOR_CANCELLED;
  }
  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    bConstraint *con, *pcon;

    /* remove track-object for old track */
    ob->track = nullptr;
    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

    /* also remove all tracking constraints */
    for (con = ob->constraints.last(); con; con = pcon) {
      pcon = con->prev;
      if (ELEM(con->type,
               CONSTRAINT_TYPE_TRACKTO,
               CONSTRAINT_TYPE_LOCKTRACK,
               CONSTRAINT_TYPE_DAMPTRACK))
      {
        BKE_constraint_remove_ex(&ob->constraints, ob, con);
      }
    }

    if (type == CLEAR_TRACK_KEEP_TRANSFORM) {
      BKE_object_apply_mat4(ob, ob->object_to_world().ptr(), true, true);
    }
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_track_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Track";
  ot->description = "Clear tracking constraint or flag from object";
  ot->idname = "OBJECT_OT_track_clear";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_track_clear_exec;

  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_clear_track_types, 0, "Type", "");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Make Track Operator
 * \{ */

enum {
  CREATE_TRACK_DAMPTRACK = 1,
  CREATE_TRACK_TRACKTO = 2,
  CREATE_TRACK_LOCKTRACK = 3,
};

static const EnumPropertyItem prop_make_track_types[] = {
    {CREATE_TRACK_DAMPTRACK, "DAMPTRACK", 0, "Damped Track Constraint", ""},
    {CREATE_TRACK_TRACKTO, "TRACKTO", 0, "Track to Constraint", ""},
    {CREATE_TRACK_LOCKTRACK, "LOCKTRACK", 0, "Lock Track Constraint", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus track_set_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *obact = context_active_object(C);

  const int type = RNA_enum_get(op->ptr, "type");

  switch (type) {
    case CREATE_TRACK_DAMPTRACK: {
      bConstraint *con;
      bDampTrackConstraint *data;

      CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
        if (ob != obact) {
          con = BKE_constraint_add_for_object(ob, "AutoTrack", CONSTRAINT_TYPE_DAMPTRACK);

          data = static_cast<bDampTrackConstraint *>(con->data);
          data->tar = obact;
          DEG_id_tag_update(&ob->id,
                            ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

          /* Light, Camera and Speaker track differently by default */
          if (ELEM(ob->type, OB_LAMP, OB_CAMERA, OB_SPEAKER)) {
            data->trackflag = TRACK_nZ;
          }
        }
      }
      CTX_DATA_END;
      break;
    }
    case CREATE_TRACK_TRACKTO: {
      bConstraint *con;
      bTrackToConstraint *data;

      CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
        if (ob != obact) {
          con = BKE_constraint_add_for_object(ob, "AutoTrack", CONSTRAINT_TYPE_TRACKTO);

          data = static_cast<bTrackToConstraint *>(con->data);
          data->tar = obact;
          DEG_id_tag_update(&ob->id,
                            ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

          /* Light, Camera and Speaker track differently by default */
          if (ELEM(ob->type, OB_LAMP, OB_CAMERA, OB_SPEAKER)) {
            data->reserved1 = TRACK_nZ;
            data->reserved2 = UP_Y;
          }
        }
      }
      CTX_DATA_END;
      break;
    }
    case CREATE_TRACK_LOCKTRACK: {
      bConstraint *con;
      bLockTrackConstraint *data;

      CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
        if (ob != obact) {
          con = BKE_constraint_add_for_object(ob, "AutoTrack", CONSTRAINT_TYPE_LOCKTRACK);

          data = static_cast<bLockTrackConstraint *>(con->data);
          data->tar = obact;
          DEG_id_tag_update(&ob->id,
                            ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

          /* Light, Camera and Speaker track differently by default */
          if (ELEM(ob->type, OB_LAMP, OB_CAMERA, OB_SPEAKER)) {
            data->trackflag = TRACK_nZ;
            data->lockflag = LOCK_Y;
          }
        }
      }
      CTX_DATA_END;
      break;
    }
  }

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_track_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Track";
  ot->description = "Make the object track another object, using various methods/constraints";
  ot->idname = "OBJECT_OT_track_set";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = track_set_exec;

  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_make_track_types, 0, "Type", "");
}

/** \} */

enum {
  MAKE_LINKS_OBDATA = 1,
  MAKE_LINKS_MATERIALS = 2,
  MAKE_LINKS_ANIMDATA = 3,
  MAKE_LINKS_GROUP = 4,
  MAKE_LINKS_DUPLICOLLECTION = 5,
  MAKE_LINKS_MODIFIERS = 6,
  MAKE_LINKS_CONSTRAINTS = 7,
  MAKE_LINKS_FONTS = 8,
  MAKE_LINKS_LIGHT_LINKING = 10,
  MAKE_LINKS_SHADOW_LINKING = 11,
};

/* Matches has_geometry_visibility() from Python.
 * Indicates whether an object has shading/visibility settings. */
static bool has_geometry_visibility(const Object &object)
{
  if (ELEM(object.type,
           OB_MESH,
           OB_CURVES_LEGACY,
           OB_SURF,
           OB_FONT,
           OB_MBALL,
           OB_LAMP,
           OB_VOLUME,
           OB_POINTCLOUD,
           OB_CURVES))
  {
    return true;
  }

  if (object.transflag & OB_DUPLICOLLECTION && object.instance_collection) {
    return true;
  }

  return false;
}

/* Return true if make link data is allowed, false otherwise */
static bool allow_make_links_data(const int type, Object *ob_src, Object *ob_dst)
{
  switch (type) {
    case MAKE_LINKS_OBDATA:
      /* Replacing data of an object in edit-mode is not supported, see #160956. */
      if (ob_src->type == ob_dst->type && ob_src->type != OB_EMPTY &&
          (ob_dst->mode & OB_MODE_EDIT) == 0)
      {
        return true;
      }
      break;
    case MAKE_LINKS_MATERIALS:
      if (OB_TYPE_SUPPORT_MATERIAL(ob_src->type) && OB_TYPE_SUPPORT_MATERIAL(ob_dst->type) &&
          /* Linking non-grease-pencil materials to a grease-pencil object causes issues.
           * We make sure that if one of the objects is a grease-pencil object, the other must be
           * as well. */
          ((ob_src->type == OB_GREASE_PENCIL) == (ob_dst->type == OB_GREASE_PENCIL)))
      {
        return true;
      }
      break;
    case MAKE_LINKS_DUPLICOLLECTION:
      if (ob_dst->type == OB_EMPTY) {
        return true;
      }
      break;
    case MAKE_LINKS_ANIMDATA:
    case MAKE_LINKS_GROUP:
      return true;
    case MAKE_LINKS_MODIFIERS:
      if (BKE_object_supports_modifiers(ob_src) && BKE_object_supports_modifiers(ob_dst)) {
        return true;
      }
      break;
    case MAKE_LINKS_CONSTRAINTS:
      return true;
    case MAKE_LINKS_FONTS:
      if ((ob_src->data != ob_dst->data) && (ob_src->type == OB_FONT) && (ob_dst->type == OB_FONT))
      {
        return true;
      }
      break;
    case MAKE_LINKS_LIGHT_LINKING:
    case MAKE_LINKS_SHADOW_LINKING:
      return has_geometry_visibility(*ob_src) && has_geometry_visibility(*ob_dst);
  }
  return false;
}

static wmOperatorStatus make_links_data_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Main *bmain = CTX_data_main(C);
  const int type = RNA_enum_get(op->ptr, "type");
  Object *ob_src;
  ID *obdata_id;
  int a;

  /* collection */
  LinkNode *ob_collections = nullptr;
  bool is_cycle = false;
  bool is_lib = false;

  ob_src = context_active_object(C);

  /* avoid searching all collections in source object each time */
  if (type == MAKE_LINKS_GROUP) {
    ob_collections = BKE_object_groups(bmain, scene, ob_src);
  }

  CTX_DATA_BEGIN (C, Base *, base_dst, selected_editable_bases) {
    Object *ob_dst = base_dst->object;

    if (ob_src != ob_dst) {
      if (allow_make_links_data(type, ob_src, ob_dst)) {
        obdata_id = ob_dst->data;

        switch (type) {
          case MAKE_LINKS_OBDATA: /* obdata */
            id_us_min(obdata_id);

            obdata_id = ob_src->data;
            id_us_plus(obdata_id);
            ob_dst->data = obdata_id;

            BKE_sculptsession_free_pbvh(*ob_dst);
            /* if amount of material indices changed: */
            BKE_object_materials_sync_length(bmain, ob_dst, ob_dst->data);
            DEG_id_tag_update(&ob_dst->id, ID_RECALC_GEOMETRY);
            break;
          case MAKE_LINKS_MATERIALS:
            /* new approach, using functions from kernel */
            for (a = 0; a < ob_src->totcol; a++) {
              Material *ma = BKE_object_material_get(ob_src, a + 1);
              /* also works with `ma == nullptr` */
              BKE_object_material_assign(bmain, ob_dst, ma, a + 1, BKE_MAT_ASSIGN_USERPREF);
            }
            DEG_id_tag_update(&ob_dst->id, ID_RECALC_GEOMETRY);
            break;
          case MAKE_LINKS_ANIMDATA:
            BKE_animdata_copy_id(bmain, id_cast<ID *>(ob_dst), id_cast<ID *>(ob_src), 0);
            if (ob_dst->data && ob_src->data) {
              if (BKE_id_is_editable(bmain, obdata_id)) {
                BKE_animdata_copy_id(bmain, ob_dst->data, ob_src->data, 0);
              }
              else {
                is_lib = true;
              }
            }
            DEG_id_tag_update(&ob_dst->id,
                              ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
            break;
          case MAKE_LINKS_GROUP: {
            LinkNode *collection_node;

            /* first clear collections */
            BKE_object_groups_clear(bmain, scene, ob_dst);

            /* now add in the collections from the link nodes */
            for (collection_node = ob_collections; collection_node;
                 collection_node = collection_node->next)
            {
              if (ob_dst->instance_collection != collection_node->link) {
                BKE_collection_object_add(
                    bmain, static_cast<Collection *>(collection_node->link), ob_dst);
              }
              else {
                is_cycle = true;
              }
            }
            break;
          }
          case MAKE_LINKS_DUPLICOLLECTION:
            ob_dst->instance_collection = ob_src->instance_collection;
            if (ob_dst->instance_collection) {
              id_us_plus(&ob_dst->instance_collection->id);
              ob_dst->transflag |= OB_DUPLICOLLECTION;
            }
            DEG_id_tag_update(&ob_dst->id, ID_RECALC_SYNC_TO_EVAL);
            break;
          case MAKE_LINKS_MODIFIERS:
            BKE_object_link_modifiers(ob_dst, ob_src);
            DEG_id_tag_update(&ob_dst->id,
                              ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
            break;
          case MAKE_LINKS_CONSTRAINTS:
            BKE_constraints_free(&ob_dst->constraints);
            BKE_constraints_copy(&ob_dst->constraints, &ob_src->constraints, true);
            DEG_id_tag_update(&ob_dst->id, ID_RECALC_GEOMETRY | ID_RECALC_TRANSFORM);
            WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, nullptr);
            break;
          case MAKE_LINKS_FONTS: {
            Curve *cu_src = id_cast<Curve *>(ob_src->data);
            Curve *cu_dst = id_cast<Curve *>(ob_dst->data);

            if (!BKE_id_is_editable(bmain, obdata_id)) {
              is_lib = true;
              break;
            }

#define CURVE_VFONT_SET(vfont_member) \
  { \
    if (cu_dst->vfont_member) { \
      id_us_min(&cu_dst->vfont_member->id); \
    } \
    cu_dst->vfont_member = cu_src->vfont_member; \
    id_us_plus((ID *)cu_dst->vfont_member); \
  } \
  ((void)0)

            CURVE_VFONT_SET(vfont);
            CURVE_VFONT_SET(vfontb);
            CURVE_VFONT_SET(vfonti);
            CURVE_VFONT_SET(vfontbi);

#undef CURVE_VFONT_SET

            DEG_id_tag_update(&ob_dst->id,
                              ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
            break;
          }
          case MAKE_LINKS_LIGHT_LINKING:
            BKE_light_linking_copy_receiver_collection(bmain, *ob_dst, *ob_src);
            break;
          case MAKE_LINKS_SHADOW_LINKING:
            BKE_light_linking_copy_blocker_collection(bmain, *ob_dst, *ob_src);
            break;
        }
      }
    }
  }
  CTX_DATA_END;

  if (type == MAKE_LINKS_GROUP) {
    if (ob_collections) {
      BLI_linklist_free(ob_collections, nullptr);
    }

    if (is_cycle) {
      BKE_report(op->reports, RPT_WARNING, "Skipped some collections because of cycle detected");
    }
  }

  if (is_lib) {
    BKE_report(op->reports, RPT_WARNING, "Skipped editing library object data");
  }

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, CTX_wm_view3d(C));
  WM_event_add_notifier(C, NC_OBJECT, nullptr);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_make_links_data(wmOperatorType *ot)
{
  static const EnumPropertyItem make_links_items[] = {
      {MAKE_LINKS_OBDATA, "OBDATA", 0, "Link Object Data", "Replace assigned Object Data"},
      {MAKE_LINKS_MATERIALS, "MATERIAL", 0, "Link Materials", "Replace assigned Materials"},
      {MAKE_LINKS_ANIMDATA,
       "ANIMATION",
       0,
       "Link Animation Data",
       "Replace assigned Animation Data"},
      {MAKE_LINKS_GROUP, "GROUPS", 0, "Link Collections", "Replace assigned Collections"},
      {MAKE_LINKS_DUPLICOLLECTION,
       "DUPLICOLLECTION",
       0,
       "Link Instance Collection",
       "Replace assigned Collection Instance"},
      {MAKE_LINKS_FONTS, "FONTS", 0, "Link Fonts to Text", "Replace Text object Fonts"},
      RNA_ENUM_ITEM_SEPR,
      {MAKE_LINKS_MODIFIERS, "MODIFIERS", 0, "Copy Modifiers", "Replace Modifiers"},
      {MAKE_LINKS_CONSTRAINTS, "CONSTRAINTS", 0, "Copy Constraints", "Replace Constraints"},
      {MAKE_LINKS_LIGHT_LINKING,
       "LIGHT_LINKING",
       0,
       "Copy Light Linking",
       "Replace assigned Light Linking collection"},
      {MAKE_LINKS_SHADOW_LINKING,
       "SHADOW_LINKING",
       0,
       "Copy Shadow Linking",
       "Replace assigned Shadow Linking collection"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Link/Transfer Data";
  ot->description = "Transfer data from active object to selected objects";
  ot->idname = "OBJECT_OT_make_links_data";

  /* API callbacks. */
  ot->exec = make_links_data_exec;
  /* The object must not be in edit-mode because multiple objects in edit-mode
   * sharing data is an invalid state which can cause crashes. See: #160956. */
  ot->poll = ED_operator_object_active_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", make_links_items, 0, "Type", "");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Make Single User Operator
 * \{ */

static bool single_data_needs_duplication(ID *id)
{
  /* NOTE: When dealing with linked data, we always make a local copy of it.
   * While in theory we could rather make it local when it only has one user, this is difficult
   * in practice with current code of this function. */
  return (id != nullptr && (id->us > 1 || ID_IS_LINKED(id)));
}

static void libblock_relink_collection(Main *bmain,
                                       Collection *collection,
                                       const bool do_collection)
{
  if (do_collection) {
    BKE_libblock_relink_to_newid(bmain, &collection->id, 0);
  }

  for (CollectionObject *cob = collection->gobject.first(); cob != nullptr; cob = cob->next) {
    BKE_libblock_relink_to_newid(bmain, &cob->ob->id, 0);
  }

  for (CollectionChild &child : collection->children) {
    libblock_relink_collection(bmain, child.collection, true);
  }
}

static Collection *single_object_users_collection(Main *bmain,
                                                  Scene *scene,
                                                  Collection *collection,
                                                  const int flag,
                                                  const bool copy_collections,
                                                  const bool is_master_collection)
{
  /* Generate new copies for objects in given collection and all its children,    * and
   * optionally also copy collections themselves. */
  if (copy_collections && !is_master_collection) {
    Collection *collection_new = id_cast<Collection *>(BKE_id_copy_ex(
        bmain, &collection->id, nullptr, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
    id_us_min(&collection_new->id);
    collection = id_cast<Collection *>(ID_NEW_SET(collection, collection_new));
  }

  /* We do not remap to new objects here, this is done in separate step. */
  for (CollectionObject &cob : collection->gobject) {
    Object *ob = cob.ob;
    /* an object may be in more than one collection */
    if ((ob->id.newid == nullptr) && ((ob->flag & flag) == flag)) {
      if (!ID_IS_LINKED(ob) && BKE_object_scenes_users_get(bmain, ob) > 1) {
        ID_NEW_SET(
            ob,
            BKE_id_copy_ex(bmain, &ob->id, nullptr, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
        id_us_min(ob->id.newid);
      }
    }
  }

  /* Since master collection has already be duplicated as part of scene copy,    * we do not
   * duplicate it here. However, this means its children need to be re-added manually here,    *
   * otherwise their parent lists are empty (which will lead to crashes, see #63101). */
  CollectionChild *child_next, *child = collection->children.first();
  CollectionChild *orig_child_last = collection->children.last();
  for (; child != nullptr; child = child_next) {
    child_next = child->next;
    Collection *collection_child_new = single_object_users_collection(
        bmain, scene, child->collection, flag, copy_collections, false);

    if (is_master_collection && copy_collections && child->collection != collection_child_new) {
      /* We do not want a collection sync here, our collections are in a complete uninitialized
       * state currently. With current code, that would lead to a memory leak - because of
       * reasons. It would be a useless loss of computing anyway, since caller has to fully
       * refresh view-layers/collections caching at the end. */
      BKE_collection_child_add_no_sync(bmain, collection, collection_child_new);
      BLI_remlink(&collection->children, child);
      MEM_delete(child);
      if (child == orig_child_last) {
        break;
      }
    }
  }

  return collection;
}

/* Warning, sets ID->newid pointers of objects and collections, but does not clear them. */
static void single_object_users(
    Main *bmain, Scene *scene, View3D *v3d, const int flag, const bool copy_collections)
{
  /* duplicate all the objects of the scene (and matching collections, if required). */
  Collection *master_collection = scene->master_collection;
  single_object_users_collection(bmain, scene, master_collection, flag, copy_collections, true);

  /* Will also handle the master collection. */
  BKE_libblock_relink_to_newid(bmain, &scene->id, 0);

  /* Collection and object pointers in collections */
  libblock_relink_collection(bmain, scene->master_collection, false);

  /* We also have to handle runtime things in UI. */
  if (v3d) {
    ID_NEW_REMAP(v3d->camera);
  }

  /* Making single user may affect other scenes if they share
   * with current one some collections in their ViewLayer. */
  BKE_main_collection_sync_remap(bmain);
}

void object_single_user_make(Main *bmain, Scene *scene, Object *ob)
{
  FOREACH_SCENE_OBJECT_BEGIN (scene, ob_iter) {
    ob_iter->flag &= ~OB_DONE;
  }
  FOREACH_SCENE_OBJECT_END;

  /* tag only the one object */
  ob->flag |= OB_DONE;

  single_object_users(bmain, scene, nullptr, OB_DONE, false);
  BKE_main_id_newptr_and_tag_clear(bmain);
}

static void single_obdata_users(
    Main *bmain, Scene *scene, ViewLayer *view_layer, View3D *v3d, const int flag)
{
  ID *id;

  FOREACH_OBJECT_FLAG_BEGIN (bmain, scene, view_layer, v3d, flag, ob) {
    if (BKE_id_is_editable(bmain, &ob->id)) {
      id = ob->data;
      if (single_data_needs_duplication(id)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

        switch (ob->type) {
          case OB_EMPTY:
            ob->data = ID_NEW_SET(ob->data,
                                  BKE_id_copy_ex(bmain,
                                                 static_cast<const ID *>(ob->data),
                                                 nullptr,
                                                 LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_LAMP:
            ob->data = ID_NEW_SET(ob->data,
                                  BKE_id_copy_ex(bmain,
                                                 static_cast<const ID *>(ob->data),
                                                 nullptr,
                                                 LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_CAMERA: {
            ob->data = ID_NEW_SET(ob->data,
                                  BKE_id_copy_ex(bmain,
                                                 static_cast<const ID *>(ob->data),
                                                 nullptr,
                                                 LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            Camera *cam = id_cast<Camera *>(ob->data);
            ID_NEW_REMAP(cam->dof.focus_object);
            break;
          }
          case OB_MESH:
            /* Needed to remap texcomesh below. */
            ob->data = ID_NEW_SET(ob->data,
                                  BKE_id_copy_ex(bmain,
                                                 static_cast<const ID *>(ob->data),
                                                 nullptr,
                                                 LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_MBALL:
            ob->data = ID_NEW_SET(ob->data,
                                  BKE_id_copy_ex(bmain,
                                                 static_cast<const ID *>(ob->data),
                                                 nullptr,
                                                 LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_CURVES_LEGACY:
          case OB_SURF:
          case OB_FONT: {
            ob->data = ID_NEW_SET(ob->data,
                                  BKE_id_copy_ex(bmain,
                                                 static_cast<const ID *>(ob->data),
                                                 nullptr,
                                                 LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            Curve *cu = id_cast<Curve *>(ob->data);
            ID_NEW_REMAP(cu->bevobj);
            ID_NEW_REMAP(cu->taperobj);
            break;
          }
          case OB_LATTICE:
            ob->data = ID_NEW_SET(ob->data,
                                  BKE_id_copy_ex(bmain,
                                                 static_cast<const ID *>(ob->data),
                                                 nullptr,
                                                 LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_ARMATURE:
            DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
            ob->data = ID_NEW_SET(ob->data,
                                  BKE_id_copy_ex(bmain,
                                                 static_cast<const ID *>(ob->data),
                                                 nullptr,
                                                 LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_SPEAKER:
            ob->data = ID_NEW_SET(ob->data,
                                  BKE_id_copy_ex(bmain,
                                                 static_cast<const ID *>(ob->data),
                                                 nullptr,
                                                 LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_LIGHTPROBE:
            ob->data = ID_NEW_SET(ob->data,
                                  BKE_id_copy_ex(bmain,
                                                 static_cast<const ID *>(ob->data),
                                                 nullptr,
                                                 LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_CURVES:
            ob->data = ID_NEW_SET(ob->data,
                                  BKE_id_copy_ex(bmain,
                                                 static_cast<const ID *>(ob->data),
                                                 nullptr,
                                                 LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_POINTCLOUD:
            ob->data = ID_NEW_SET(ob->data,
                                  BKE_id_copy_ex(bmain,
                                                 static_cast<const ID *>(ob->data),
                                                 nullptr,
                                                 LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_VOLUME:
            ob->data = ID_NEW_SET(ob->data,
                                  BKE_id_copy_ex(bmain,
                                                 static_cast<const ID *>(ob->data),
                                                 nullptr,
                                                 LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_GREASE_PENCIL:
            ob->data = ID_NEW_SET(ob->data,
                                  BKE_id_copy_ex(bmain,
                                                 static_cast<const ID *>(ob->data),
                                                 nullptr,
                                                 LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          default:
            printf("ERROR %s: cannot copy %s\n", __func__, id->name);
            BLI_assert_msg(0, "This should never happen.");

            /* We need to end the FOREACH_OBJECT_FLAG_BEGIN iterator to prevent memory leak. */
            BKE_scene_objects_iterator_end(&iter_macro);
            return;
        }

        id_us_min(id);
      }
    }
  }
  FOREACH_OBJECT_FLAG_END;

  Mesh *mesh = bmain->meshes.first();
  while (mesh) {
    ID_NEW_REMAP(mesh->texcomesh);
    mesh = static_cast<Mesh *>(mesh->id.next);
  }
}

void single_obdata_user_make(Main *bmain, Scene *scene, Object *ob)
{
  FOREACH_SCENE_OBJECT_BEGIN (scene, ob_iter) {
    ob_iter->flag &= ~OB_DONE;
  }
  FOREACH_SCENE_OBJECT_END;

  /* Tag only the one object. */
  ob->flag |= OB_DONE;

  single_obdata_users(bmain, scene, nullptr, nullptr, OB_DONE);
}

static void single_object_action_users(
    Main *bmain, Scene *scene, ViewLayer *view_layer, View3D *v3d, const int flag)
{
  FOREACH_OBJECT_FLAG_BEGIN (bmain, scene, view_layer, v3d, flag, ob) {
    if (BKE_id_is_editable(bmain, &ob->id)) {
      AnimData *adt = BKE_animdata_from_id(&ob->id);
      if (adt == nullptr) {
        continue;
      }

      ID *id_act = id_cast<ID *>(adt->action);
      if (single_data_needs_duplication(id_act)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
        BKE_animdata_duplicate_id_action(bmain, &ob->id, USER_DUP_ACT | USER_DUP_LINKED_ID);
      }
    }
  }
  FOREACH_OBJECT_FLAG_END;
}

static void single_objectdata_action_users(
    Main *bmain, Scene *scene, ViewLayer *view_layer, View3D *v3d, const int flag)
{
  FOREACH_OBJECT_FLAG_BEGIN (bmain, scene, view_layer, v3d, flag, ob) {
    if (BKE_id_is_editable(bmain, &ob->id) && ob->data != nullptr) {
      ID *id_obdata = ob->data;
      AnimData *adt = BKE_animdata_from_id(id_obdata);
      if (adt == nullptr) {
        continue;
      }

      ID *id_act = id_cast<ID *>(adt->action);
      if (single_data_needs_duplication(id_act)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
        BKE_animdata_duplicate_id_action(bmain, id_obdata, USER_DUP_ACT | USER_DUP_LINKED_ID);
      }
    }
  }
  FOREACH_OBJECT_FLAG_END;
}

static void single_mat_users(
    Main *bmain, Scene *scene, ViewLayer *view_layer, View3D *v3d, const int flag)
{
  Material *ma, *man;
  int a;

  FOREACH_OBJECT_FLAG_BEGIN (bmain, scene, view_layer, v3d, flag, ob) {
    if (BKE_id_is_editable(bmain, &ob->id)) {
      const bool is_obdata_editable = ob->data ? BKE_id_is_editable(bmain, ob->data) : false;
      for (a = 1; a <= ob->totcol; a++) {
        if (!ob->matbits[a - 1] && !is_obdata_editable) {
          /* Cannot edit material usage of obdata if it's not editable (e.g. local object using a
           * linked mesh). See also #161211. */
          continue;
        }
        ma = BKE_object_material_get(ob, short(a));
        if (single_data_needs_duplication(&ma->id)) {
          man = id_cast<Material *>(
              BKE_id_copy_ex(bmain, &ma->id, nullptr, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
          man->id.us = 0;
          BKE_object_material_assign(bmain, ob, man, short(a), BKE_MAT_ASSIGN_EXISTING);
        }
      }
    }
  }
  FOREACH_OBJECT_FLAG_END;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Make Local Operator
 * \{ */

enum {
  MAKE_LOCAL_SELECT_OB = 1,
  MAKE_LOCAL_SELECT_OBDATA = 2,
  MAKE_LOCAL_SELECT_OBDATA_MATERIAL = 3,
  MAKE_LOCAL_ALL = 4,
};

static int tag_localizable_looper(LibraryIDLinkCallbackData *cb_data)
{
  ID **id_pointer = cb_data->id_pointer;
  if (*id_pointer) {
    (*id_pointer)->tag &= ~ID_TAG_DOIT;
  }

  return IDWALK_RET_NOP;
}

static void tag_localizable_objects(bContext *C, const int mode)
{
  Main *bmain = CTX_data_main(C);

  BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);

  /* Set ID_TAG_DOIT flag for all selected objects, so next we can check whether
   * object is gonna to become local or not.
   */
  CTX_DATA_BEGIN (C, Object *, object, selected_objects) {
    object->id.tag |= ID_TAG_DOIT;

    /* If obdata is also going to become local, mark it as such too. */
    if (mode == MAKE_LOCAL_SELECT_OBDATA && object->data) {
      ID *data_id = object->data;
      data_id->tag |= ID_TAG_DOIT;
    }
  }
  CTX_DATA_END;

  /* Also forbid making objects local if other library objects are using
   * them for modifiers or constraints.
   *
   * FIXME This is ignoring all other linked ID types potentially using the selected tagged
   * objects! Probably works fine in most 'usual' cases though.
   */
  for (Object *object = bmain->objects.first(); object;
       object = static_cast<Object *>(object->id.next))
  {
    if ((object->id.tag & ID_TAG_DOIT) == 0 && ID_IS_LINKED(object)) {
      BKE_library_foreach_ID_link(
          nullptr, &object->id, tag_localizable_looper, nullptr, IDWALK_READONLY);
    }
    if (object->data) {
      ID *data_id = object->data;
      if ((data_id->tag & ID_TAG_DOIT) == 0 && ID_IS_LINKED(data_id)) {
        BKE_library_foreach_ID_link(
            nullptr, data_id, tag_localizable_looper, nullptr, IDWALK_READONLY);
      }
    }
  }

  /* TODO(sergey): Drivers targets? */
}

/**
 * Instance indirectly referenced zero user objects,  * otherwise they're lost on reload, see
 * #40595.
 */
static bool make_local_all__instance_indirect_unused(Main *bmain,
                                                     const Scene *scene,
                                                     ViewLayer *view_layer,
                                                     Collection *collection)
{
  Object *ob;
  bool changed = false;

  for (ob = bmain->objects.first(); ob; ob = static_cast<Object *>(ob->id.next)) {
    if (ID_IS_LINKED(ob) && (ob->id.us == 0)) {
      Base *base;

      id_us_plus(&ob->id);

      BKE_collection_object_add(bmain, collection, ob);
      BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
      base = BKE_view_layer_base_find(view_layer, ob);
      base_select(base, BA_SELECT);
      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

      changed = true;
    }
  }

  return changed;
}

static void make_local_animdata_tag_strips(ListBaseT<NlaStrip> *strips)
{
  for (NlaStrip &strip : *strips) {
    if (strip.act) {
      strip.act->id.tag &= ~ID_TAG_PRE_EXISTING;
    }

    make_local_animdata_tag_strips(&strip.strips);
  }
}

/* Tag all actions used by given animdata to be made local. */
static void make_local_animdata_tag(AnimData *adt)
{
  if (adt) {
    /* Actions - Active and Temp */
    if (adt->action) {
      adt->action->id.tag &= ~ID_TAG_PRE_EXISTING;
    }
    if (adt->tmpact) {
      adt->tmpact->id.tag &= ~ID_TAG_PRE_EXISTING;
    }

    /* Drivers */
    /* TODO: need to handle the ID-targets too? */

    /* NLA Data */
    for (NlaTrack &nlt : adt->nla_tracks) {
      make_local_animdata_tag_strips(&nlt.strips);
    }
  }
}

static void make_local_material_tag(Material *ma)
{
  if (ma) {
    ma->id.tag &= ~ID_TAG_PRE_EXISTING;
    make_local_animdata_tag(BKE_animdata_from_id(&ma->id));

    /* About node-trees: root one is made local together with material,
     * others we keep linked (for now). */
  }
}

static wmOperatorStatus make_local_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Material *ma, ***matarar;
  const int mode = RNA_enum_get(op->ptr, "type");
  int a;

  /* NOTE: we (ab)use ID_TAG_PRE_EXISTING to cherry pick which ID to make local... */
  if (mode == MAKE_LOCAL_ALL) {
    const Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Collection *collection = CTX_data_collection(C);

    BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, false);

    /* De-select so the user can differentiate newly instanced from existing objects. */
    BKE_view_layer_base_deselect_all(*bmain, scene, view_layer);

    if (make_local_all__instance_indirect_unused(bmain, scene, view_layer, collection)) {
      BKE_report(op->reports,
                 RPT_INFO,
                 "Orphan library objects added to the current scene to avoid loss");
    }
  }
  else {
    BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, true);
    tag_localizable_objects(C, mode);

    CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
      if ((ob->id.tag & ID_TAG_DOIT) == 0) {
        continue;
      }

      ob->id.tag &= ~ID_TAG_PRE_EXISTING;
      make_local_animdata_tag(BKE_animdata_from_id(&ob->id));
      for (ParticleSystem &psys : ob->particlesystem) {
        psys.part->id.tag &= ~ID_TAG_PRE_EXISTING;
      }

      if (mode == MAKE_LOCAL_SELECT_OBDATA_MATERIAL) {
        for (a = 0; a < ob->totcol; a++) {
          ma = ob->mat[a];
          if (ma) {
            make_local_material_tag(ma);
          }
        }

        matarar = BKE_object_material_array_p(ob);
        if (matarar) {
          for (a = 0; a < ob->totcol; a++) {
            ma = (*matarar)[a];
            if (ma) {
              make_local_material_tag(ma);
            }
          }
        }
      }

      if (ELEM(mode, MAKE_LOCAL_SELECT_OBDATA, MAKE_LOCAL_SELECT_OBDATA_MATERIAL) &&
          ob->data != nullptr)
      {
        ID *ob_data = ob->data;
        ob_data->tag &= ~ID_TAG_PRE_EXISTING;
        make_local_animdata_tag(BKE_animdata_from_id(ob_data));
      }
    }
    CTX_DATA_END;
  }

  BKE_library_make_local(
      bmain, nullptr, nullptr, true, false, true); /* nullptr is all libraries. */

  WM_event_add_notifier(C, NC_WINDOW, nullptr);
  return OPERATOR_FINISHED;
}

void OBJECT_OT_make_local(wmOperatorType *ot)
{
  static const EnumPropertyItem type_items[] = {
      {MAKE_LOCAL_SELECT_OB, "SELECT_OBJECT", 0, "Selected Objects", ""},
      {MAKE_LOCAL_SELECT_OBDATA, "SELECT_OBDATA", 0, "Selected Objects and Data", ""},
      {MAKE_LOCAL_SELECT_OBDATA_MATERIAL,
       "SELECT_OBDATA_MATERIAL",
       0,
       "Selected Objects, Data and Materials",
       ""},
      {MAKE_LOCAL_ALL, "ALL", 0, "All", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Make Local";
  ot->description = "Make library linked data-blocks local to this file";
  ot->idname = "OBJECT_OT_make_local";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = make_local_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Make Library Override Operator
 * \{ */

static bool make_override_library_object_overridable_check(Main *bmain, Object *object)
{
  /* An object is actually overridable only if it is in at least one local collection.
   * Unfortunately 'direct link' flag is not enough here. */
  for (Collection &collection : bmain->collections) {
    if (!ID_IS_LINKED(&collection) && BKE_collection_has_object(&collection, object)) {
      return true;
    }
  }
  for (Scene &scene : bmain->scenes) {
    if (!ID_IS_LINKED(&scene) && BKE_collection_has_object(scene.master_collection, object)) {
      return true;
    }
  }
  return false;
}

static wmOperatorStatus make_override_library_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obact = CTX_data_active_object(C);
  ID *id_root = nullptr;
  bool is_override_instancing_object = false;

  bool user_overrides_from_selected_objects = false;

  if (!ID_IS_LINKED(obact) && obact->instance_collection != nullptr &&
      ID_IS_LINKED(obact->instance_collection))
  {
    if (!ID_IS_OVERRIDABLE_LIBRARY(obact->instance_collection)) {
      BKE_reportf(op->reports,
                  RPT_ERROR_INVALID_INPUT,
                  "Collection '%s' (instantiated by the active object) is not overridable",
                  obact->instance_collection->id.name + 2);
      return OPERATOR_CANCELLED;
    }

    id_root = &obact->instance_collection->id;
    is_override_instancing_object = true;
    user_overrides_from_selected_objects = false;
  }
  else if (!make_override_library_object_overridable_check(bmain, obact)) {
    const int i = RNA_property_int_get(op->ptr, op->type->prop);
    const uint collection_session_uid = *(reinterpret_cast<const uint *>(&i));
    if (collection_session_uid == MAIN_ID_SESSION_UID_UNSET) {
      BKE_reportf(op->reports,
                  RPT_ERROR_INVALID_INPUT,
                  "Could not find an overridable root hierarchy for object '%s'",
                  obact->id.name + 2);
      return OPERATOR_CANCELLED;
    }
    Collection *collection = static_cast<Collection *>(
        BLI_listbase_bytes_find(&bmain->collections,
                                &collection_session_uid,
                                sizeof(collection_session_uid),
                                offsetof(ID, session_uid)));
    id_root = &collection->id;
    user_overrides_from_selected_objects = true;
  }
  /* Else, poll func ensures us that ID_IS_LINKED(obact) is true, or that it is already an
   * existing liboverride. */
  else {
    BLI_assert(ID_IS_LINKED(obact) || ID_IS_OVERRIDE_LIBRARY_REAL(obact));
    id_root = &obact->id;
    user_overrides_from_selected_objects = true;
  }

  /* Make already existing selected liboverrides editable. */
  bool is_active_override = false;
  FOREACH_SELECTED_OBJECT_BEGIN (view_layer, CTX_wm_view3d(C), ob_iter) {
    if (ID_IS_OVERRIDE_LIBRARY_REAL(ob_iter) && !ID_IS_LINKED(ob_iter)) {
      ob_iter->id.override_library->flag &= ~LIBOVERRIDE_FLAG_SYSTEM_DEFINED;
      is_active_override = is_active_override || (&ob_iter->id == id_root);
      DEG_id_tag_update(&ob_iter->id, ID_RECALC_SYNC_TO_EVAL);
    }
  }
  FOREACH_SELECTED_OBJECT_END;
  /* If the active object is a liboverride, there is no point going further, since in the weird
   * case where some other selected objects would be linked ones, there is no way to properly
   * create overrides for them currently.
   *
   * Could be added later if really needed, but would rather avoid that extra complexity here. */
  if (is_active_override) {
    return OPERATOR_FINISHED;
  }

  /** Currently there is no 'all editable' option from the 3DView. */
  const bool do_fully_editable = false;

  std::unique_ptr<Set<uint32_t>> user_overrides_objects_uids =
      do_fully_editable ? nullptr : std::make_unique<Set<uint32_t>>();

  if (do_fully_editable) {
    /* Pass. */
  }
  else if (user_overrides_from_selected_objects) {
    /* Only selected objects can be 'user overrides'. */
    FOREACH_SELECTED_OBJECT_BEGIN (view_layer, CTX_wm_view3d(C), ob_iter) {
      user_overrides_objects_uids->add(ob_iter->id.session_uid);
    }
    FOREACH_SELECTED_OBJECT_END;
  }
  else {
    /* Only armatures inside the root collection (and their children) can be 'user overrides'. */
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN ((Collection *)id_root, ob_iter) {
      if (ob_iter->type == OB_ARMATURE) {
        user_overrides_objects_uids->add(ob_iter->id.session_uid);
      }
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }

  BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);

  /* For the time being, replace selected linked objects by their overrides in all collections.
   * While this may not be the absolute best behavior in all cases, in most common one this
   * should match the expected result. */
  if (user_overrides_objects_uids != nullptr) {
    for (Collection &coll_iter : bmain->collections) {
      if (ID_IS_LINKED(&coll_iter)) {
        continue;
      }
      for (CollectionObject &coll_ob_iter : coll_iter.gobject) {
        if (user_overrides_objects_uids->contains(coll_ob_iter.ob->id.session_uid)) {
          /* Tag for remapping when creating overrides. */
          coll_iter.id.tag |= ID_TAG_DOIT;
          break;
        }
      }
    }
    /* Also tag the Scene itself for remapping when creating overrides (includes the scene's master
     * collection too). */
    scene->id.tag |= ID_TAG_DOIT;
  }

  ID *id_root_override;
  const bool success = BKE_lib_override_library_create(bmain,
                                                       scene,
                                                       view_layer,
                                                       nullptr,
                                                       id_root,
                                                       id_root,
                                                       &obact->id,
                                                       &id_root_override,
                                                       do_fully_editable);

  if (!do_fully_editable) {
    /* Define liboverrides from selected/validated objects as user defined. */
    ID *id_hierarchy_root_override = id_root_override->override_library->hierarchy_root;
    ID *id_iter;
    FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
      if (ID_IS_LINKED(id_iter) || !ID_IS_OVERRIDE_LIBRARY_REAL(id_iter) ||
          id_iter->override_library->hierarchy_root != id_hierarchy_root_override)
      {
        continue;
      }
      if (user_overrides_objects_uids->contains(id_iter->override_library->reference->session_uid))
      {
        id_iter->override_library->flag &= ~LIBOVERRIDE_FLAG_SYSTEM_DEFINED;
      }
    }
    FOREACH_MAIN_ID_END;
  }

  if (success) {
    if (is_override_instancing_object) {
      /* Remove the instance empty from this scene, the items now have an overridden collection
       * instead. */
      base_free_and_unlink(bmain, scene, obact);
    }
    else {
      /* Remove the found root ID from the view layer. */
      switch (GS(id_root->name)) {
        case ID_GR: {
          Collection *collection_root = id_cast<Collection *>(id_root);
          for (CollectionParent &collection_parent :
               collection_root->runtime->parents.items_mutable())
          {
            if (ID_IS_LINKED(collection_parent.collection) ||
                !BKE_view_layer_has_collection(view_layer, collection_parent.collection))
            {
              continue;
            }
            BKE_collection_child_remove(bmain, collection_parent.collection, collection_root);
          }
          break;
        }
        case ID_OB: {
          /* TODO: Not sure how well we can handle this case, when we don't have the collections
           * as reference containers... */
          break;
        }
        default:
          break;
      }
    }
  }

  DEG_id_tag_update(&CTX_data_scene(C)->id, ID_RECALC_BASE_FLAGS | ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_WINDOW, nullptr);
  WM_event_add_notifier(C, NC_WM | ND_LIB_OVERRIDE_CHANGED, nullptr);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  return success ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

/* Set the object to override. */
static wmOperatorStatus make_override_library_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent * /*event*/)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obact = context_active_object(C);

  /* Sanity checks. */
  if (!scene || ID_IS_LINKED(scene) || !obact) {
    return OPERATOR_CANCELLED;
  }

  if ((!ID_IS_LINKED(obact) && obact->instance_collection != nullptr &&
       ID_IS_OVERRIDABLE_LIBRARY(obact->instance_collection)) ||
      make_override_library_object_overridable_check(bmain, obact))
  {
    return make_override_library_exec(C, op);
  }

  if (!ID_IS_LINKED(obact)) {
    if (ID_IS_OVERRIDE_LIBRARY_REAL(obact)) {
      return make_override_library_exec(C, op);
    }
    BKE_report(op->reports, RPT_ERROR, "Cannot make library override from a local object");
    return OPERATOR_CANCELLED;
  }

  VectorSet<Collection *> potential_root_collections;
  for (Collection &collection : bmain->collections) {
    /* Only check for linked collections from the same library, in the current view-layer. */
    if (!ID_IS_LINKED(&collection.id) || collection.id.lib != obact->id.lib ||
        !BKE_view_layer_has_collection(view_layer, &collection))
    {
      continue;
    }
    if (!BKE_collection_has_object_recursive(&collection, obact)) {
      continue;
    }
    if (potential_root_collections.is_empty()) {
      potential_root_collections.add_new(&collection);
    }
    else {
      bool has_parents_in_potential_roots = false;
      bool is_potential_root = false;
      for (auto *collection_root_iter : potential_root_collections) {
        if (BKE_collection_has_collection(collection_root_iter, &collection)) {
          BLI_assert_msg(!BKE_collection_has_collection(&collection, collection_root_iter),
                         "Invalid loop in collection hierarchy");
          /* Current potential root is already 'better' (higher up in the collection hierarchy)
           * than current collection, nothing else to do. */
          has_parents_in_potential_roots = true;
        }
        else if (BKE_collection_has_collection(&collection, collection_root_iter)) {
          BLI_assert_msg(!BKE_collection_has_collection(collection_root_iter, &collection),
                         "Invalid loop in collection hierarchy");
          /* Current potential root is in the current collection's hierarchy, so the later is a
           * better candidate as root collection. */
          is_potential_root = true;
          potential_root_collections.remove(collection_root_iter);
        }
        else {
          /* Current potential root is not found in current collection's hierarchy, so the later
           * is a potential candidate as root collection. */
          is_potential_root = true;
        }
      }
      /* Only add the current collection as potential root if it is not a descendant of any
       * already known potential root collections. */
      if (is_potential_root && !has_parents_in_potential_roots) {
        potential_root_collections.add_new(&collection);
      }
    }
  }

  if (potential_root_collections.is_empty()) {
    RNA_property_int_set(op->ptr, op->type->prop, MAIN_ID_SESSION_UID_UNSET);
    return make_override_library_exec(C, op);
  }
  if (potential_root_collections.size() == 1) {
    Collection *collection_root = potential_root_collections.pop();
    RNA_property_int_set(
        op->ptr, op->type->prop, *(reinterpret_cast<int *>(&collection_root->id.session_uid)));
    return make_override_library_exec(C, op);
  }

  BKE_reportf(op->reports,
              RPT_ERROR,
              "Too many potential root collections (%d) for the override hierarchy, "
              "please use the Outliner instead",
              int(potential_root_collections.size()));
  return OPERATOR_CANCELLED;
}

static bool make_override_library_poll(bContext *C)
{
  Base *base_act = CTX_data_active_base(C);
  /* If the active object is not selected, do nothing (operators rely on selection too, they will
   * misbehave if the active object is not also selected, see e.g. #120701). */
  if ((base_act == nullptr) || ((base_act->flag & BASE_SELECTED) == 0)) {
    return false;
  }

  /* Object must be directly linked to be overridable. */
  Object *obact = base_act->object;
  return (
      ED_operator_objectmode(C) && obact != nullptr &&
      (ID_IS_LINKED(obact) || ID_IS_OVERRIDE_LIBRARY(obact) ||
       (obact->instance_collection != nullptr &&
        ID_IS_OVERRIDABLE_LIBRARY(obact->instance_collection) && !ID_IS_OVERRIDE_LIBRARY(obact))));
}

void OBJECT_OT_make_override_library(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Library Override";
  ot->description =
      "Create a local override of the selected linked objects, and their hierarchy of "
      "dependencies";
  ot->idname = "OBJECT_OT_make_override_library";

  /* API callbacks. */
  ot->invoke = make_override_library_invoke;
  ot->exec = make_override_library_exec;
  ot->poll = make_override_library_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_int(ot->srna,
                     "collection",
                     MAIN_ID_SESSION_UID_UNSET,
                     INT_MIN,
                     INT_MAX,
                     "Override Collection",
                     "Session UID of the directly linked collection containing the selected "
                     "object, to make an override from",
                     INT_MIN,
                     INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  ot->prop = prop;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Reset Library Override Operator
 * \{ */

static bool reset_clear_override_library_poll(bContext *C)
{
  Base *base_act = CTX_data_active_base(C);
  /* If the active object is not selected, do nothing (operators rely on selection too, they will
   * misbehave if the active object is not also selected, see e.g. #120701). */
  if ((base_act == nullptr) || ((base_act->flag & BASE_SELECTED) == 0)) {
    return false;
  }

  /* Object must be local and an override. */
  Object *obact = base_act->object;
  return (ED_operator_objectmode(C) && obact != nullptr && !ID_IS_LINKED(obact) &&
          ID_IS_OVERRIDE_LIBRARY(obact));
}

static wmOperatorStatus reset_override_library_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);

  /* Reset all selected liboverrides. */
  FOREACH_SELECTED_OBJECT_BEGIN (CTX_data_view_layer(C), CTX_wm_view3d(C), ob_iter) {
    if (ID_IS_OVERRIDE_LIBRARY_REAL(ob_iter) && !ID_IS_LINKED(ob_iter)) {
      BKE_lib_override_library_id_reset(bmain, &ob_iter->id, false);
    }
  }
  FOREACH_SELECTED_OBJECT_END;

  WM_event_add_notifier(C, NC_WINDOW, nullptr);
  WM_event_add_notifier(C, NC_WM | ND_LIB_OVERRIDE_CHANGED, nullptr);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_reset_override_library(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset Library Override";
  ot->description = "Reset the selected local overrides to their linked references values";
  ot->idname = "OBJECT_OT_reset_override_library";

  /* API callbacks. */
  ot->exec = reset_override_library_exec;
  ot->poll = reset_clear_override_library_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Clear Library Override Operator
 * \{ */

static wmOperatorStatus clear_override_library_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Scene *scene = CTX_data_scene(C);
  LinkNode *todo_objects = nullptr, *todo_object_iter;

  /* Make already existing selected liboverrides editable. */
  FOREACH_SELECTED_OBJECT_BEGIN (view_layer, CTX_wm_view3d(C), ob_iter) {
    if (ID_IS_LINKED(ob_iter)) {
      continue;
    }
    BLI_linklist_prepend_alloca(&todo_objects, ob_iter);
  }
  FOREACH_SELECTED_OBJECT_END;

  for (todo_object_iter = todo_objects; todo_object_iter != nullptr;
       todo_object_iter = todo_object_iter->next)
  {
    Object *ob_iter = static_cast<Object *>(todo_object_iter->link);
    if (BKE_lib_override_library_is_hierarchy_leaf(bmain, &ob_iter->id)) {
      bool do_remap_active = false;
      BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
      if (BKE_view_layer_active_object_get(view_layer) == ob_iter) {
        do_remap_active = true;
      }
      BKE_libblock_remap(bmain,
                         &ob_iter->id,
                         ob_iter->id.override_library->reference,
                         ID_REMAP_SKIP_INDIRECT_USAGE);
      if (do_remap_active) {
        BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
        Object *ref_object = id_cast<Object *>(ob_iter->id.override_library->reference);
        Base *basact = BKE_view_layer_base_find(view_layer, ref_object);
        if (basact != nullptr) {
          view_layer->basact = basact;
        }
        DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
      }
      BKE_id_delete(bmain, &ob_iter->id);
    }
    else {
      BKE_lib_override_library_id_reset(bmain, &ob_iter->id, true);
    }
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS | ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_WINDOW, nullptr);
  WM_event_add_notifier(C, NC_WM | ND_LIB_OVERRIDE_CHANGED, nullptr);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_clear_override_library(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Library Override";
  ot->description =
      "Delete the selected local overrides and relink their usages to the linked data-blocks if "
      "possible, else reset them and mark them as non editable";
  ot->idname = "OBJECT_OT_clear_override_library";

  /* API callbacks. */
  ot->exec = clear_override_library_exec;
  ot->poll = reset_clear_override_library_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Make Single User Operator
 * \{ */

enum {
  MAKE_SINGLE_USER_ALL = 1,
  MAKE_SINGLE_USER_SELECTED = 2,
};

static wmOperatorStatus make_single_user_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C); /* ok if this is nullptr */
  const int flag = (RNA_enum_get(op->ptr, "type") == MAKE_SINGLE_USER_SELECTED) ? SELECT : 0;
  const bool copy_collections = false;
  bool update_deps = false;

  if (RNA_boolean_get(op->ptr, "object")) {
    if (flag == SELECT) {
      BKE_view_layer_selected_objects_tag(*bmain, scene, view_layer, OB_DONE);
      single_object_users(bmain, scene, v3d, OB_DONE, copy_collections);
    }
    else {
      single_object_users(bmain, scene, v3d, 0, copy_collections);
    }

    /* needed since object relationships may have changed */
    update_deps = true;
  }

  if (RNA_boolean_get(op->ptr, "obdata")) {
    single_obdata_users(bmain, scene, view_layer, v3d, flag);

    /* Needed since some IDs were remapped? (incl. mesh->texcomesh, see #73797). */
    update_deps = true;
  }

  if (RNA_boolean_get(op->ptr, "material")) {
    single_mat_users(bmain, scene, view_layer, v3d, flag);
  }

  if (RNA_boolean_get(op->ptr, "animation")) {
    single_object_action_users(bmain, scene, view_layer, v3d, flag);
  }

  if (RNA_boolean_get(op->ptr, "obdata_animation")) {
    single_objectdata_action_users(bmain, scene, view_layer, v3d, flag);
  }

  BKE_main_id_newptr_and_tag_clear(bmain);

  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  if (update_deps) {
    DEG_relations_tag_update(bmain);
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus make_single_user_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  return WM_operator_props_popup_confirm_ex(
      C, op, event, IFACE_("Make Selected Objects Single-User"), IFACE_("Make Single"));
}

void OBJECT_OT_make_single_user(wmOperatorType *ot)
{
  static const EnumPropertyItem type_items[] = {
      {MAKE_SINGLE_USER_SELECTED, "SELECTED_OBJECTS", 0, "Selected Objects", ""},
      {MAKE_SINGLE_USER_ALL, "ALL", 0, "All", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Make Single User";
  ot->description = "Make linked data local to each object";
  ot->idname = "OBJECT_OT_make_single_user";

  /* Note that the invoke callback is only used from operator search,    * otherwise this does
   * nothing by default. */

  /* API callbacks. */
  ot->invoke = make_single_user_invoke;
  ot->exec = make_single_user_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", type_items, MAKE_SINGLE_USER_SELECTED, "Type", "");

  RNA_def_boolean(ot->srna, "object", false, "Object", "Make single user objects");
  RNA_def_boolean(ot->srna, "obdata", false, "Object Data", "Make single user object data");
  RNA_def_boolean(
      ot->srna, "material", false, "Materials", "Make materials local to each data-block");
  RNA_def_boolean(ot->srna,
                  "animation",
                  false,
                  "Object Animation",
                  "Make object animation data local to each object");
  RNA_def_boolean(ot->srna,
                  "obdata_animation",
                  false,
                  "Object Data Animation",
                  "Make object data (mesh, curve etc.) animation data local to each object");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Drop Named Material on Object Operator
 * \{ */

std::string drop_named_material_tooltip(bContext *C, StringRef name, const int mval[2])
{
  int mat_slot = 0;
  Object *ob = ED_view3d_give_material_slot_under_cursor(C, mval, &mat_slot);
  if (ob == nullptr) {
    return {};
  }
  mat_slot = max_ii(mat_slot, 1);

  Material *prev_mat = BKE_object_material_get(ob, mat_slot);

  if (prev_mat) {
    return fmt::format(fmt::runtime(TIP_("Drop {} on {} (slot {}, replacing {})")),
                       name,
                       ob->id.name + 2,
                       mat_slot,
                       prev_mat->id.name + 2);
  }
  return fmt::format(
      fmt::runtime(TIP_("Drop {} on {} (slot {})")), name, ob->id.name + 2, mat_slot);
}

static wmOperatorStatus drop_named_material_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  int mat_slot = 0;
  Object *ob = ED_view3d_give_material_slot_under_cursor(C, event->mval, &mat_slot);
  mat_slot = max_ii(mat_slot, 1);

  Material *ma = id_cast<Material *>(
      WM_operator_properties_id_lookup_from_name_or_session_uid(bmain, op->ptr, ID_MA));

  if (ob == nullptr || ma == nullptr) {
    return OPERATOR_CANCELLED;
  }

  int assign_type = BKE_MAT_ASSIGN_USERPREF;
  /* When trying to assign to non-editable object data, assign to the object instead. */
  if (BKE_id_is_editable(bmain, &ob->id) && ob->data && !BKE_id_is_editable(bmain, ob->data)) {
    assign_type = BKE_MAT_ASSIGN_OBJECT;
  }

  BKE_object_material_assign(CTX_data_main(C), ob, ma, mat_slot, assign_type);

  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);

  WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, ob);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);
  WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_LINKS, ma);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_drop_named_material(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Drop Named Material on Object";
  ot->idname = "OBJECT_OT_drop_named_material";

  /* API callbacks. */
  ot->invoke = drop_named_material_invoke;
  ot->poll = ED_operator_objectmode_with_view3d_poll_msg;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  WM_operator_properties_id_lookup(ot, true);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Drop Geometry Nodes on Object Operator
 * \{ */

std::string drop_geometry_nodes_tooltip(bContext * /*C*/,
                                        PointerRNA * /*properties*/,
                                        const int /*mval*/[2])
{
  return {};
}

static wmOperatorStatus drop_geometry_nodes_invoke(bContext * /*C*/,
                                                   wmOperator * /*op*/,
                                                   const wmEvent * /*event*/)
{
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_drop_geometry_nodes(wmOperatorType *ot)
{
  ot->name = "Drop Geometry Node Group on Object";
  ot->idname = "OBJECT_OT_drop_geometry_nodes";

  ot->invoke = drop_geometry_nodes_invoke;
  ot->poll = ED_operator_view3d_active;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  PropertyRNA *prop = RNA_def_int(ot->srna,
                                  "session_uid",
                                  0,
                                  INT32_MIN,
                                  INT32_MAX,
                                  "Session UID",
                                  "Session UID of the geometry node group being dropped",
                                  INT32_MIN,
                                  INT32_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  RNA_def_boolean(ot->srna,
                  "show_datablock_in_modifier",
                  true,
                  "Show the data-block selector in the modifier",
                  "");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Unlink Object Operator
 * \{ */

static wmOperatorStatus object_unlink_data_exec(bContext *C, wmOperator *op)
{
  ID *id;
  PropertyPointerRNA pprop;

  ui::context_active_but_prop_get_templateID(C, &pprop.ptr, &pprop.prop);

  if (pprop.prop == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Incorrect context for running object data unlink");
    return OPERATOR_CANCELLED;
  }

  id = pprop.ptr.owner_id;

  if (GS(id->name) == ID_OB) {
    Object *ob = id_cast<Object *>(id);
    if (ob->data) {
      ID *id_data = ob->data;

      if (GS(id_data->name) == ID_IM) {
        id_us_min(id_data);
        ob->data = nullptr;
      }
      else {
        BKE_report(op->reports, RPT_ERROR, "Cannot unlink this object data");
        return OPERATOR_CANCELLED;
      }
    }
  }

  RNA_property_update(C, &pprop.ptr, pprop.prop);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_unlink_data(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unlink";
  ot->idname = "OBJECT_OT_unlink_data";

  /* API callbacks. */
  ot->exec = object_unlink_data_exec;

  /* flags */
  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

}  // namespace blender::ed::object
