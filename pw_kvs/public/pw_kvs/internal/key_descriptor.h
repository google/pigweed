// Copyright 2020 The Pigweed Authors
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
#include <cstdint>
#include <string_view>

#include "pw_kvs/flash_memory.h"
#include "pw_kvs/internal/hash.h"

namespace pw::kvs::internal {

// Caches information about a key-value entry. Facilitates quickly finding
// entries without having to read flash.
class KeyDescriptor {
 public:
  enum State { kValid, kDeleted };

  constexpr KeyDescriptor(std::string_view key)
      : KeyDescriptor(key, 0, 0, kValid) {}

  uint32_t hash() const { return key_hash_; }
  uint32_t transaction_id() const { return transaction_id_; }
  uint32_t address() const { return address_; }
  State state() const { return state_; }

  // True if the KeyDesctiptor's transaction ID is newer than the specified ID.
  bool IsNewerThan(uint32_t other_transaction_id) const {
    // TODO: Consider handling rollover.
    return transaction_id() > other_transaction_id;
  }

  bool deleted() const { return state_ == kDeleted; }

 private:
  friend class Entry;

  constexpr KeyDescriptor(std::string_view key,
                          uint32_t version,
                          FlashPartition::Address address,
                          State initial_state)
      : key_hash_(Hash(key)),
        transaction_id_(version),
        address_(address),
        state_(initial_state) {}

  uint32_t key_hash_;
  uint32_t transaction_id_;
  FlashPartition::Address address_;

  // TODO: This information could be packed into the above fields to save RAM.
  State state_;
};

}  // namespace pw::kvs::internal
