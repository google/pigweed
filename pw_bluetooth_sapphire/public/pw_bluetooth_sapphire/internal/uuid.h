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

#include "pw_bluetooth/uuid.h"
#include "pw_bluetooth_sapphire/internal/host/common/uuid.h"
#include "pw_span/span.h"

namespace pw::bluetooth_sapphire::internal {

inline bt::UUID UuidFrom(const pw::bluetooth::Uuid& uuid) {
  return bt::UUID(bt::BufferView(pw::as_bytes(uuid.As128BitSpan())));
}

}  // namespace pw::bluetooth_sapphire::internal
