/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include "ED_object.hh"

#include "object_intern.hh"

namespace blender::ed::object {

void object_modifier_add_asset_register() {}

void ui_template_modifier_asset_menu_items(const bContext & /*C*/,
                                           ui::Layout & /*layout*/,
                                           const StringRef /*catalog_path*/,
                                           const bool /*skip_essentials*/)
{
}

}  // namespace blender::ed::object
