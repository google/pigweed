// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_RANDOM_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_RANDOM_H_

#include <zircon/syscalls.h>

#include "pw_random/random.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"

namespace bt {

// Returns the global random number generator. This returns nullptr until it is
// configured by initialization code.
pw::random::RandomGenerator* random_generator();

// Sets the global random number generator used by the host stack. To prevent overriding, the
// current generator must be nullptr or an assert will fail.
void set_random_generator(pw::random::RandomGenerator* generator);

template <typename T>
T Random() {
  static_assert(std::is_trivial_v<T> && !std::is_pointer_v<T>,
                "Type cannot be filled with random bytes");
  BT_DEBUG_ASSERT(random_generator());
  T t;
  random_generator()->Get({reinterpret_cast<std::byte*>(&t), sizeof(T)});
  return t;
}

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_RANDOM_H_
