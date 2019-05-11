// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "identifier.h"

#include "random.h"

namespace bt {
namespace common {

PeerId RandomPeerId() {
  PeerId id = kInvalidPeerId;
  while (id == kInvalidPeerId) {
    // TODO(BT-748): zx_cprng_draw() current guarantees this random ID to be
    // unique and that there will be no collisions. Re-consider where this
    // address is generated and whether we need to provide unique-ness
    // guarantees beyond device scope.
    id = PeerId(Random<uint64_t>());
  }
  return id;
}

}  // namespace common
}  // namespace bt
