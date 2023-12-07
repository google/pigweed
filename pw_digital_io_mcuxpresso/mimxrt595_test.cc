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

#include <cstdint>

#include "pw_digital_io/digital_io.h"
#include "pw_digital_io_mcuxpresso/digital_io.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace pw::digital_io {
namespace {

constexpr uint32_t kPort = 0;
constexpr uint32_t kPin = 8;

TEST(DigitalIoTest, SetOk) {
  McuxpressoDigitalOut out(GPIO, kPort, kPin, pw::digital_io::State::kActive);
  EXPECT_TRUE(out.SetState(pw::digital_io::State::kInactive).ok());
}

TEST(DigitalIoTest, GetOk) {
  McuxpressoDigitalIn in(GPIO, kPort, kPin);
  EXPECT_TRUE(in.GetState().ok());
}

}  // namespace
}  // namespace pw::digital_io