/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * The compositor is no longer supported. Only a small part of its API is kept, because the shared
 * node-operation framework (see `source/blender/compositor/`) still relies on it.
 */

#pragma once

#include "BKE_compute_context_cache_fwd.hh"
#include "BKE_node_tree_zones.hh"

namespace blender {

struct Scene;
struct bNode;

class ComputeContext;

namespace bke {
class bNodeTreeZone;
}

namespace bke::compositor {

enum class ExecutionMode {
  Render,
  Preview,
};

/** The compositor is no longer supported, this always returns false. */
bool is_enabled(const Scene &scene, ExecutionMode mode);

/**
 * Return the compute context for the given node, taking the zones that have to be entered into
 * account. Returns null if the node is not in a supported zone.
 */
const ComputeContext *get_zone_viewer_compute_context(
    const bNode &node,
    const bke::bNodeTreeZone *zone,
    const ComputeContext &compute_context,
    bke::ComputeContextCache &compute_context_cache);

}  // namespace bke::compositor
}  // namespace blender
