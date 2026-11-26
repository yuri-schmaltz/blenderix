/* SPDX-FileCopyrightText: Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 *
 * The soft-body simulation is no longer supported. Only the small part of its API that is still
 * referenced by the code that handles the (now unused) soft-body data is kept here.
 */

namespace blender {

struct Object;
struct SoftBody;

struct BodyPoint {
  float origS[3], origE[3], origT[3], pos[3], vec[3], force[3];
  float goal;
  float prevpos[3], prevvec[3], prevdx[3], prevdv[3]; /* used by Heun integration */
  float impdv[3], impdx[3];
  int nofsprings;
  int *springs;
  float choke, choke2, frozen;
  float colball;
  short loc_flag; /* reserved by locale module specific states */
  float mass;
  float springweight;
};

/**
 * Frees internal data and soft-body itself.
 */
extern void sbFree(struct Object *ob);

/**
 * Copy soft-body data.
 */
extern SoftBody *sbCopy(SoftBody *sb, int flag);

/**
 * Frees simulation data to reset simulation.
 */
extern void sbFreeSimulation(SoftBody *sb);

}  // namespace blender
