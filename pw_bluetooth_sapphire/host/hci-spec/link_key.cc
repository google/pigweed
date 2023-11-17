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

#include "pw_bluetooth_sapphire/internal/host/hci-spec/link_key.h"

namespace bt::hci_spec {

LinkKey::LinkKey() : rand_(0), ediv_(0) { value_.fill(0); }

LinkKey::LinkKey(const UInt128& ltk, uint64_t rand, uint16_t ediv)
    : value_(ltk), rand_(rand), ediv_(ediv) {}

}  // namespace bt::hci_spec
