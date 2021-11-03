// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_PERSISTED_DATA_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_PERSISTED_DATA_H_

#include <lib/fit/function.h>

#include <optional>

#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt_defs.h"

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
