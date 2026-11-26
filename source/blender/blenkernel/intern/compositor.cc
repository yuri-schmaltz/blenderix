/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * The compositor itself is no longer supported. Only the small part of its API that is shared with
 * the node-operation framework remains here.
 */

#include "DNA_node_types.h"

#include "BLI_assert.hh"
#include "BLI_vector.hh"

#include "BKE_compositor.hh"
#include "BKE_compute_context_cache.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_zones.hh"


namespace blender::bke::compositor {

bool is_enabled(const Scene & /*scene*/, ExecutionMode /*mode*/)
{
  return false;
}

const ComputeContext *get_zone_viewer_compute_context(
    const bNode &node,
    const bke::bNodeTreeZone *zone,
    const ComputeContext &compute_context,
    bke::ComputeContextCache &compute_context_cache)
{
  const ComputeContext *current_context = &compute_context;
  const bke::bNodeTreeZones *zones = node.owner_tree().zones();
  if (!zones) {
    return current_context;
  }
  const bke::bNodeTreeZone *node_zone = zones->get_zone_by_node(node.identifier);
  Vector<const bke::bNodeTreeZone *> zone_stack = zones->get_zones_to_enter(zone, node_zone);

  for (const bke::bNodeTreeZone *current_zone : zone_stack) {
    const bNode &output_node = *current_zone->output_node();
    if (output_node.is_type("GeometryNodeRepeatOutput"_ustr)) {
      const auto *repeat_storage = static_cast<NodeGeometryRepeatOutput *>(output_node.storage);
      const int inspection_index = repeat_storage->inspection_index;
      current_context = &compute_context_cache.for_repeat_zone(
          current_context, *current_zone->output_node(), inspection_index);
    }
    else if (output_node.is_type("NodeClosureOutput"_ustr)) {
      /* Viewers inside closures are not supported and are ignored. */
      return nullptr;
    }
    else {
      BLI_assert_unreachable();
      return nullptr;
    }
  }

  return current_context;
}

}  // namespace blender::bke::compositor
