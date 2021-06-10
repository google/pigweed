// Copyright 2021 The Pigweed Authors
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
#include "pw_i2c/device.h"

#include "gtest/gtest.h"
#include "pw_bytes/byte_builder.h"

namespace pw {
namespace i2c {
namespace {

// Fake test initiator that's used for testing.
class TestInitiator : public Initiator {
 public:
  explicit TestInitiator() {}

 private:
  Status DoWriteReadFor(Address,
                        ConstByteSpan,
                        ByteSpan,
                        chrono::SystemClock::duration) override {
    // Empty implementation.
    return OkStatus();
  }

  ByteBuffer<10> write_buffer_;
  ByteBuffer<10> read_buffer_;
};

// This test just checks to make sure the Device object compiles.
// TODO(b/185609270): Full test coverage.
TEST(DeviceCompilationTest, CompileOk) {
  constexpr Address kTestDeviceAddress = Address::SevenBit<0x3F>();

  TestInitiator initiator;
  Device device = Device(initiator, kTestDeviceAddress);
}

}  // namespace
}  // namespace i2c
}  // namespace pw
