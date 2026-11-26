/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "BKE_sound_types.hh"

#include "BLI_bounds_types.hh"
#include "BLI_set.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

namespace blender {

struct Depsgraph;

namespace nodes::eval_log {
class NodesEvalLog;
}  // namespace nodes::eval_log

namespace bke {

/* Runtime data specific to the sequencer, e.g. when using scene strips. */
class SequencerRuntime {
 public:
  Depsgraph *depsgraph = nullptr;

  ~SequencerRuntime();
};

/* Audio runtime data. */
struct SoundAnimationUse;
struct SoundRecording;

enum class AudioAnimationWriteState {
  Normal,
  Bake,
};

enum class AudioAnimationCacheType {
  None,
  Automatic,
  Explicit,
};

struct SceneAudioRuntime {
  AUD_Sequence sound_scene;
  AUD_Handle playback_handle;
  AUD_Handle sound_scrub_handle;
  Set<AUD_SequenceEntry> speaker_handles;
  bool volume_update_needed = false;
  /* Set by the depsgraph when an input to this evaluated #Scene's animation cache changes. */
  bool animation_cache_dirty = true;
  /* Controls how evaluated audio updates write to Audaspace. */
  AudioAnimationWriteState animation_write_state = AudioAnimationWriteState::Normal;
  /* Automatic caches only contain directly evaluable animation. Explicit caches can also contain
   * dependency-driven values and can satisfy automatic requests while clean and compatible. */
  AudioAnimationCacheType animation_cache_type = AudioAnimationCacheType::None;
  /* Identity of the frame-change handler lists associated with the current audio cache state.
   * An empty value means that no frame-change handlers were present. */
  std::optional<uint64_t> animation_cache_frame_change_handlers_hash;
  /* Inclusive frame ranges currently stored in the evaluated Scene's audio cache, sorted and
   * separated by frames that are not stored. */
  Vector<Bounds<int>> cached_animation_frames;
  /* Audio state of rendered frames that isn't stored per frame, see
   * #BKE_sound_recording_ensure. Owned by the readers of the render's audio. */
  std::weak_ptr<SoundRecording> recording;
  /* Held by readers of the audio animation that don't lock the audio device, see
   * #BKE_sound_audio_animation_use. */
  std::weak_ptr<SoundAnimationUse> animation_use;
};

class SceneRuntime : NonCopyable, NonMovable {
 public:
  SequencerRuntime sequencer;
  SceneAudioRuntime audio;
};

}  // namespace bke
}  // namespace blender
