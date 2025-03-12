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

#include "pw_bluetooth_sapphire/internal/discovery_filter.h"

#include "pw_bluetooth/low_energy/central2.h"
#include "pw_bluetooth_sapphire/internal/uuid.h"

namespace pw::bluetooth_sapphire::internal {

bt::hci::DiscoveryFilter DiscoveryFilterFrom(
    const pw::bluetooth::low_energy::Central2::ScanFilter& in) {
  bt::hci::DiscoveryFilter out;
  if (in.service_uuid.has_value()) {
    bt::UUID uuid = internal::UuidFrom(in.service_uuid.value());
    out.set_service_uuids({std::move(uuid)});
  }
  if (in.service_data_uuid.has_value()) {
    bt::UUID uuid = internal::UuidFrom(in.service_data_uuid.value());
    out.set_service_data_uuids({std::move(uuid)});
  }
  if (in.manufacturer_id.has_value()) {
    out.set_manufacturer_code(in.manufacturer_id.value());
  }
  if (in.connectable.has_value()) {
    out.set_connectable(in.connectable.value());
  }
  if (in.name.has_value()) {
    out.set_name_substring(std::string(in.name.value()));
  }
  if (in.max_path_loss.has_value()) {
    out.set_pathloss(in.max_path_loss.value());
  }
  if (in.solicitation_uuid.has_value()) {
    bt::UUID uuid = internal::UuidFrom(in.solicitation_uuid.value());
    out.set_solicitation_uuids({std::move(uuid)});
  }
  return out;
}

}  // namespace pw::bluetooth_sapphire::internal
