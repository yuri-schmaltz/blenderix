/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscript
 *
 * Operators to manage the Python scripts that make up the user interface.
 */

#include "BLI_listbase.hh"
#include "BKE_context.hh"
#include "BKE_report.hh"

#include "WM_api.hh"
#include "WM_types.hh"
#include "wm_event_system.hh"

#include "script_intern.hh" /* own include */

#ifdef WITH_PYTHON
#  include "BPY_extern_run.hh"
#endif

namespace blender {

#ifdef WITH_PYTHON
static bool script_test_modal_operators(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  for (wmWindow &win : wm->windows) {
    for (wmEventHandler &handler_base : win.runtime->modalhandlers) {
      if (handler_base.type == WM_HANDLER_TYPE_OP) {
        wmEventHandler_Op *handler = reinterpret_cast<wmEventHandler_Op *>(&handler_base);
        if (handler->op != nullptr) {
          wmOperatorType *ot = handler->op->type;
          if (ot->rna_ext.srna) {
            return true;
          }
        }
      }
    }
  }

  return false;
}
#endif

static wmOperatorStatus script_reload_exec(bContext *C, wmOperator *op)
{
#ifdef WITH_PYTHON

  /* clear running operators */
  if (script_test_modal_operators(C)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot reload with running modal operators");
    return OPERATOR_CANCELLED;
  }

  /* Postpone when called from Python so this can be called from an operator
   * that might be re-registered, crashing Blender when we try to read from the
   * freed operator type which, see #80694. */
  {
    const char *imports[] = {"bpy", nullptr};
    BPY_run_string_exec(C,
                        imports,
                        "def fn():\n"
                        "    bpy.utils.load_scripts(reload_scripts=True)\n"
                        "    return None\n"
                        "bpy.app.timers.register(fn)");
  }

  /* Note that #WM_script_tag_reload is called from `bpy.utils.load_scripts`,
   * any additional updates required by this operator should go there. */

  return OPERATOR_FINISHED;
#else
  UNUSED_VARS(C, op);
  return OPERATOR_CANCELLED;
#endif
}

void SCRIPT_OT_reload(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reload Scripts";
  ot->description = "Reload scripts";
  ot->idname = "SCRIPT_OT_reload";

  /* API callbacks. */
  ot->exec = script_reload_exec;
}

}  // namespace blender
