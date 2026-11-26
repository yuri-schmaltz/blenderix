/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstddef>
#include <optional>

#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BKE_paint_types.hh"
#include "DNA_action_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "ED_transverts.hh"

namespace blender::nodes {
struct BundleKey {
  static bool is_valid_key(StringRef key);
};
bool BundleKey::is_valid_key(StringRef /*key*/)
{
  return true;
}
}  // namespace blender::nodes

namespace blender {
struct AnimData;
struct bSound;
struct Depsgraph;
struct ID;
struct Main;
struct Object;
struct Scene;

void BKE_sound_evaluate(Depsgraph * /*depsgraph*/, Main * /*bmain*/, bSound * /*sound*/)
{
}

bool id_can_have_animdata(const ID * /*id*/)
{
  return false;
}

bool id_type_can_have_animdata(const short /*id_type*/)
{
  return false;
}

AnimData *BKE_animdata_from_id(const ID * /*id*/)
{
  return nullptr;
}

bool BKE_animdata_free(ID * /*id*/, bool /*do_id_user*/)
{
  return false;
}

bool BKE_animdata_copy_id(Main * /*bmain*/, ID * /*id_to*/, ID * /*id_from*/, int /*flag*/)
{
  return false;
}

void BKE_animdata_copy_id_action(Main * /*bmain*/, ID * /*id*/)
{
}

void BKE_animsys_update_driver_array(ID * /*id*/)
{
}

Material ***BKE_object_material_array_p(Object *ob)
{
  return ob ? &ob->mat : nullptr;
}

short *BKE_object_material_len_p(Object * /*ob*/)
{
  static short zero = 0;
  return &zero;
}

void BKE_object_materials_sync_length(Main * /*bmain*/, Object * /*ob*/, ID * /*id*/)
{
}

void BKE_objects_materials_sync_length_all(Main * /*bmain*/, ID * /*id*/)
{
}

void BKE_object_material_from_eval_data(Main * /*bmain*/, Object * /*ob_orig*/, const ID * /*data_eval*/)
{
}

Material *BKE_object_material_get(Object * /*ob*/, short /*act*/)
{
  return nullptr;
}

Material *BKE_object_material_get_eval(Object * /*ob*/, short /*act*/)
{
  return nullptr;
}

void BKE_id_material_eval_assign(ID * /*id*/, int /*slot*/, Material * /*material*/)
{
}

int BKE_id_material_used_with_fallback_eval(const ID & /*id*/)
{
  return 1;
}

int BKE_object_material_used_with_fallback_eval(const Object & /*ob*/)
{
  return 1;
}

int BKE_object_material_count_eval(const Object * /*ob*/)
{
  return 0;
}

void BKE_object_material_remap_calc(Object * /*ob_dst*/, Object *ob_src, short *remap_src_to_dst)
{
  if (remap_src_to_dst && ob_src) {
    for (int i = 0; i < ob_src->totcol; ++i) {
      remap_src_to_dst[i] = 0;
    }
  }
}

void BKE_object_material_array_assign(
    Main * /*bmain*/, Object * /*ob*/, Material *** /*matar*/, int /*totcol*/, bool /*to_object_only*/)
{
}

Material *BKE_material_default_empty()
{
  static Material default_mat;
  static const bool initialized = []() {
    default_mat.r = 0.8f;
    default_mat.g = 0.8f;
    default_mat.b = 0.8f;
    default_mat.a = 1.0f;
    default_mat.roughness = 0.4f;
    default_mat.metallic = 0.0f;
    return true;
  }();
  (void)initialized;
  return &default_mat;
}

void BKE_material_defaults_free_gpu()
{
}

struct View3D;
struct ARegion;
void ED_annotation_draw_view3d(Scene * /*scene*/,
                               Depsgraph * /*depsgraph*/,
                               View3D * /*v3d*/,
                               ARegion * /*region*/,
                               bool /*on_back*/)
{
}

struct bContext;
void ED_annotation_draw_view2d(const bContext * /*C*/, bool /*on_back*/)
{
}

struct Image;
struct ImageUser;
struct bNode;
struct bNodeTree;
bool ED_object_get_active_image(Object * /*ob*/,
                                int /*mat_nr*/,
                                Image **r_ima,
                                ImageUser **r_iuser,
                                const bNode **r_node,
                                const bNodeTree **r_ntree)
{
  if (r_ima) {
    *r_ima = nullptr;
  }
  if (r_iuser) {
    *r_iuser = nullptr;
  }
  if (r_node) {
    *r_node = nullptr;
  }
  if (r_ntree) {
    *r_ntree = nullptr;
  }
  return false;
}

bool BKE_ptcache_object_has(Scene * /*scene*/, Object * /*ob*/, int /*mode*/)
{
  return false;
}

void BKE_ptcache_object_reset(Scene * /*scene*/, Object * /*ob*/, int /*mode*/)
{
}

void BKE_sound_update_scene(struct Depsgraph * /*depsgraph*/, Scene * /*scene*/)
{
}

void BKE_sound_update_fps(Main * /*bmain*/, Scene * /*scene*/)
{
}

namespace seq {
struct Editing;
struct Strip;
Editing *editing_get(const Scene * /*scene*/)
{
  return nullptr;
}
Strip *select_active_get(const Scene * /*scene*/)
{
  return nullptr;
}
void sound_update_length(Main * /*bmain*/, Scene * /*scene*/)
{
}
}  // namespace seq

struct bContext;
struct AssetLibraryReference;
struct wmRegionListenerParams;
namespace ed::asset {
void pre_save_assets(Main * /*bmain*/)
{
}
uint64_t types_supported_as_filter_flags()
{
  return 0;
}
namespace list {
void storage_tag_main_data_dirty()
{
}
void clear(const AssetLibraryReference * /*library_reference*/, const bContext * /*C*/)
{
}
void clear_all_library(const bContext * /*C*/)
{
}
void asset_reading_region_listen_fn(const wmRegionListenerParams * /*params*/)
{
}
bool is_loaded(const AssetLibraryReference * /*library_reference*/)
{
  return false;
}
}  // namespace list
}  // namespace ed::asset

namespace asset_system {
bool is_or_contains_remote_libraries(const AssetLibraryReference & /*library_ref*/)
{
  return false;
}
void remote_library_cancel_all_asset_downloads(bContext & /*C*/)
{
}
void remote_library_cancel_all_listing_downloads(const bContext & /*C*/)
{
}
bool remote_library_has_unfinished_asset_downloads()
{
  return false;
}
std::optional<float> remote_library_total_asset_downloads_progress()
{
  return std::nullopt;
}
void all_library_reload_catalogs_if_dirty()
{
}
void all_library_tag_catalogs_dirty()
{
}
}  // namespace asset_system

namespace ui {
class Layout;
enum class NodeAssetMenuOperatorType : int8_t;
}  // namespace ui
namespace ed::geometry {
/* The geometry nodes editor is not part of this build, so the asset menus
 * that it used to provide have nothing to draw. */
void ui_template_node_operator_asset_root_items(ui::Layout & /*layout*/,
                                                const bContext & /*C*/)
{
}
void ui_template_node_operator_asset_menu_items(ui::Layout & /*layout*/,
                                                const bContext & /*C*/,
                                                StringRef /*catalog_path*/)
{
}
void ui_template_node_operator_registration_errors(ui::Layout & /*layout*/,
                                                   StringRefNull /*message*/)
{
}
}  // namespace ed::geometry
namespace ed::space_node {
void ui_template_node_asset_menu_items(ui::Layout & /*layout*/,
                                       const bContext & /*C*/,
                                       StringRef /*catalog_path*/,
                                       const ui::NodeAssetMenuOperatorType /*operator_type*/)
{
}
}  // namespace ed::space_node

void BKE_sound_invalidate_scene_audio_animation_cache(Scene * /*scene*/)
{
}

struct MultiresModifierData;
MultiresModifierData *BKE_sculpt_multires_active(const Scene * /*scene*/, Object * /*ob*/)
{
  return nullptr;
}

bool BKE_paint_select_face_test(const Object * /*ob*/)
{
  return false;
}

void BKE_sculptsession_update_before_eval(Object * /*ob*/)
{
}

void BKE_sculptsession_free(Object * /*ob*/)
{
}

void BKE_animdata_duplicate_id_action(Main * /*bmain*/, ID * /*id*/, const uint32_t /*dup_flag*/)
{
}

struct BlendWriter;
void BKE_animdata_blend_write(BlendWriter * /*writer*/, ID * /*id*/)
{
}

struct KeyingSet;
struct PointerRNA;
struct PropertyRNA;
namespace animrig {
KeyingSet *scene_get_active_keyingset(const Scene * /*scene*/)
{
  return nullptr;
}
bool id_frame_has_keyframe(ID * /*id*/, float /*frame*/)
{
  return false;
}
bool is_autokey_on(const Scene * /*scene*/)
{
  return false;
}
bool autokeyframe_cfra_can_key(const Scene * /*scene*/, ID * /*id*/)
{
  return false;
}
bool autokeyframe_property(bContext * /*C*/,
                           Scene * /*scene*/,
                           PointerRNA * /*ptr*/,
                           PropertyRNA * /*prop*/,
                           int /*rnaindex*/,
                           float /*cfra*/,
                           bool /*only_if_property_keyed*/)
{
  return false;
}
}  // namespace animrig

struct bArmature;
struct Bone;
bool ANIM_bone_in_visible_collection(const bArmature * /*arm*/, const Bone * /*bone*/)
{
  return false;
}

struct RenderEngineType;
RenderEngineType *RE_engines_find(const char * /*idname*/)
{
  return nullptr;
}

void BKE_paint_free(struct Paint * /*paint*/)
{
}

struct bToolRef;
struct bContext;
PaintMode BKE_paintmode_get_from_tool(const bToolRef * /*tref*/)
{
  return PaintMode::Invalid;
}

PaintMode BKE_paintmode_get_active_from_context(const bContext * /*C*/)
{
  return PaintMode::Invalid;
}

struct RegionView3D;
bool BKE_sculptsession_use_pbvh_draw(const Object * /*ob*/, const RegionView3D * /*rv3d*/)
{
  return false;
}

struct ViewLayer;
Paint *BKE_paint_get_active(const Main & /*bmain*/, Scene * /*scene*/, ViewLayer * /*view_layer*/)
{
  return nullptr;
}

Paint *BKE_paint_get_active_from_context(const bContext * /*C*/)
{
  return nullptr;
}

struct Brush;
const Brush *BKE_paint_brush_for_read(const Paint * /*paint*/)
{
  return nullptr;
}

void BKE_rotMode_change_values(float /*quat*/[4],
                               float /*eul*/[3],
                               float /*axis*/[3],
                               float * /*angle*/,
                               eRotationModes /*oldMode*/,
                               eRotationModes /*newMode*/)
{
}

void BKE_curve_correct_bezpart(const float /*v1*/[2],
                               float /*v2*/[2],
                               float /*v3*/[2],
                               const float /*v4*/[2])
{
}

void BKE_curve_forward_diff_bezier(
    float q0, float q1, float q2, float q3, float *p, int it, int stride)
{
  if (it <= 0) {
    return;
  }
  const float inv = 1.0f / float(it);
  char *ptr = reinterpret_cast<char *>(p);
  for (int i = 0; i <= it; i++) {
    const float t = float(i) * inv;
    const float u = 1.0f - t;
    const float val = u * u * u * q0 + 3.0f * u * u * t * q1 + 3.0f * u * t * t * q2 +
                      t * t * t * q3;
    *reinterpret_cast<float *>(ptr) = val;
    ptr += stride;
  }
}

namespace bke::pbvh {
class Tree;
}
namespace bke::object {
pbvh::Tree *pbvh_get(Object & /*object*/)
{
  return nullptr;
}
const pbvh::Tree *pbvh_get(const Object & /*object*/)
{
  return nullptr;
}
}  // namespace bke::object

void BKE_paint_face_set_overlay_color_get(int /*face_set*/, int /*seed*/, unsigned char *r_color)
{
  if (r_color) {
    r_color[0] = 255;
    r_color[1] = 255;
    r_color[2] = 255;
    r_color[3] = 0;
  }
}

struct ToolSettings;
struct BMesh;
struct BMLoop;
struct BMFace;
struct BMUVOffsets;
bool uvedit_uv_select_test_ex(const ToolSettings * /*ts*/,
                              const BMesh * /*bm*/,
                              const BMLoop * /*l*/,
                              const BMUVOffsets & /*offsets*/)
{
  return false;
}

bool uvedit_edge_select_test_ex(const ToolSettings * /*ts*/,
                                const BMesh * /*bm*/,
                                const BMLoop * /*l*/,
                                const BMUVOffsets & /*offsets*/)
{
  return false;
}

bool uvedit_face_select_test_ex(const ToolSettings * /*ts*/,
                                const BMesh * /*bm*/,
                                const BMFace * /*efa*/)
{
  return false;
}

bool uvedit_uv_select_test(const Scene * /*scene*/,
                           const BMesh * /*bm*/,
                           const BMLoop * /*l*/,
                           const BMUVOffsets & /*offsets*/)
{
  return false;
}

bool uvedit_edge_select_test(const Scene * /*scene*/,
                             const BMesh * /*bm*/,
                             const BMLoop * /*l*/,
                             const BMUVOffsets & /*offsets*/)
{
  return false;
}

void ED_uvedit_live_unwrap_end(bool /*cancel*/)
{
}

void ED_uvedit_live_unwrap(const Scene * /*scene*/, Span<Object *> /*objects*/)
{
}

void ED_uvedit_sync_uvselect_ensure_if_needed(const ToolSettings * /*ts*/, BMesh * /*bm*/)
{
}

struct wmTimer;
bool ED_uvedit_live_unwrap_timer_check(const wmTimer * /*timer*/)
{
  return false;
}

struct RigidBodyWorld;
bool BKE_rigidbody_check_sim_running(RigidBodyWorld * /*rbw*/, float /*ctime*/)
{
  return false;
}

void BKE_sculptsession_free_pbvh(Object & /*object*/)
{
}

void ED_paint_proj_mesh_data_check(
    Scene & /*scene*/, Object & /*ob*/, bool *r_uvs, bool *r_mat, bool *r_tex, bool *r_stencil)
{
  if (r_uvs) {
    *r_uvs = false;
  }
  if (r_mat) {
    *r_mat = false;
  }
  if (r_tex) {
    *r_tex = false;
  }
  if (r_stencil) {
    *r_stencil = false;
  }
}

struct wmOperator;
struct UndoStep;
namespace ed::sculpt_paint::undo {
void geometry_begin(const Scene & /*scene*/, Object & /*object*/, const wmOperator * /*op*/)
{
}
void geometry_end(Object & /*object*/)
{
}
size_t step_memory_size_get(UndoStep * /*us*/)
{
  return 0;
}
}  // namespace ed::sculpt_paint::undo

struct wmWindowManager;
void ED_preview_kill_jobs_for_undo(wmWindowManager * /*wm*/, Main * /*bmain*/)
{
}

void ED_preview_restart_work(const bContext * /*C*/)
{
}

struct ScrArea;
bool ED_markers_region_visible(const ScrArea * /*area*/, const ARegion * /*region*/)
{
  return false;
}

struct Brush;
Brush *BKE_paint_brush(Paint * /*paint*/)
{
  return nullptr;
}

struct ViewLayer;
struct FreestyleLineStyle;
FreestyleLineStyle *BKE_linestyle_active_from_view_layer(ViewLayer * /*view_layer*/)
{
  return nullptr;
}

struct wmEvent;
Object *ED_pose_object_from_context(bContext * /*C*/)
{
  return nullptr;
}

struct bPoseChannel;
bPoseChannel *BKE_pose_channel_active(Object * /*ob*/, bool /*check_bonecoll*/)
{
  return nullptr;
}

bPoseChannel *BKE_pose_channel_active_if_bonecoll_visible(Object * /*ob*/)
{
  return nullptr;
}

struct bGPdata;
bGPdata *ED_annotation_data_get_active_direct(ID * /*id*/, ScrArea * /*area*/, Scene * /*scene*/)
{
  return nullptr;
}

bGPdata **ED_annotation_data_get_pointers_direct(
    ID * /*id*/, ScrArea * /*area*/, Scene * /*scene*/, PointerRNA * /*r_ptr*/)
{
  return nullptr;
}

struct wmWindow;
bool ED_time_scrub_event_in_region_poll(const wmWindow * /*win*/,
                                        const ScrArea * /*area*/,
                                        const ARegion * /*region*/,
                                        const wmEvent * /*event*/)
{
  return false;
}

struct FCurve;
struct bAction;
FCurve *BKE_fcurve_find_by_rna_context_ui(bContext * /*C*/,
                                          const PointerRNA * /*ptr*/,
                                          PropertyRNA * /*prop*/,
                                          int /*rnaindex*/,
                                          AnimData **r_animdata,
                                          bAction **r_action,
                                          bool *r_driven,
                                          bool *r_special)
{
  if (r_animdata) {
    *r_animdata = nullptr;
  }
  if (r_action) {
    *r_action = nullptr;
  }
  if (r_driven) {
    *r_driven = false;
  }
  if (r_special) {
    *r_special = false;
  }
  return nullptr;
}
void ED_render_view3d_update(Depsgraph * /*depsgraph*/,
                             wmWindow * /*win*/,
                             ScrArea * /*area*/,
                             bool /*updated*/)
{
}
void DRW_engine_external_free(RegionView3D * /*rv3d*/)
{
}
void BKE_rigidbody_ensure_local_object(Main * /*bmain*/, Object * /*ob*/)
{
}
bool BKE_paint_select_elem_test(const Object * /*ob*/)
{
  return false;
}
bool BKE_paint_select_vert_test(const Object * /*ob*/)
{
  return false;
}
bool BKE_paint_select_grease_pencil_test(const Object * /*ob*/)
{
  return false;
}
}  // namespace blender
