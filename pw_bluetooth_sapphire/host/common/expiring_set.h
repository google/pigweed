// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_EXPIRING_SET_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_EXPIRING_SET_H_

#include <lib/async/cpp/time.h>
#include <lib/async/default.h>

#include <unordered_map>

#include <pw_async/dispatcher.h>
#include <pw_async_fuchsia/util.h>

namespace bt {

// A set which only holds items until the expiry time given.
template <typename Key>
class ExpiringSet {
 public:
  virtual ~ExpiringSet() = default;
  explicit ExpiringSet(pw::async::Dispatcher& pw_dispatcher) : pw_dispatcher_(pw_dispatcher) {}

  // Add an item with the key `k` to the set, until the `expiration` passes.
  // If the key is already in the set, the expiration is updated, even if it changes the expiration.
  void add_until(Key k, pw::chrono::SystemClock::time_point expiration) { elems_[k] = expiration; }

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
