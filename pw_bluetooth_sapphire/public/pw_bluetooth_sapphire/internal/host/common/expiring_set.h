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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_EXPIRING_SET_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_EXPIRING_SET_H_

#include <pw_async/dispatcher.h>

#include <unordered_map>

namespace bt {

// A set which only holds items until the expiry time given.
template <typename Key>
class ExpiringSet {
 public:
  virtual ~ExpiringSet() = default;
  explicit ExpiringSet(pw::async::Dispatcher& pw_dispatcher)
      : pw_dispatcher_(pw_dispatcher) {}

  // Add an item with the key `k` to the set, until the `expiration` passes.
  // If the key is already in the set, the expiration is updated, even if it
  // changes the expiration.
  void add_until(Key k, pw::chrono::SystemClock::time_point expiration) {
    elems_[k] = expiration;
  }

  // Remove an item from the set. Idempotent.
  void remove(const Key& k) { elems_.erase(k); }

  // Check if a key is in the map.
  // Expired keys are removed when they are checked.
  bool contains(const Key& k) {
    auto it = elems_.find(k);
    if (it == elems_.end()) {
      return false;
    }
    if (it->second <= pw_dispatcher_.now()) {
      elems_.erase(it);
      return false;
    }
    return true;
  }

 private:
  std::unordered_map<Key, pw::chrono::SystemClock::time_point> elems_;
  pw::async::Dispatcher& pw_dispatcher_;
};

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_EXPIRING_SET_H_
