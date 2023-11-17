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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_PERSISTED_DATA_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_PERSISTED_DATA_H_

#include <lib/fit/function.h>

#include <optional>

#include "pw_bluetooth_sapphire/internal/host/gatt/gatt_defs.h"

namespace bt::gatt {

struct ServiceChangedCCCPersistedData final {
  bool notify;
  bool indicate;

  bool operator==(const ServiceChangedCCCPersistedData& that) const {
    return indicate == that.indicate && notify == that.notify;
  }
};

using PersistServiceChangedCCCCallback =
    fit::function<void(PeerId, ServiceChangedCCCPersistedData)>;

using RetrieveServiceChangedCCCCallback =
    fit::function<std::optional<ServiceChangedCCCPersistedData>(PeerId)>;

}  // namespace bt::gatt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_PERSISTED_DATA_H_
