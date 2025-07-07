// Copyright 2025 The Pigweed Authors
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

#include <cstddef>

#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_bluetooth_proxy/comms2/embossed_multibuf.h"

namespace pw::bluetooth::proxy {

/// An H4 packet.
class H4Packet : public EmbossedMultiBuf {
 public:
  using Type = emboss::H4PacketType;

  constexpr static size_t kLayer = 1;

  explicit H4Packet(Allocator& allocator) : EmbossedMultiBuf(allocator) {}

  /// Returns ``true`` if an ``H4Packet`` can be created from the data
  // in ``buffer``.
  static bool CanConstructFrom(const MultiBuf& buffer);

  Status Prepare(MultiBuf& buffer);

  /// Moves data from ``buffer`` into an empty ``H4Packet`` to initialize it.
  /// Crashes if this ``H4Packet`` is not empty.
  void PopulateFrom(MultiBuf&& buffer) {
    PW_ASSERT(empty());
    Assign(std::move(buffer));
  }

  size_t size() const { return multibuf().size(); }

  Type type() const { return type_; }
  Status SetType(Type type);

 private:
  // H4 packets should contain a type and at least one byte of data.
  static constexpr size_t kMinSize = sizeof(Type) + 1;

  void Assign(MultiBuf&& buffer);

  Type type_ = Type::UNKNOWN;
};

}  // namespace pw::bluetooth::proxy
