/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_cpp_type_make.hh"

#include "BKE_cpp_types.hh"
#include "BKE_geometry_set.hh"
#include "BKE_instances.hh"

#include "DNA_meshdata_types.h"

namespace blender {

struct Tex;
struct Image;
struct Material;
struct VFont;
struct Scene;
struct Text;
struct Mask;
struct bSound;

void BKE_cpp_types_init()
{
  static bool initialized = false;
  if (initialized) {
    return;
  }
  initialized = true;

  register_cpp_types();

  BLI_CPP_TYPE_REGISTER(bke::GeometrySet,
                        CPPTypeFlags::Printable | CPPTypeFlags::EqualityComparable);
  BLI_CPP_TYPE_REGISTER(bke::InstanceReference, CPPTypeFlags::None);

  BLI_CPP_TYPE_REGISTER(Object *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(Collection *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(Tex *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(Image *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(Material *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(VFont *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(Scene *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(Text *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(Mask *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(bSound *, CPPTypeFlags::BasicType);

  BLI_CPP_TYPE_REGISTER(MStringProperty, CPPTypeFlags::None);
}

}  // namespace blender
