// Copyright 2024 The Pigweed Authors
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

#include "pw_bluetooth/controller2.h"
#include "pw_bluetooth/gatt/client2.h"
#include "pw_bluetooth/gatt/server2.h"
#include "pw_bluetooth/low_energy/central2.h"
#include "pw_bluetooth/low_energy/channel.h"
#include "pw_bluetooth/low_energy/peripheral2.h"
#include "pw_bluetooth/pairing_delegate2.h"
#include "pw_unit_test/framework.h"

namespace pw::bluetooth {
namespace {

// This empty test ensures that the included API compiles.
TEST(ApiTest, ApiCompiles) {}

}  // namespace
}  // namespace pw::bluetooth
