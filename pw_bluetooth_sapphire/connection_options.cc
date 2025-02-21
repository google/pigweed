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

#include "pw_bluetooth_sapphire/internal/connection_options.h"

#include "pw_bluetooth_sapphire/internal/uuid.h"

namespace pw::bluetooth_sapphire::internal {

bt::gap::LowEnergyConnectionOptions ConnectionOptionsFrom(
    bluetooth::low_energy::Connection2::ConnectionOptions options) {
  bt::gap::LowEnergyConnectionOptions out;
  out.bondable_mode = options.bondable_mode ? bt::sm::BondableMode::Bondable
                                            : bt::sm::BondableMode::NonBondable;
  if (options.service_filter) {
    out.service_uuid = internal::UuidFrom(options.service_filter.value());
  }
  // TODO: https://pwbug.dev/396449684 - Use options.parameters and
  // options.att_mtu when supported by Sapphire.
  return out;
}

}  // namespace pw::bluetooth_sapphire::internal
