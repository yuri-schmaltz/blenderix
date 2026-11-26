/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_runtime_backup_scene.h"

#include "BKE_scene_runtime.hh"
#include "BKE_sound.hh"

#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

namespace blender::deg {

SceneBackup::SceneBackup(const Depsgraph *depsgraph) : sequencer_backup(depsgraph)
{
  reset();
}

void SceneBackup::reset()
{
  audio_runtime = {};
  rigidbody_last_time = -1;
}

void SceneBackup::init_from_scene(Scene *scene)
{
  audio_runtime = std::move(scene->runtime->audio);
  scene->runtime->audio = {};
}

void SceneBackup::restore_to_scene(Scene *scene)
{
  scene->runtime->audio = std::move(audio_runtime);

  reset();
}

}  // namespace blender::deg
