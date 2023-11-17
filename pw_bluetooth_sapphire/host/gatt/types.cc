// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_sapphire/internal/host/gatt/types.h"

namespace bt::gatt {

Service::Service(bool primary, const UUID& type)
    : primary_(primary), type_(type) {}

Characteristic::Characteristic(
    IdType id,
    const UUID& type,
    uint8_t properties,
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

Descriptor::Descriptor(IdType id,
                       const UUID& type,
                       const att::AccessRequirements& read_permissions,
                       const att::AccessRequirements& write_permissions)
    : id_(id),
      type_(type),
      read_permissions_(read_permissions),
      write_permissions_(write_permissions) {}

}  // namespace bt::gatt
