// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT2_SERVER_IDS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT2_SERVER_IDS_H_

#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"

namespace bthost {
// Separate types to help prevent mixing up the two types of service IDs used in gatt2/Server.
class ClientServiceId : public bt::Identifier<uint64_t> {
 public:
  constexpr explicit ClientServiceId(uint64_t value) : Identifier<uint64_t>(value) {}
  constexpr ClientServiceId() : ClientServiceId(0u) {}
};

class InternalServiceId : public bt::Identifier<uint64_t> {
 public:
  constexpr explicit InternalServiceId(uint64_t value) : Identifier<uint64_t>(value) {}
  constexpr InternalServiceId() : InternalServiceId(0u) {}
};
}  // namespace bthost

namespace std {
template <>
struct hash<bthost::ClientServiceId> {
  size_t operator()(const bthost::ClientServiceId& id) const {
    return std::hash<decltype(id.value())>()(id.value());
  }
};

template <>
struct hash<bthost::InternalServiceId> {
  size_t operator()(const bthost::InternalServiceId& id) const {
    return std::hash<decltype(id.value())>()(id.value());
  }
};
}  // namespace std

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT2_SERVER_IDS_H_
