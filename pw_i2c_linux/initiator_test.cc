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

#include "pw_i2c_linux/initiator.h"

#include <type_traits>

#include "gtest/gtest.h"
#include "pw_i2c/initiator.h"

namespace pw::i2c {
namespace {

// A bus path that doesn't exist. Linux only supports 255 minor numbers.
constexpr auto kBusPathMissing = "/dev/i2c-256";
// A bus path that points at something that is not an I2C bus.
constexpr auto kBusPathInvalid = "/dev/null";

TEST(LinuxInitiatorTest, TestOpenMissingFileFails) {
  EXPECT_EQ(LinuxInitiator::OpenI2cBus(kBusPathMissing).status(),
            Status::InvalidArgument());
}

TEST(LinuxInitiatorTest, TestOpenInvalidFileFails) {
  EXPECT_EQ(LinuxInitiator::OpenI2cBus(kBusPathInvalid).status(),
            Status::InvalidArgument());
}

// Check that LinuxInitiator implements Initiator.
static_assert(std::is_assignable_v<Initiator&, LinuxInitiator&>);

}  // namespace
}  // namespace pw::i2c