/* SPDX-FileCopyrightText: Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 *
 * The collision system is no longer supported. The collision modifier is a no-op, so only the
 * type declarations are kept here, for the code that still refers to collision data.
 */

#include "DNA_listBase.h"

namespace blender {

struct BVHTree;
struct Collection;
struct CollisionModifierData;
struct Depsgraph;
struct ListBase;
struct Object;

/* -------------------------------------------------------------------- */
/** \name Collision Modifier Data
 * \{ */

/**
 * Move Collision modifier object inter-frame with step = [0,1]
 *
 * \param step: is limited from 0 (frame start position) to 1 (frame end position).
 */
void collision_move_object(struct CollisionModifierData *collmd,
                           float step,
                           float prevstep,
                           bool moving_bvh);

void collision_get_collider_velocity(float vel_old[3],
                                     float vel_new[3],
                                     struct CollisionModifierData *collmd,
                                     struct CollPair *collpair);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Collision Relations
 * \{ */

struct CollisionRelation {
  struct CollisionRelation *next, *prev;
  struct Object *ob;
};

/**
 * Create list of collision relations in the collection or entire scene.
 * This is used by the depsgraph to build relations, as well as faster
 * lookup of colliders during evaluation.
 */
ListBaseT<CollisionRelation> *BKE_collision_relations_create(struct Depsgraph *depsgraph,
                                                             struct Collection *collection,
                                                             unsigned int modifier_type);
void BKE_collision_relations_free(ListBaseT<CollisionRelation> *relations);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Collision Cache
 * \{ */

struct ColliderCache {
  struct ColliderCache *next, *prev;
  struct Object *ob;
  struct CollisionModifierData *collmd;
};

/**
 * Create effective list of colliders from relations built beforehand.
 * Self will be excluded.
 */
ListBaseT<ColliderCache> *BKE_collider_cache_create(struct Depsgraph *depsgraph,
                                                    struct Object *self,
                                                    struct Collection *collection);
void BKE_collider_cache_free(ListBaseT<ColliderCache> **colliders);

/** \} */

}  // namespace blender
