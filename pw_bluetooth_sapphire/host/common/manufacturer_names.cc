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

#include "pw_bluetooth_sapphire/internal/host/common/manufacturer_names.h"

#include <string_view>

namespace bt {

// This file used to contain a list of string identifiers for converting a
// manufacturer ID to a human readable string identifying the manufacturer.
// This is being refactored and the function contained here will likely be
// removed soon.
//
// The manufacturer's identifiers can be found in the Bluetooth SIG Assigned
// Numbers document, see
// https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers.
//
// TODO(https://fxbug.dev/321947674) - Remove or rename.
std::string GetManufacturerName(uint16_t manufacturer_id) {
  char buffer[std::string_view("0x0000").size() + 1];
  snprintf(buffer, sizeof(buffer), "0x%04x", manufacturer_id);
  return std::string(buffer);
}

}  // namespace bt
