// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "identifier.h"

#include "random.h"

namespace bt {

PeerId RandomPeerId() {
  PeerId id = kInvalidPeerId;
  while (id == kInvalidPeerId) {
    // TODO(fxbug.dev/1341): zx_cprng_draw() current guarantees this random ID to be
    // unique and that there will be no collisions. Re-consider where this
    // address is generated and whether we need to provide unique-ness
    // guarantees beyond device scope.
    id = PeerId(Random<uint64_t>());
  }
  return id;
}

}  // namespace bt
