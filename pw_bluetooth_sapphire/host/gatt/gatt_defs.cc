// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt_defs.h"

namespace bt {
namespace gatt {

ServiceData::ServiceData(att::Handle start, att::Handle end, const UUID& type)
    : range_start(start), range_end(end), type(type) {}

CharacteristicData::CharacteristicData(Properties props,
                                       std::optional<ExtendedProperties> ext_props,
                                       att::Handle handle, att::Handle value_handle,
                                       const UUID& type)
    : properties(props),
      extended_properties(ext_props),
      handle(handle),
      value_handle(value_handle),
      type(type) {}

DescriptorData::DescriptorData(att::Handle handle, const UUID& type) : handle(handle), type(type) {}

}  // namespace gatt
}  // namespace bt
