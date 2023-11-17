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

#pragma once
#include <pw_bluetooth/hci_commands.emb.h>

#include <cstdint>

#include "pw_bluetooth_sapphire/internal/host/common/uint128.h"

namespace bt::hci_spec {

// Represents a key used to encrypt a link.
class LinkKey final {
 public:
  LinkKey();
  LinkKey(const UInt128& value, uint64_t rand, uint16_t ediv);

  // 128-bit BR/EDR link key, LE Long Term Key, or LE Short Term key.
  const UInt128& value() const { return value_; }

  // Encrypted diversifier and random values used to identify the LTK. These
  // values are set to 0 for the LE Legacy STK, LE Secure Connections LTK, and
  // BR/EDR Link Key.
  uint64_t rand() const { return rand_; }
  uint16_t ediv() const { return ediv_; }

  bool operator==(const LinkKey& other) const {
    return value() == other.value() && rand() == other.rand() &&
           ediv() == other.ediv();
  }

  auto view() { return pw::bluetooth::emboss::MakeLinkKeyView(&value_); }

 private:
  UInt128 value_;
  uint64_t rand_;
  uint16_t ediv_;
};

}  // namespace bt::hci_spec
