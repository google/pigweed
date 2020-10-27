// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "types.h"

namespace bt::gatt {

Service::Service(bool primary, const UUID& type) : primary_(primary), type_(type) {}

Characteristic::Characteristic(IdType id, const UUID& type, uint8_t properties,
                               uint16_t extended_properties,
                               const att::AccessRequirements& read_permissions,
                               const att::AccessRequirements& write_permissions,
                               const att::AccessRequirements& update_permissions)
    : id_(id),
      type_(type),
      properties_(properties),
      extended_properties_(extended_properties),
      read_permissions_(read_permissions),
      write_permissions_(write_permissions),
      update_permissions_(update_permissions) {}

Descriptor::Descriptor(IdType id, const UUID& type, const att::AccessRequirements& read_permissions,
                       const att::AccessRequirements& write_permissions)
    : id_(id),
      type_(type),
      read_permissions_(read_permissions),
      write_permissions_(write_permissions) {}

}  // namespace bt::gatt
