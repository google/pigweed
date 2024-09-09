// Copyright 2024 The Pigweed Authors
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

#pragma once

#include "pw_bluetooth_sapphire/internal/host/common/identifier.h"

namespace bthost {
// Separate types to help prevent mixing up the two types of service IDs used in
// gatt2/Server.
class ClientServiceId : public bt::Identifier<uint64_t> {
 public:
  constexpr explicit ClientServiceId(uint64_t value)
      : Identifier<uint64_t>(value) {}
  constexpr ClientServiceId() : ClientServiceId(0u) {}
};

class InternalServiceId : public bt::Identifier<uint64_t> {
 public:
  constexpr explicit InternalServiceId(uint64_t value)
      : Identifier<uint64_t>(value) {}
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
