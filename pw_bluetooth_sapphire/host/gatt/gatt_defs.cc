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

#include "pw_bluetooth_sapphire/internal/host/gatt/gatt_defs.h"

namespace bt::gatt {

ServiceData::ServiceData(ServiceKind kind_in,
                         att::Handle start,
                         att::Handle end,
                         const UUID& type_in)
    : kind(kind_in), range_start(start), range_end(end), type(type_in) {}

CharacteristicData::CharacteristicData(
    Properties props,
    std::optional<ExtendedProperties> ext_props,
    att::Handle handle_in,
    att::Handle value_handle_in,
    const UUID& type_in)
    : properties(props),
      extended_properties(ext_props),
      handle(handle_in),
      value_handle(value_handle_in),
      type(type_in) {}

DescriptorData::DescriptorData(att::Handle handle_in, const UUID& type_in)
    : handle(handle_in), type(type_in) {}

}  // namespace bt::gatt
