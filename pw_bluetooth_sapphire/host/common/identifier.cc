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

#include "pw_bluetooth_sapphire/internal/host/common/identifier.h"

#include "pw_bluetooth_sapphire/internal/host/common/random.h"

namespace bt {

PeerId RandomPeerId() {
  PeerId id = kInvalidPeerId;
  while (id == kInvalidPeerId) {
    // TODO(fxbug.dev/1341): zx_cprng_draw() current guarantees this random ID
    // to be unique and that there will be no collisions. Re-consider where this
    // address is generated and whether we need to provide unique-ness
    // guarantees beyond device scope.
    id = PeerId(Random<uint64_t>());
  }
  return id;
}

}  // namespace bt
